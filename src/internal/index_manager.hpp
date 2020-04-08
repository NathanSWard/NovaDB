#ifndef NOVA_INDEX_MANAGER_HPP
#define NOVA_INDEX_MANAGER_HPP

#include "detail.hpp"
#include "index.hpp"
#include "util/multi_string.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <string_view>
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
using single_field_index_map = std::unordered_map<std::string, std::unique_ptr<Index>>;

template<class Index>
using compound_index_map = std::map<multi_string, std::unique_ptr<Index>, detail::compound_index_map_compare>;

enum class index_type : std::uint8_t {
    single_field_unique,
    single_field_multi,
    compound_unique,
    compound_multi,
};

class index_manager {
    single_field_index_map<single_field_unique_index_interface> sfu_;
    single_field_index_map<single_field_multi_index_interface> sfm_;
    compound_index_map<compound_unique_index_interface> cu_;
    compound_index_map<compound_multi_index_interface> cm_;
public:
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

                if (auto const found = sfm_.find(fields...); found == sfm_.end()) {
                    if (auto const [it, b] = sfu_.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                            return opt_t{*static_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
            else { // multi
                using base_t = single_field_multi_index_interface;
                using derived_t = ordered_single_field_multi_index<Filter>;
                using opt_t = optional<derived_t&>;

                if (auto const found = sfu_.find(fields...); found == sfu_.end()) {
                    if (auto const [it, b] = sfm_.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                        return opt_t{*static_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
        } 
        else { // compound index
            if constexpr (Unique) {
                using base_t = compound_unique_index_interface;
                using derived_t = ordered_compound_unique_index<sizeof...(Fields), Filter>;
                using opt_t = optional<derived_t&>;

                if (auto const found = cm_.find(fields...); found == cm_.end()) {
                    if (auto const [it, b] = cu_.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                        return opt_t{*static_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
            else { // multi
                using base_t = compound_multi_index_interface;
                using derived_t = ordered_compound_multi_index<sizeof...(Fields), Filter>;
                using opt_t = optional<derived_t&>;

                if (auto const found = cu_.find(fields...); found == cu_.end()) {
                    if (auto const [it, b] = cm_.try_emplace(std::forward<Fields>(fields)..., detail::lazy_allocation<base_t, derived_t>{}); b)
                        return opt_t{*static_cast<derived_t*>(it->second.get())};
                }
                return opt_t{};
            }
        }
    }

    /*
    // CHAGE TO STRING_VIEW WHEN USING ABSEIL CONTAINERS!
    void insert_value(std::string const& field, bson const& val, document* const doc) {
        if (auto const found = sfu_.find(field); found != sfu_.end())
            found->second->insert(val, doc);
        if (auto const found = sfm_.find(field); found != sfm_.end())
            found->second->insert(val, doc);
    }

    template<class... Fields, class... Vals>
    void insert_values(multi_string&& fields, span<bson const> const vals, document* const doc) {
        if (auto [first, last] = cu_.equal_range(fields); first != cu_.end()) {
            while (first != last) {
                first->second->insert(vals, doc);
                ++first;
            }
        }
        if (auto [first, last] = cm_.equal_range(fields); first != cm_.end()) {
            while (first != last) {
                first->second->insert(vals, doc);
                ++first;
            }
        }
    }
    */
};

} // namespace nova

#endif // NOVA_INDEX_MANAGER_HPP