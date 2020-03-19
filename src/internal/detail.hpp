#ifndef INTERNAL_DETAIL_H
#define INTERNAL_DETAIL_H

#include <memory>
#include <type_traits>

namespace nova::detail {

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