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
#include <iostream>
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
    single_field_index_map<single_field_unique_index_interface> single_field_unique_indices_{};
    single_field_index_map<single_field_multi_index_interface> single_field_multi_indices_{};
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
                using result_t = lookup_result<std::string, derived_t>;

                if (!single_field_multi_indices_.contains(fields...))
                    if (auto const [it, b] = single_field_unique_indices_.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                        return result_t{it->first, *static_cast<derived_t*>(it->second.get())}; 
                return result_t{};
            }
            else { // multi
                using base_t = single_field_multi_index_interface;
                using derived_t = ordered_single_field_multi_index<Filter>;
                using result_t = lookup_result<std::string, derived_t>;

                if (!single_field_unique_indices_.contains(fields...))
                    if (auto const [it, b] = single_field_multi_indices_.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                        return result_t{it->first, *static_cast<derived_t*>(it->second.get())}; 
                return result_t{};
            }
        } 
        else { // compound index
            multi_string fields_string(std::forward<Fields>(fields)...);
            if constexpr (Unique) {
                using base_t = compound_unique_index_interface;
                using derived_t = ordered_compound_unique_index<sizeof...(Fields), Filter>;
                using result_t = lookup_result<multi_string, derived_t>;

                if (auto const [first, last] = compound_multi_indices_.equal_range(fields_string)
                    ; first == compound_multi_indices_.end() || std::none_of(first, last, [&fields_string](auto&& p){ return p.first == fields_string; })) {

                    auto const it = compound_unique_indices_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(std::move(fields_string)),
                        std::forward_as_tuple(std::unique_ptr<base_t>((base_t*) new derived_t())));

                    return result_t{it->first, *reinterpret_cast<derived_t*>(it->second.get())};
                }
                return result_t{};
            }
            else { // multi
                using base_t = compound_multi_index_interface;
                using derived_t = ordered_compound_multi_index<sizeof...(Fields), Filter>;
                using result_t = lookup_result<multi_string, derived_t>;

                if (auto const [first, last] = compound_unique_indices_.equal_range(fields_string)
                    ; first == compound_unique_indices_.end() || std::none_of(first, last, [&fields_string](auto&& p){ return p.first == fields_string; })) {

                    auto const it = compound_multi_indices_.emplace(
                        std::piecewise_construct,
                        std::forward_as_tuple(std::move(fields_string)),
                        std::forward_as_tuple(std::unique_ptr<base_t>((base_t*) new derived_t())));

                    return result_t{it->first, *reinterpret_cast<derived_t*>(it->second.get())};
                }
                return result_t{};
            }
        }
    }

    // remove a document from all indices held.
    // precondition: the document *must* have been previously inserted
    void remove_document(document const& doc) {
        for (auto&& [field, index] : single_field_unique_indices_) {
            if (auto const value = doc.values().lookup(field); value) {
                DEBUG_ASSERT(index->contains_doc(std::addressof(doc)));
                index->erase(value.value());
            }
        }

        for (auto&& [field, index] : single_field_multi_indices_) {
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

        register_single_field(single_field_unique_indices_);
        register_single_field(single_field_multi_indices_);
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

    template<class... Fields>
    auto lookup(Fields const&... fields) {
        if constexpr(sizeof...(Fields) == 0) {
            static_assert(detail::always_false<Fields...>::value);
        }
        else if constexpr (sizeof...(Fields) == 1) {
            using result_t = optional<sf_index_cursor>;
            std::string_view const field{fields...};

            if (auto const found = single_field_unique_indices_.find(field); found != single_field_unique_indices_.end())
                return result_t{found->second->iterate()};
            if (auto const found = single_field_multi_indices_.find(field); found != single_field_multi_indices_.end())
                return result_t{found->second->iterate()};
            return result_t{};
        }
        else { // sizeof...(Fields) > 1
            using variant_t = std::variant<sf_index_cursor, cmp_index_cursor>;
            using result_t = optional<variant_t>;
            auto const sv_tpl = std::make_tuple(std::string_view{fields}...);
            
            // check compound indices first
            if (auto const found = compound_unique_indices_.find(sv_tpl); found != compound_unique_indices_.end())
                return result_t{variant_t{found->second->iterate()}};
            if (auto const found = compound_multi_indices_.find(sv_tpl); found != compound_multi_indices_.end())
                return result_t{variant_t{found->second->iterate()}};

            // check single field indices
            if (auto const found = lookup_fields_in_single_filed_indices(sv_tpl); found)
                return result_t{variant_t{std::move(found.value())}};

            return result_t{}; 
        }
    }

    void print_indices() const {
        auto print_single_field = [](auto&& index_map) {
            for (auto&& [field, map] : index_map) {
                std::cout << "    indexed field: \"" << field << "\"\n";
                std::cout << "{\n";
                for (auto&& [val, doc_ptr] : map->iterate()) 
                    std::cout << fmt::format("    {}, {},\n", val, doc_ptr->id());
                std::cout << "}\n";
            }
        };

        auto print_compound = [](auto&& index_map) {
            for (auto&& [fields, map] : index_map) {
                std::cout << "    indexed fields: ";
                std::cout << "[";
                auto first = true;
                for (auto&& f : fields) {
                    if (!first)
                        std::cout << ", ";
                    std::cout << fmt::format("\"{}\"", f);
                    first = false;
                }
                std::cout << "]\n";
                std::cout << "{\n";
                for (auto&& [vals, doc_ptr] : map->iterate())  {
                    std::cout << "    [";
                    first = true;
                    for (auto&& val : vals) {
                        if (!first)
                            std::cout << ", ";
                        std::cout << fmt::format("{}", val);
                        first = false;
                    }
                    std::cout << "], ";
                    std::cout << fmt::format("{},\n", doc_ptr->id());
                }
                std::cout << "}\n";
            }
        };

        print_single_field(single_field_unique_indices_);
        print_single_field(single_field_multi_indices_);
        print_compound(compound_unique_indices_);
        print_compound(compound_multi_indices_);
    }

private:
    template<std::size_t I, class... Fields>
    optional<sf_index_cursor> lookup_fields_in_single_filed_indices(std::tuple<Fields...> const& fields) {
        if constexpr (I == sizeof...(Fields))
            return {};
        else {
            if (auto const found = this->lookup(std::get<I>(fields)); found)
                return found;
            else
                return lookup_fields_in_single_filed_indices<I + 1>(fields);
        }
    }
};

} // namespace nova

#endif // NOVA_INDEX_MANAGER_HPP