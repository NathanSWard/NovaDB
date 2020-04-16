#ifndef NOVA_INDEX_MANAGER_HPP
#define NOVA_INDEX_MANAGER_HPP

#include <absl/container/btree_map.h>
#include <absl/container/flat_hash_map.h>

#include "detail.hpp"
#include "index.hpp"
#include "util/multi_string.hpp"
#include "util/non_null_ptr.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace nova {

namespace detail {

struct compound_index_map_compare {
    using is_transparent = void;

    bool operator()(std::string_view const sv, multi_string const& ms) const noexcept {
        DEBUG_ASSERT(ms.size() > 0);
        return sv < ms[0];
    }

    bool operator()(multi_string const& ms, std::string_view const sv) const noexcept {
        DEBUG_ASSERT(ms.size() > 0);
        return ms[0] < sv;
    }

    bool operator()(multi_string const& lhs, multi_string const& rhs) const noexcept {
        auto const min = std::min(lhs.size(), rhs.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (lhs[i] == rhs[i])
                continue;
            return lhs[i] < rhs[i];
        }
        return false;
    }

    template<class... Views, std::size_t I>
    bool tpl_cmp_impl(multi_string const& ms, std::tuple<Views...> const& tpl, std::size_t const min) const noexcept {
        if constexpr (I == sizeof...(Views))
            return false;
        else {
            if (I == min)
                return false;
            if (ms[I] == std::get<I>(tpl))
                return tpl_cmp_impl<I + 1>(ms, tpl, min);
            return ms[I] < std::get<I>(tpl);
        }
    }

    template<class... Views, std::size_t I>
    bool tpl_cmp_impl(std::tuple<Views...> const& tpl, multi_string const& ms, std::size_t const min) const noexcept {
        if constexpr (I == sizeof...(Views))
            return false;
        else {
            if (I == min)
                return false;
            if (std::get<I>(tpl) == ms[I])
                return tpl_cmp_impl<I + 1>(tpl, ms, min);
            return std::get<I>(tpl) < ms[I];
        }
    }

    template<class... Views>
    bool operator()(multi_string const& ms, std::tuple<Views...> const& tpl) const noexcept {
        return tpl_cmp_impl<0>(ms, tpl, std::min(ms.size(), std::tuple_size_v<decltype(tpl)>));
    }

    template<class... Views>
    bool operator()(std::tuple<Views...> const& tpl, multi_string const& ms) const noexcept {
        return tpl_cmp_impl<0>(tpl, ms, std::min(ms.size(), std::tuple_size_v<decltype(tpl)>));
    }
};

template<class Base, class Derived, class... Args>
class lazy_allocation {
    std::tuple<Args...> args_;
public:
    template<class... Args2>
    lazy_allocation(Args2&&... args)
        : args_(std::forward_as_tuple(std::forward<Args2>(args)...))
    {}

    operator std::unique_ptr<Base>() const {
        return std::apply([](auto&&... args){
            return std::make_unique<Derived>(std::forward<decltype(args)>(args)...);
        }, args_);
    }
};

template<class B, class D, class... Args>
lazy_allocation(Args...) -> lazy_allocation<B, D, Args...>;

} // namespace detail

template<class Index>
using single_field_index_map = absl::flat_hash_map<std::string, std::unique_ptr<Index>>;

template<class Index>
using compound_index_map = absl::btree_multimap<multi_string, std::unique_ptr<Index>, detail::compound_index_map_compare>;

enum class index_type : std::uint8_t {
    single_field_unique,
    single_field_multi,
    compound_unique,
    compound_multi,
};

class index_manager {
    single_field_index_map<single_field_unique_index_interface> single_field_unique_indices{};
    single_field_index_map<single_field_multi_index_interface> single_field_multi_indices{};
    compound_index_map<compound_unique_index_interface> compound_unique_indices_{};
    compound_index_map<compound_multi_index_interface> compound_multi_indices_{};
public:
    index_manager() = default;
    index_manager(index_manager&&) = default;
    index_manager& operator=(index_manager&&) = default;

    template<bool Unique, class Filter = detail::no_filter, class... Fields, 
             std::enable_if_t<std::conjunction_v<std::is_constructible<std::string, Fields>...>, int> = 0>
    decltype(auto) create_index(Fields&&... fields) {

        if constexpr (sizeof...(Fields) == 0) {
            static_assert(detail::always_false<Filter>::value, "index must reference at least 1 document field");
        }
        else if constexpr (sizeof...(Fields) == 1) { // single field index
            if constexpr (Unique) {
                using base_t = single_field_unique_index_interface;
                using derived_t = ordered_single_field_unique_index<Filter>;
                using opt_t = optional<derived_t&>;

                if (!single_field_multi_indices.contains(fields...)) {
                    if (auto const [it, b] = single_field_unique_indices.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                        return opt_t{*static_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
            else { // multi
                using base_t = single_field_multi_index_interface;
                using derived_t = ordered_single_field_multi_index<Filter>;
                using opt_t = optional<derived_t&>;

                if (!single_field_unique_indices.contains(fields...)) {
                    if (auto const [it, b] = single_field_multi_indices.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                        return opt_t{*static_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
        } 
        else { // compound index
            multi_string fields_string(std::forward<Fields>(fields)...);
            if constexpr (Unique) {
                using base_t = compound_unique_index_interface;
                using derived_t = ordered_compound_unique_index<sizeof...(Fields), Filter>;
                using opt_t = optional<derived_t&>;

                if (auto const [first, last] = compound_multi_indices_.equal_range(fields_string)
                    ; first == compound_multi_indices_.end() || std::none_of(first, last, [&fields_string](auto&& p){ return p.first == fields_string; })) {
                    auto const it = compound_unique_indices_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(std::move(fields_string)),
                        std::forward_as_tuple(std::unique_ptr<base_t>((base_t*) new derived_t())));
                    return opt_t{*reinterpret_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
            else { // multi
                using base_t = compound_multi_index_interface;
                using derived_t = ordered_compound_multi_index<sizeof...(Fields), Filter>;
                using opt_t = optional<derived_t&>;

                if (auto const [first, last] = compound_unique_indices_.equal_range(fields_string)
                    ; first == compound_unique_indices_.end() || std::none_of(first, last, [&fields_string](auto&& p){ return p.first == fields_string; })) {
                    auto const it = compound_multi_indices_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(std::move(fields_string)),
                        std::forward_as_tuple(std::unique_ptr<base_t>((base_t*) new derived_t())));
                    return opt_t{*reinterpret_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
        }
    }

    // remove a document from all indices held.
    // precondition: the document *must* have been previously inserted
    void remove_document(document const& doc) {
        for (auto&& [field, index] : single_field_unique_indices) {
            if (auto const value = doc.values().lookup(field); value) {
                DEBUG_ASSERT(index->contains_doc(std::addressof(doc)));
                index->erase(value.value());
            }
        }

        for (auto&& [field, index] : single_field_multi_indices) {
            if (auto const value = doc.values().lookup(field); value) {
                DEBUG_ASSERT(index->contains_doc(std::addressof(doc)));
                index->erase(value.value(), std::addressof(doc));
            }
        }

        for (auto&& [fields, index] : compound_unique_indices_) {
            if (doc.values().contains(fields.begin(), fields.end())) {
                DEBUG_ASSERT(index->contains_doc(std::addressof(doc)));
                std::vector<non_null_ptr<bson const>> values;
                values.reserve(fields.size());
                for (auto&& field : fields)
                    values.push_back(std::addressof(doc.values().lookup(field).value()));
                index->erase(values);
            }
        }

        for (auto&& [fields, index] : compound_multi_indices_) {
            if (doc.values().contains(fields.begin(), fields.end())) {
                DEBUG_ASSERT(index->contains_doc(std::addressof(doc)));
                std::vector<non_null_ptr<bson const>> values;
                values.reserve(fields.size());
                for (auto&& field : fields)
                    values.push_back(std::addressof(doc.values().lookup(field).value()));
                index->erase(values, std::addressof(doc));
            }
        }
    }

    void remove_document(non_null_ptr<document> const doc) {
        remove_document(*doc);
    }

    void register_document(document& doc) {
        auto register_single_field = [&doc](auto&& index_map) {
            for (auto&& [field, index] : index_map)
                if (auto const found = doc.values().lookup(field); found)
                    index->insert(found.value(), std::addressof(doc));
        };

        auto register_compound = [&doc](auto&& index_map) {
            for (auto&& [fields, index] : index_map) {
                if (doc.values().contains(fields.begin(), fields.end())) {
                    std::vector<non_null_ptr<bson const>> vals;
                    vals.reserve(index->field_count());
                    for (auto&& field : fields)
                        vals.push_back(std::addressof(doc.values().lookup(field).value()));
                    index->insert(vals, std::addressof(doc));
                }
            }
        };

        register_single_field(single_field_unique_indices);
        register_single_field(single_field_multi_indices);
        register_compound(compound_unique_indices_);
        register_compound(compound_multi_indices_);
    }

    void register_document(non_null_ptr<document> const doc) {
        register_document(*doc);
    }

    template<class It>
    void register_documents(It first, It const last) {
        std::for_each(first, last, [this](auto&& doc){ register_document(doc); });
    }
};

} // namespace nova

#endif // NOVA_INDEX_MANAGER_HPP