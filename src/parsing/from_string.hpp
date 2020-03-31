#ifndef FROM_STRING_HPP
#define FROM_STRING_HPP

#include "../internal/optional.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>

namespace nova {

namespace detail {

template<class, class...>
struct is_one_of;

template<class T>
struct is_one_of<T> {
    static constexpr bool value = false;
};

template<class T, class U, class... Rest>
struct is_one_of<T, U, Rest...> {
    static constexpr bool value = std::is_same_v<T, U> ? true : is_one_of<T, Rest...>::value;
};

template<class T, class... Ts>
static constexpr bool is_one_of_v = is_one_of<T, Ts...>::value;

} // namespace detail

template<class T, std::enable_if_t<std::is_same_v<T, std::string>, int> = 0>
std::string from_string(std::string_view const sv) {
    return std::string{sv};
} 

template<class T, std::enable_if_t<std::is_same_v<T, bool>, int> = 0>
constexpr optional<bool> from_string(std::string_view const sv) noexcept {

    constexpr std::string_view true_ = "true";
    constexpr std::string_view false_ = "false";
    
    auto str_eq = [](auto&& strA, auto&& strB) {
        return (strA.size() == strB.size()) && 
        std::equal(strA.begin(), strA.end(), strB.begin(), 
        [](unsigned char c1, unsigned char c2) {
            return std::tolower(c1) == std::tolower(c2);
        });
    };
    
    if (str_eq(sv, true_))
        return {true};
    else if (str_eq(sv, false_))
        return {false};
    else
        return {};
}

template<class T, std::enable_if_t<detail::is_one_of_v<T, std::uint32_t, std::uint64_t, std::int32_t, std::int64_t>, int> = 0>
constexpr optional<T> from_string(std::string_view const sv) noexcept {
    T t{};
    if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), t)
    ; ec == std::errc{})
        return {t};
    else
        return {};
}

template<class T, std::enable_if_t<detail::is_one_of_v<T, float, double>, int> = 0>
constexpr optional<T> from_string(std::string_view const sv) noexcept {
    char* end = nullptr;
    if constexpr (std::is_same_v<T, float>) {
        float const f = std::strtof(sv.data(), &end);
        if (end == sv.data() || errno == ERANGE)
            return {};
        return {f};
    }
    else { // double
        double const d = std::strtod(sv.data(), &end);
        if (end == sv.data() || errno == ERANGE)
            return {};
        return {d};
    }
}

} // namespace nova

#endif // FROM_STRING_HPP