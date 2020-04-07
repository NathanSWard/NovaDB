#ifndef NOVA_CURSOR_HPP
#define NOVA_CURSOR_HPP

#include "util/inplace_function.hpp"
#include "util/optional.hpp"

#include <type_traits>
#include <utility>

namespace nova {

using cursor_doc_gen = inplace_function<optional<document&>(), 16>;

class cursor {
    cursor_doc_gen gen_;
public:
    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, cursor>, int> = 0>
    constexpr cursor(Fn&& fn) : gen_(std::forward<Fn>(fn)) {}

    struct sentinel {};
    class iter {
        cursor_doc_gen gen_;
        optional<document&> opt_;
        iter(cursor_doc_gen const& gen, optional<document&> opt)
            : gen_(gen), opt_(opt) {}
    public:
        explicit constexpr iter(cursor_doc_gen const& gen) 
            : gen_(gen), opt_(gen_()) {}

        explicit constexpr iter(cursor_doc_gen&& gen) noexcept
            : gen_(std::move(gen)), opt_(gen_()) {}

        iter& operator++() {
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

    [[nodiscard]] auto begin() const& { return iter{gen_}; }
    [[nodiscard]] auto begin() && { return iter{std::move(gen_)}; }
    [[nodiscard]] constexpr auto end() const noexcept { return sentinel{}; }
};

inline static constexpr auto zero_index_lookup = []() -> optional<document&> { return {}; };

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

} // namespace nova

#endif // NOVA_CURSOR_HPP