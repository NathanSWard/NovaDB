#ifndef NOVA_INPLACE_FUNCTION_HPP
#define NOVA_INPLACE_FUNCTION_HPP

#include "../../debug.hpp"

#include <type_traits>
#include <utility>
#include <functional>

namespace nova {

namespace inplace_function_detail {

inline static constexpr std::size_t InplaceFunctionDefaultCapacity = 32;

template<class T> struct wrapper {
    using type = T;
};

struct bad_inplace_function_call : std::exception {
    constexpr char const* what() const noexcept final {
        return "bad_inplace_function_call";
    }
};

template<class R, class... Args> 
struct vtable {
    using storage_ptr_t = void*;

    using invoke_ptr_t = R(*)(storage_ptr_t, Args&&...);
    using process_ptr_t = void(*)(storage_ptr_t, storage_ptr_t);
    using destructor_ptr_t = void(*)(storage_ptr_t);

    invoke_ptr_t const invoke_ptr;
    process_ptr_t const copy_ptr;
    process_ptr_t const relocate_ptr;
    destructor_ptr_t const destructor_ptr;

    explicit constexpr vtable() noexcept 
        : invoke_ptr{ [](storage_ptr_t, Args&&...) -> R
            { DEBUG_THROW(bad_inplace_function_call{}); }
        }
        , copy_ptr{ [](storage_ptr_t, storage_ptr_t) -> void {} }
        , relocate_ptr{ [](storage_ptr_t, storage_ptr_t) -> void {} }
        , destructor_ptr{ [](storage_ptr_t) -> void {} }
    {}

    template<class C> 
    explicit constexpr vtable(wrapper<C>) noexcept 
        : invoke_ptr{ [](storage_ptr_t storage_ptr, Args&&... args) -> R
            { return (*static_cast<C*>(storage_ptr))(static_cast<Args&&>(args)...); }
        }
        , copy_ptr{ [](storage_ptr_t dst_ptr, storage_ptr_t src_ptr)
            { ::new (dst_ptr) C{ (*static_cast<C*>(src_ptr)) }; }
        }
        , relocate_ptr{ [](storage_ptr_t dst_ptr, storage_ptr_t src_ptr) {
                ::new (dst_ptr) C{ std::move(*static_cast<C*>(src_ptr)) };
                static_cast<C*>(src_ptr)->~C();
            }
        }
        , destructor_ptr{ [](storage_ptr_t src_ptr) 
            { static_cast<C*>(src_ptr)->~C(); }
        }
    {}

    vtable(const vtable&) = delete;
    vtable(vtable&&) = delete;

    vtable& operator= (const vtable&) = delete;
    vtable& operator= (vtable&&) = delete;

    ~vtable() = default;
};

template<class R, class... Args>
inline constexpr vtable<R, Args...> empty_vtable{};

template<size_t DstCap, size_t DstAlign, size_t SrcCap, size_t SrcAlign>
struct is_valid_inplace_dst : std::true_type {
    static_assert(DstCap >= SrcCap,
        "Cannot squeeze larger inplace_function into a smaller one"
    );

    static_assert(DstAlign % SrcAlign == 0,
        "Incompatible inplace_function alignments"
    );
};

} // namespace inplace_function_detail

template<
    class Signature,
    size_t Capacity = inplace_function_detail::InplaceFunctionDefaultCapacity,
    size_t Alignment = alignof(std::aligned_storage_t<Capacity>)
>
class inplace_function; // unspecified

namespace inplace_function_detail {
    template<class> struct is_inplace_function : std::false_type {};
    template<class Sig, size_t Cap, size_t Align>
    struct is_inplace_function<inplace_function<Sig, Cap, Align>> : std::true_type {};
    template<class T>
    inline static constexpr auto is_inplace_function_v = is_inplace_function<T>::value;
} // namespace inplace_function_detail

template<class R, class... Args, std::size_t Capacity, std::size_t Alignment>
class inplace_function<R(Args...), Capacity, Alignment> {
    using storage_t = std::aligned_storage_t<Capacity, Alignment>;
    using vtable_t = inplace_function_detail::vtable<R, Args...>;
    using vtable_ptr_t = vtable_t const*;

    template <class, size_t, size_t> friend class inplace_function;

public:
    using capacity = std::integral_constant<std::size_t, Capacity>;
    using alignment = std::integral_constant<std::size_t, Alignment>;

    constexpr inplace_function() noexcept 
        : vtable_ptr_{std::addressof(inplace_function_detail::empty_vtable<R, Args...>)}
    {}

    template<class T, class C = std::decay_t<T>,
             class = std::enable_if_t<!inplace_function_detail::is_inplace_function_v<C>
                                      && std::is_invocable_r_v<R, C&, Args...>>>
    inplace_function(T&& closure) {
        static_assert(std::is_copy_constructible_v<C>,
            "inplace_function cannot be constructed from non-copyable type");

        static_assert(sizeof(C) <= Capacity,
            "inplace_function cannot be constructed from object with this (large) size");

        static_assert(Alignment % alignof(C) == 0,
            "inplace_function cannot be constructed from object with this (large) alignment");

        static const vtable_t vt{inplace_function_detail::wrapper<C>{}};
        vtable_ptr_ = std::addressof(vt);

        ::new (std::addressof(storage_)) C{std::forward<T>(closure)};
    }

    template<size_t Cap, size_t Align>
    constexpr inplace_function(inplace_function<R(Args...), Cap, Align> const& other)
        : inplace_function(other.vtable_ptr_, other.vtable_ptr_->copy_ptr, std::addressof(other.storage_))
    {
        static_assert(inplace_function_detail::is_valid_inplace_dst<Capacity, Alignment, Cap, Align>::value, 
            "conversion not allowed");
    }

    template<size_t Cap, size_t Align>
    constexpr inplace_function(inplace_function<R(Args...), Cap, Align>&& other) noexcept
        : inplace_function(std::exchange(other.vtable_ptr_, 
            std::addressof(inplace_function_detail::empty_vtable<R, Args...>)), 
            other.vtable_ptr_->relocate_ptr, std::addressof(other.storage_))
    {
        static_assert(inplace_function_detail::is_valid_inplace_dst<Capacity, Alignment, Cap, Align>::value, 
            "conversion not allowed");
    }

    constexpr inplace_function(std::nullptr_t) noexcept 
        : vtable_ptr_{std::addressof(inplace_function_detail::empty_vtable<R, Args...>)}
    {}

    constexpr inplace_function(inplace_function const& other) 
        : vtable_ptr_{other.vtable_ptr_}
    {
        vtable_ptr_->copy_ptr(
            std::addressof(storage_),
            std::addressof(other.storage_)
        );
    }

    constexpr inplace_function(inplace_function&& other) noexcept 
        : vtable_ptr_{std::exchange(other.vtable_ptr_, std::addressof(inplace_function_detail::empty_vtable<R, Args...>))}
    {
        vtable_ptr_->relocate_ptr(std::addressof(storage_), std::addressof(other.storage_));
    }

    constexpr inplace_function& operator=(std::nullptr_t) noexcept {
        vtable_ptr_->destructor_ptr(std::addressof(storage_));
        vtable_ptr_ = std::addressof(inplace_function_detail::empty_vtable<R, Args...>);
        return *this;
    }

    constexpr inplace_function& operator=(inplace_function other) noexcept {
        vtable_ptr_->destructor_ptr(std::addressof(storage_));
        vtable_ptr_ = std::exchange(other.vtable_ptr_, std::addressof(inplace_function_detail::empty_vtable<R, Args...>));
        vtable_ptr_->relocate_ptr(std::addressof(storage_), std::addressof(other.storage_));
        return *this;
    }

    ~inplace_function() {
        vtable_ptr_->destructor_ptr(std::addressof(storage_));
    }

    constexpr R operator()(Args... args) const {
        return vtable_ptr_->invoke_ptr(std::addressof(storage_), std::forward<Args>(args)...);
    }

    constexpr bool operator== (std::nullptr_t) const noexcept {
        return !operator bool();
    }

    constexpr bool operator!= (std::nullptr_t) const noexcept {
        return operator bool();
    }

    explicit constexpr operator bool() const noexcept {
        return vtable_ptr_ != std::addressof(inplace_function_detail::empty_vtable<R, Args...>);
    }

    void swap(inplace_function& other) noexcept {
        if (this == std::addressof(other)) 
            return;
        storage_t tmp;
        vtable_ptr_->relocate_ptr(std::addressof(tmp), std::addressof(storage_));
        other.vtable_ptr_->relocate_ptr(std::addressof(storage_), std::addressof(other.storage_));
        vtable_ptr_->relocate_ptr(std::addressof(other.storage_), std::addressof(tmp));
        std::swap(vtable_ptr_, other.vtable_ptr_);
    }

    friend void swap(inplace_function& lhs, inplace_function& rhs) noexcept {
        lhs.swap(rhs);
    }

private:
    vtable_ptr_t vtable_ptr_;
    mutable storage_t storage_;

    constexpr inplace_function(vtable_ptr_t vtable_ptr, typename vtable_t::process_ptr_t process_ptr, 
                               typename vtable_t::storage_ptr_t storage_ptr) 
        : vtable_ptr_{vtable_ptr}
    {
        process_ptr(std::addressof(storage_), storage_ptr);
    }
};

} // namespace nova

#endif // NOVA_INPLACE_FUNCTION_HPP