#ifndef NOVA_CURSOR_HPP
#define NOVA_CURSOR_HPP

#include "util/inplace_function.hpp"
#include "util/optional.hpp"

#include <type_traits>
#include <utility>

namespace nova {

namespace {
    inline static constexpr std::size_t largest_gen_size = 32UL; // TODO: check all possible size, don't hard code in size
}

template<class T>
class basic_cursor {
    using gen_t = inplace_function<optional<T>(), largest_gen_size>;
    std::size_t size_;
    gen_t gen_;
public:
    using value_type = T;

    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, basic_cursor>, int> = 0>
    constexpr basic_cursor(Fn&& fn) : size_(fn.size()), gen_(std::forward<Fn>(fn)) {}

    struct sentinel {};
    class iterator {
        gen_t gen_;
        optional<T> opt_;
        iterator(gen_t const& gen, optional<T> opt)
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

        [[nodiscard]] constexpr decltype(auto) operator*() noexcept {
            return *opt_;
        }

        [[nodiscard]] constexpr bool operator!=(sentinel const) const noexcept {
            return opt_.has_value();
        }
    };

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] auto begin() const& { return iterator{gen_}; }
    [[nodiscard]] auto begin() && { return iterator{std::move(gen_)}; }
    [[nodiscard]] constexpr auto end() const noexcept { return sentinel{}; }
};

using cursor = basic_cursor<document&>;
using const_cursor = basic_cursor<document const&>;

template<class T>
struct zero_index_lookup_t {
    constexpr optional<T&> operator()() const noexcept {
        return {};
    }

    constexpr std::size_t size() const noexcept {
        return 0;
    }
};

template<class T>
inline static constexpr zero_index_lookup_t<T> zero_index_lookup{};

template<class T>
class single_index_lookup {
    T* t_;
public:
    explicit constexpr single_index_lookup(non_null_ptr<T> const t) noexcept : t_(t) {}

    [[nodiscard]] constexpr optional<T&> operator()() noexcept {
        if (t_) {
            auto const tmp = std::exchange(t_, nullptr);
            return {*tmp};
        }
        return {};
    }

    constexpr std::size_t size() const noexcept {
        return 1;
    }
};

template<class T>
single_index_lookup(non_null_ptr<T>) -> single_index_lookup<T>;

template<class T, class Deref, class First, class Last = First>
class multiple_index_lookup_iter {
    First first_;
    Last const last_;
public:
    static_assert(std::is_constructible_v<optional<T>, std::invoke_result_t<Deref, First>>);

    template<class It, class Sent>
    constexpr explicit multiple_index_lookup_iter(It&& first, Sent&& last) noexcept
        : first_(std::forward<It>(first)), last_(std::forward<Sent>(last)) {}

    [[nodiscard]] constexpr optional<T> operator()() noexcept {
        if (first_ != last_) {
            auto const it = first_;
            std::advance(first_, 1);
            return tl::make_optional<T>(Deref{}(it));
        }
        return {};
    }

    std::size_t size() const noexcept {
        return std::distance(first_, last_);
    }
};

template<class T, class Deref, class First, class Last>
multiple_index_lookup_iter(First, Last) -> multiple_index_lookup_iter<T, Deref, First, Last>;

template<class T>
class multiple_index_lookup_vec {
    std::vector<non_null_ptr<T>> vec_;
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

    std::size_t size() const noexcept {
        return vec_.size();
    }
};

template<template<class> class V, class T>
multiple_index_lookup_vec(V<non_null_ptr<T>>) -> multiple_index_lookup_vec<T>;

} // namespace nova

#endif // NOVA_CURSOR_HPP