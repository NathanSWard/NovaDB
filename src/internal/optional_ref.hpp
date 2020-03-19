#ifndef OPTIONAL_REF_H
#define OPTIONAL_REF_H

#include "../debug.hpp"
#include <type_traits>

namespace nova {

template<class T>
class optional_ref {
    T* ptr_ = nullptr;
public:
    constexpr optional_ref() noexcept = default;
    constexpr optional_ref(T& t)
        : ptr_(std::addressof(t))
    {}

    constexpr explicit operator bool() const noexcept { return ptr_ != nullptr; }
    constexpr T& operator*() const noexcept { 
        DEBUG_ASSERT(*this);
        return *ptr_; 
    }
    constexpr T* operator->() const noexcept { 
        DEBUG_ASSERT(*this);
        return ptr_; 
    }
};

template<class T>
optional_ref(T&) -> optional_ref<T>;

} // namespace nova

#endif // OPTIONAL_REF_H