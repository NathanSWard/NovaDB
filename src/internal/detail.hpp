#ifndef INTERNAL_DETAIL_H
#define INTERNAL_DETAIL_H

#include <algorithm>
#include <memory>
#include <type_traits>

namespace nova::detail {

template<typename It, typename T, typename Cmp = std::less<>>
constexpr auto lower_bound(It first, It const last, T const& val, Cmp cmp = {}) {
    auto len = std::distance(first, last);
    while (len > 0) {
        auto half = len >> 1;
        It middle = first;
        std::advance(middle, half);
        if (comp(*middle, val)) {
            first = middle;
            ++first;
            len = len - half - 1;
        }
        else
            len = half;
    }
    return first;
}

template<class It, class T, class Cmp>
auto binary_search(It first, It const last, T const& val, Cmp cmp) {
    first = nova::detail::lower_bound(first, last, val, cmp);
    return first == last ? last : first;
}

template<class It, class T>
auto binary_search(It first, It const last, T const& val) {
    first = nova::detail::lower_bound(first, last, val);
    return first == last ? last : first;
}

template<class T>
struct is_string_comparable {
    template<class U>
    static auto test(int) -> decltype(std::declval<std::string const&>() == std::declval<U const&>(), std::true_type{});
    template<class>
    static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(0))::value;
};

template<class T>
static constexpr bool is_string_comparable_v = is_string_comparable<T>::value;

template<class T>
struct always_false {
    static constexpr bool value = !std::is_same_v<T, T>;
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template<class T>
inline constexpr void ignore(T&&) noexcept {}

template<class T>
class recursive_wrapper {
    std::unique_ptr<T> ptr_;
public:
    template<class... Args, std::enable_if_t<!std::is_same_v<std::decay_t<T>, recursive_wrapper<T>>, int>>
    recursive_wrapper(Args&&... args) 
        : ptr_(std::make_unique<T>(std::forward<Args>(args)...)) 
    {
        static_assert(std::is_constructible_v<T, Args...>);
    }

    recursive_wrapper(recursive_wrapper&& other) = default;

    recursive_wrapper(recursive_wrapper const& other)
        : ptr_(std::make_unique<T>(*(other.ptr_))) {}

    recursive_wrapper& operator=(recursive_wrapper&& other) = default;

    recursive_wrapper& operator=(recursive_wrapper const& other) {
        ptr_ = std::make_unique<T>(*(other.ptr_));
    }

    bool operator==(recursive_wrapper const& other) const noexcept {
        return *ptr_ == *(other.ptr_);
    }

    operator T& () const noexcept { return *ptr_; }

    constexpr T const& get() const noexcept { return *ptr_; }
    constexpr T& get() noexcept { return *ptr_; }
};

template<class T, class U>
using check_recursive_t = std::conditional_t<std::is_same_v<T, U>, recursive_wrapper<U>, T>;

} // namespace nova::detail

#endif // INTERNAL_DETAIL_H