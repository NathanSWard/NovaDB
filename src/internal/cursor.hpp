#ifndef NOVA_CURSOR_HPP
#define NOVA_CURSOR_HPP

#include "util/inplace_function.hpp"
#include "util/optional.hpp"

#include <type_traits>
#include <utility>

namespace nova {

namespace {
    inline static constexpr std::size_t largest_gen_size = sizeof(std::vector<document*>);
}

template<class T>
class basic_cursor {
    using gen_t = inplace_function<optional<T&>(), largest_gen_size>;
    gen_t gen_;
public:
    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, basic_cursor>, int> = 0>
    constexpr basic_cursor(Fn&& fn) : gen_(std::forward<Fn>(fn)) {}

    struct sentinel {};
    class iterator {
        gen_t gen_;
        optional<T&> opt_;
        iterator(gen_t const& gen, optional<T&> opt)
            : gen_(gen), opt_(opt) {}
    public:
        explicit constexpr iterator(gen_t const& gen) 
            : gen_(gen), opt_(gen_()) {}

        explicit constexpr iterator(gen_t&& gen) noexcept
            : gen_(std::move(gen)), opt_(gen_()) {}

        iterator& operator++() {
            opt_ = gen_();
            return *this;
        }

        [[nodiscard]] constexpr T& operator*() noexcept {
            return *opt_;
        }

        [[nodiscard]] constexpr T* operator->() noexcept {
            return std::addressof(*opt_);
        }

        [[nodiscard]] constexpr bool operator!=(sentinel const) const noexcept {
            return opt_.has_value();
        }
    };

    [[nodiscard]] auto begin() const& { return iterator{gen_}; }
    [[nodiscard]] auto begin() && { return iterator{std::move(gen_)}; }
    [[nodiscard]] constexpr auto end() const noexcept { return sentinel{}; }
};

using cursor = basic_cursor<document>;
using const_cursor = basic_cursor<document const>;

template<class T>
inline static constexpr auto zero_index_lookup = []() -> optional<T&> { return {}; };

template<class T>
class single_index_lookup {
    T* t_;
public:
    explicit constexpr single_index_lookup(T* const t) noexcept : t_(t) {}

    [[nodiscard]] constexpr optional<T&> operator()() noexcept {
        if (t_) {
            T* const tmp = std::exchange(t_, nullptr);
            return {*tmp};
        }
        return {};
    }
};

template<class T>
single_index_lookup(T* const) -> single_index_lookup<T>;

struct deref_map_iter_second {
    template<class It>
    constexpr decltype(auto) operator()(It&& it) const noexcept {
        return *(it->second);
    }
};

template<class T, class First, class Last = First, class Deref = deref_map_iter_second>
class multiple_index_lookup_iter {
    First first_;
    Last const last_;
public:
    constexpr explicit multiple_index_lookup_iter(First const first, Last const last) noexcept
        : first_(first), last_(last) {}

    [[nodiscard]] constexpr optional<T&> operator()() noexcept {
        if (first_ != last_) {
            auto const it = first_;
            std::advance(first_, 1);
            return {Deref{}(it)};
        }
        return {};
    }
};

template<class T, class First, class Last, class Deref>
multiple_index_lookup_iter(First, Last) -> multiple_index_lookup_iter<T, First, Last, Deref>;

template<class T>
class multiple_index_lookup_vec {
    std::vector<T*> vec_;
public:
    template<class V, std::enable_if_t<!std::is_same_v<V, multiple_index_lookup_vec>, int> = 0>
    explicit multiple_index_lookup_vec(V&& v) 
        : vec_(std::forward<V>(v)) 
    {
        // std::reverse(vec_.begin(), vec_.end()); maybe?
    }

    [[nodiscard]] optional<T&> operator()() noexcept {
        if (!vec_.empty()) {
            auto const doc = vec_.back();
            vec_.pop_back();
            return {*doc};
        }
        return {};
    }
};

template<template<class> class V, class T>
multiple_index_lookup_vec(V<T*>) -> multiple_index_lookup_vec<T>;

} // namespace nova

#endif // NOVA_CURSOR_HPP