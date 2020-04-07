#ifndef NOVA_MAP_RESULTS_HPP
#define NOVA_MAP_RESULTS_HPP

#include <fmt/format.h>
#include "../../debug.hpp"
#include "optional.hpp"
#include "non_null_ptr.hpp"

#include <optional>
#include <tuple>
#include <type_traits>

namespace nova {

template<class K, class V>
class valid_lookup {
    K const& key_;
    V& val_;
public:
    constexpr valid_lookup(K const& key, V& val) noexcept
        : key_(key), val_(val) {}

    [[nodiscard]] constexpr K const& key() const noexcept { 
        return key_; 
    }
    [[nodiscard]] constexpr V& value() noexcept {
        return val_; 
    }
    [[nodiscard]] constexpr V const& value() const noexcept {
        return val_; 
    }

    template<std::size_t N>
    [[nodiscard]] decltype(auto) get() const noexcept {
        if constexpr (N == 0)
            return key_;
        else if constexpr (N == 1)
            return val_;
    }
};

template<class K, class V>
class lookup_result {
    K const* key_ = nullptr;
    V* val_ = nullptr;

public:
    constexpr lookup_result() noexcept = default;

    constexpr lookup_result(K const& key, V& val) noexcept
        : key_(std::addressof(key))
        , val_(std::addressof(val))
    {}

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return key_ != nullptr; }
    [[nodiscard]] constexpr K const& key() const noexcept {
        return *key_;
    }
    [[nodiscard]] constexpr V& value() const noexcept {
        return *val_;
    }
    [[nodiscard]] constexpr valid_lookup<K, V> operator*() const noexcept { 
        DEBUG_ASSERT(key_);
        return {*key_, *val_}; 
    }
};

template<class K, class V>
class insert_result {
    K const* key_;
    V* val_;
    bool inserted_;

public:
    constexpr insert_result(K const& key, V& val, bool const inserted) noexcept
        : key_(std::addressof(key))
        , val_(std::addressof(val))
        , inserted_(inserted) 
    {}

    [[nodiscard]] constexpr bool is_inserted() const noexcept { return inserted_; }
    [[nodiscard]] constexpr K const& key() const noexcept { return *key_; }
    [[nodiscard]] constexpr V& value() noexcept { return *val_; }
    [[nodiscard]] constexpr V const& value() const noexcept { return *val_; }
};

template<class K, class V>
using update_result = insert_result<K, V>;

template<class K, class V>
class remove_result {
    std::optional<std::pair<K, V>> opt_{std::nullopt};

public:
    constexpr remove_result() noexcept = default;

    template<class K1, class V1>
    constexpr remove_result(K1&& key, V1&& val)
        : opt_(std::in_place, std::forward<K1>(key), std::forward<V1>(val))
    {}

    [[nodiscard]] constexpr explicit operator bool() const noexcept { return opt_; }

    [[nodiscard]] constexpr K& get_key() { 
        DEBUG_ASSERT(opt_);
        return opt_->first; 
    }
    [[nodiscard]] constexpr V& get_value() { 
        DEBUG_ASSERT(opt_);
        return opt_->second; 
    }

    [[nodiscard]] constexpr K take_key() { 
        DEBUG_ASSERT(opt_);
        return (*std::move(opt_)).first; 
    }
    [[nodiscard]] constexpr V take_value() { 
        DEBUG_ASSERT(opt_);
        return (*std::move(opt_)).second; 
    }
};

} // namespace nova

namespace std {
    template<class K, class V>
    struct tuple_size<nova::valid_lookup<K, V>> : ::std::integral_constant<std::size_t, 2> {};

    template<class K, class V>
    struct tuple_element<0, nova::valid_lookup<K, V>> {
        using type = K const&;
    };

    template<class K, class V>
    struct tuple_element<1, nova::valid_lookup<K, V>> {
        using type = V&;
    };
}

#endif // NOVA_MAP_RESULTS_HPP