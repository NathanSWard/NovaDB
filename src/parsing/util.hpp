#ifndef PARSING_UTIL_HPP
#define PARSING_UTIL_HPP

#include <algorithm>
#include <cctype>
#include <string_view>

namespace nova {

constexpr bool str_eq(std::string_view const a, std::string_view const b) noexcept {
    if (a.size() != b.size())
        return false;
    for (auto ait = a.cbegin(), bit = b.cbegin(); ait != a.cend(); ++ait, ++bit)
        if (std::tolower(static_cast<unsigned char>(*ait)) != std::tolower(static_cast<unsigned char>(*bit)))
            return false;
    return true;
}

} // namespace nova

#endif // PARSING_UTIL_HPP