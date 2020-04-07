#ifndef NOVA_CURSOR_HPP
#define NOVA_CURSOR_HPP

#include "util/inplace_function.hpp"
#include "util/optional.hpp"

#include <type_traits>
#include <utility>

namespace nova {

namespace {
    static constexpr std::size_t largest_gen_size = sizeof(std::vector<document*>);
}

class cursor {
    using gen_t = inplace_function<optional<document&>(), largest_gen_size>;
    gen_t gen_;
public:
    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, cursor>, int> = 0>
    constexpr cursor(Fn&& fn) : gen_(std::forward<Fn>(fn)) {}

    struct sentinel {};
    class iterator {
        gen_t gen_;
        optional<document&> opt_;
        iterator(gen_t const& gen, optional<document&> opt)
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

        [[nodiscard]] constexpr document& operator*() noexcept {
            return *opt_;
        }

        [[nodiscard]] constexpr document* operator->() noexcept {
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

class const_cursor {
    using gen_t = inplace_function<optional<document const&>(), largest_gen_size>;
    gen_t gen_;
public:
    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, const_cursor>, int> = 0>
    constexpr const_cursor(Fn&& fn) : gen_(std::forward<Fn>(fn)) {}

    struct sentinel {};
    class iterator {
        gen_t gen_;
        optional<document const&> opt_;
    public:
        explicit constexpr iterator(gen_t const& gen) 
            : gen_(gen), opt_(gen_()) {}

        explicit constexpr iterator(gen_t&& gen) noexcept
            : gen_(std::move(gen)), opt_(gen_()) {}

        iterator& operator++() {
            opt_ = gen_();
            return *this;
        }

        [[nodiscard]] constexpr document const& operator*() noexcept {
            return *opt_;
        }

        [[nodiscard]] constexpr document const* operator->() noexcept {
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

inline static constexpr auto zero_index_lookup = []() -> optional<document&> { return {}; };
inline static constexpr auto zero_index_lookup_const = []() -> optional<document const&> { return {}; };

class single_index_lookup {
    document* doc_;
public:
    explicit constexpr single_index_lookup(document* const doc) noexcept : doc_(doc) {}

    [[nodiscard]] constexpr optional<document&> operator()() noexcept {
        if (doc_) {
            document* const doc = std::exchange(doc_, nullptr);
            return {*doc};
        }
        return {};
    }
};

class single_index_lookup_const {
    document const* doc_;
public:
    explicit constexpr single_index_lookup_const(document const* const doc) noexcept : doc_(doc) {}

    [[nodiscard]] constexpr optional<document const&> operator()() noexcept {
        if (doc_) {
            document const* const doc = std::exchange(doc_, nullptr);
            return {*doc};
        }
        return {};
    }
};

template<auto Deref, class First, class Last = First>
class multiple_index_lookup_iter {
    First first_;
    Last const last_;
public:
    static_assert(std::is_invocable_r_v<document&, decltype(Deref), First>);

    constexpr explicit multiple_index_lookup_iter(First const first, Last const last) noexcept
        : first_(first), last_(last) {}

    [[nodiscard]] constexpr optional<document&> operator()() noexcept {
        if (first_ != last_) {
            auto const it = first_;
            std::advance(first_, 1);
            return {Deref(it)};
        }
        return {};
    }
};

template<auto Deref, class First, class Last = First>
class multiple_index_lookup_iter_const {
    First first_;
    Last const last_;
public:
    static_assert(std::is_invocable_r_v<document const&, decltype(Deref), First>);

    constexpr explicit multiple_index_lookup_iter_const(First const first, Last const last) noexcept
        : first_(first), last_(last) {}

    [[nodiscard]] constexpr optional<document const&> operator()() noexcept {
        if (first_ != last_) {
            auto const it = first_;
            std::advance(first_, 1);
            return {Deref(it)};
        }
        return {};
    }
};

class multiple_index_lookup_vec {
    std::vector<document*> vec_;
public:
    template<class V, std::enable_if_t<!std::is_same_v<V, multiple_index_lookup_vec>, int> = 0>
    explicit multiple_index_lookup_vec(V&& v) 
        : vec_(std::forward<V>(v)) 
    {
        // std::reverse(vec_.begin(), vec_.end()); maybe?
    }

    [[nodiscard]] optional<document&> operator()() noexcept {
        if (!vec_.empty()) {
            auto const doc = vec_.back();
            vec_.pop_back();
            return {*doc};
        }
        return {};
    }
};

class multiple_index_lookup_vec_const {
    std::vector<document const*> vec_;
public:
    template<class V, std::enable_if_t<!std::is_same_v<V, multiple_index_lookup_vec>, int> = 0>
    explicit multiple_index_lookup_vec_const(V&& v) 
        : vec_(std::forward<V>(v)) 
    {
        // std::reverse(vec_.begin(), vec_.end()); maybe?
    }

    [[nodiscard]] optional<document const&> operator()() noexcept {
        if (!vec_.empty()) {
            auto const doc = vec_.back();
            vec_.pop_back();
            return {*doc};
        }
        return {};
    }
};

} // namespace nova

#endif // NOVA_CURSOR_HPP