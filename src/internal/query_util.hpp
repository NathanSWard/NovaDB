#ifndef NOVA_QUERY_UTIL_HPP
#define NOVA_QUERY_UTIL_HPP

#include "bson.hpp"
#include "detail.hpp"

namespace nova {

template<class Cmp, class T>
auto query_cmp(std::string_view const field, T&& value) {
    using get_t = std::conditional_t<detail::is_string_comparable_v<T>, std::string, T>;
    auto query = [t = std::forward<T>(value)](bson const& b) -> bool {
        if (auto const opt = b.template as<get_t>(); opt)
            return Cmp{}(*opt, t);
        return false;
    };
    return std::make_tuple(field, query);
}

template<class T>
auto is_equal_query(std::string_view const field, T&& value) {
    return query_cmp<std::equal_to<>>(field, std::forward<T>(value));
}

template<class T>
auto is_not_equal_query(std::string_view const field, T&& value) {
    return query_cmp<std::not_equal_to<>>(field, std::forward<T>(value));
}

template<class T>
auto is_less_query(std::string_view const field, T&& value) {
    return query_cmp<std::less<>>(field, std::forward<T>(value));
}

template<class T>
auto is_less_eq_query(std::string_view const field, T&& value) {
    return query_cmp<std::less_equal<>>(field, std::forward<T>(value));
}

template<class T>
auto is_greater_query(std::string_view const field, T&& value) {
    return query_cmp<std::greater<>>(field, std::forward<T>(value));
}

template<class T>
auto is_greater_eq_query(std::string_view const field, T&& value) {
    return query_cmp<std::greater_equal<>>(field, std::forward<T>(value));
}

} // namespace nova

#endif