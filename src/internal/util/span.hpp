#ifndef NOVA_SPAN_HPP
#define NOVA_SPAN_HPP

#include "../../debug.hpp"
#include "../detail.hpp"
#include "non_null_ptr.hpp"

#include <cstdint>
#include <type_traits>

namespace nova {

inline static constexpr size_t dynamic_extent = -1;

namespace detail {

template<std::size_t N>
struct span_size_storage {
    static constexpr std::size_t size_ = N;
};

template<>
struct span_size_storage<dynamic_extent> {
    constexpr span_size_storage(std::size_t const size) noexcept : size_(size) {}
    std::size_t size_;
};

template <class R, class T>
struct is_span_convertible_range
    : std::bool_constant<std::is_convertible_v<std::remove_pointer_t<decltype(std::data(std::declval<R&>()))> (*)[], T (*)[]>> {};

template<class R, class T>
inline static constexpr bool is_span_convertible_range_v = is_span_convertible_range<R, T>::value;

template<class T>
struct is_container_like  {
    template<class U>
    static auto test(int) -> decltype(std::declval<U>().size(), std::declval<U>().data(), std::true_type{});
    template<class>
    static std::false_type test(...);

    static constexpr bool value = decltype(test<T>(0))::value;
};

template<class T>
inline static constexpr bool is_container_like_v = is_container_like<T>::value;

} // namespace detail

template<class T, size_t N = dynamic_extent>
class span : private detail::span_size_storage<N> {
    using storage = detail::span_size_storage<N>;
    T* ptr_ = nullptr;
public:
    constexpr span() noexcept = default;

    constexpr span(span const&) = default;
    constexpr span& operator=(span const&) = default; 
    constexpr span(span&&) = default;
    constexpr span& operator=(span&&) = default; 

    template<class First, class Last>
    constexpr span(First const first, Last const last) noexcept
        : storage(std::distance(first, last))
        , ptr_(std::addressof(*first))
    {}

    template<class It, std::enable_if_t<detail::is_iterator_like_v<It>, int> = 0>
    constexpr span(It const it, std::size_t const count) noexcept
        : storage(count)
        , ptr_(std::addressof(*it))
    {}

    template<class R, std::enable_if_t<std::conjunction_v<detail::meta_not<std::is_same_v<std::decay_t<R>, span>>, 
        detail::is_container_like<R>, detail::is_span_convertible_range<R, T>>, int> = 0>
    constexpr span(R&& r) noexcept 
        : storage(r.size())
        , ptr_(r.data()) 
    {}

    template<class U, size_t M, std::enable_if_t<N == M, int> = 0>
    constexpr span(std::array<U, N>& arr) noexcept
        : ptr_(arr.data()) {}

    template<class U, size_t M, std::enable_if_t<N == M, int> = 0>
    constexpr span(std::array<U, N> const& arr) noexcept
        : ptr_(arr.data()) {}

    //template<class U, std::enable_if_t<std::is_lvalue_reference_v<U> && std::is_same_v<T, std::decay_t<U>>
    //                                   && !std::is_same_v<std::decay_t<U>, span>, int> = 0>
    //constexpr span(U&& u) noexcept 
    //    : storage(1)
    //    , ptr_(std::addressof(u)) 
    //{}

    constexpr T& operator[](std::size_t const pos) const noexcept {
        DEBUG_ASSERT(pos < size());
        return ptr_[pos];
    }

    constexpr std::size_t size() const noexcept {
        return this->size_;
    }

    constexpr T* begin() const noexcept {
        return ptr_;
    }
    constexpr T* end() const noexcept {
        return ptr_ + this->size_;
    }
};

template<class T, std::size_t N, class U, std::size_t... I>
constexpr std::array<T, N> _span_to_array_impl(span<U> const s, std::index_sequence<I...>) {
    return {{s[I]...}};
}

template<class T, std::size_t N, class U>
constexpr std::array<T, N> span_to_array(span<U> const s) {
    DEBUG_ASSERT(s.size() == N);
    return _span_to_array_impl<T, N>(s, std::make_index_sequence<N>{});
}

template<class T, std::size_t N, class U, std::size_t... I>
constexpr std::array<T, N> _span_to_array_deref_impl(span<non_null_ptr<U>> const s, std::index_sequence<I...>) {
    return {{(*(s[I]))...}};
}

template<class T, std::size_t N, class U>
constexpr std::array<T, N> span_to_array_deref(span<non_null_ptr<U>> const s) {
    DEBUG_ASSERT(s.size() == N);
    return _span_to_array_deref_impl<T, N>(s, std::make_index_sequence<N>{});
}

} // namespave nova

#endif // NOVA_SPAN_HPP