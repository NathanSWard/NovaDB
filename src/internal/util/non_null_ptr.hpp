#ifndef NOVA_NON_NULL_PTR_HPP
#define NOVA_NON_NULL_PTR_HPP

#include "../../debug.hpp"

#include <cstddef>

namespace nova {

template <class T>
class non_null_ptr {
public:
    template<typename U, std::enable_if_t<std::is_convertible_v<U, T*>, int> = 0>
    constexpr non_null_ptr(U&& u) noexcept
        : ptr_(std::forward<U>(u))
    {
        DEBUG_ASSERT(ptr_ != nullptr);
    }

    template<typename = std::enable_if_t<!std::is_same_v<std::nullptr_t, T>>>
    constexpr non_null_ptr(T const u) noexcept
        : ptr_(u)
    {
        DEBUG_ASSERT(ptr_ != nullptr);
    }

    template<typename U, std::enable_if_t<std::is_convertible_v<U, T>, int> = 0>
    constexpr non_null_ptr(non_null_ptr<U> const& other) : non_null_ptr(other.get()) {}

    constexpr non_null_ptr(non_null_ptr const& other) noexcept = default;
    constexpr non_null_ptr& operator=(non_null_ptr const& other) noexcept = default;

    constexpr auto get() const noexcept {
        DEBUG_ASSERT(ptr_ != nullptr);
        return ptr_;
    }

    constexpr operator T*() const noexcept { return get(); }
    constexpr auto operator->() const noexcept { return get(); }
    constexpr decltype(auto) operator*() const noexcept { return *get(); }

    non_null_ptr(std::nullptr_t) = delete;
    non_null_ptr& operator=(std::nullptr_t) = delete;

    non_null_ptr& operator++() = delete;
    non_null_ptr& operator--() = delete;
    non_null_ptr operator++(int) = delete;
    non_null_ptr operator--(int) = delete;
    non_null_ptr& operator+=(std::ptrdiff_t) = delete;
    non_null_ptr& operator-=(std::ptrdiff_t) = delete;
    void operator[](std::ptrdiff_t) const = delete;

private:
    T* ptr_;
};

} // namespace nova

#endif // NOVA_NON_NULL_PTR_HPP