#ifndef NON_NULL_PTR_H
#define NON_NULL_PTR_H

#include <cstddef>

namespace nova {

template<class T>
class non_null_ptr {
    T* ptr_;
public:
    non_null_ptr(std::nullptr_t) = delete;

    constexpr non_null_ptr(T* ptr) noexcept : ptr_(ptr) {}
    constexpr non_null_ptr(non_null_ptr const&) noexcept = default;
    constexpr non_null_ptr(non_null_ptr&&) noexcept = default;

    constexpr non_null_ptr& operator=(std::nullptr_t) = delete;
    constexpr non_null_ptr& operator=(non_null_ptr const&) = default;
    constexpr non_null_ptr& operator=(non_null_ptr&&) = default;

    constexpr operator T*() const noexcept { return ptr_; }
    constexpr T& operator*() const noexcept { return *ptr_; }
};

} // namespace nova

#endif // NON_NULL_PTR_H