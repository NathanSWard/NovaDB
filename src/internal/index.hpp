#ifndef NOVA_INDEX_HPP
#define NOVA_INDEX_HPP

#include "cursor.hpp"
#include "document.hpp"
#include "util/function_ref.hpp"
#include "util/inplace_function.hpp"
#include "util/span.hpp"

#include <map>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nova {

namespace detail {

template<class It>
[[nodiscard]] inline constexpr decltype(auto) deref_map_iter(It const& it) noexcept(noexcept(*(it->second))) {
    return *(it->second);
}

} // namespace detail

struct no_filter {
    template<class T>
    constexpr bool operator()(T&&) const noexcept { return true; }
};

enum class index_insert_result : std::uint8_t {
    success,
    already_exists,
    filter_failed,
};

struct _base_index_interface {
    [[nodiscard]] virtual bool empty() const noexcept = 0;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual std::size_t field_count() const noexcept = 0;
    virtual void clear() = 0;
};

struct _single_field_index_interface : public _base_index_interface {
    virtual index_insert_result insert(bson const&, document* const) = 0;
    [[nodiscard]] virtual lookup_result<bson, document> lookup_one(bson const&) = 0;
    [[nodiscard]] virtual lookup_result<bson, document const> lookup_one(bson const&) const = 0;
    [[nodiscard]] virtual cursor lookup_if(function_ref<bool(bson const&)>) = 0;
    virtual bool erase(bson const&) = 0;
    virtual std::size_t erase_if(function_ref<bool(bson const&)>) = 0;
    virtual function_ref<bool(bson const&)> value_filter() const noexcept = 0;
};

struct single_field_unique_index_interface : public _single_field_index_interface {};

struct single_field_multi_index_interface : public _single_field_index_interface {
    [[nodiscard]] virtual cursor lookup_many(bson const&) = 0;
};

struct _compound_index_interface : public _base_index_interface {
    virtual index_insert_result insert(span<bson>, document* const) = 0;
    [[nodiscard]] virtual lookup_result<span<bson const>, document> lookup_one(span<bson const>) = 0;
    [[nodiscard]] virtual lookup_result<span<bson const>, document const> lookup_one(span<bson const>) const = 0;
    [[nodiscard]] virtual cursor lookup_if(function_ref<bool(span<bson const>)>) = 0;
    virtual std::size_t erase(span<bson>) = 0;
    virtual std::size_t erase_if(function_ref<bool(span<bson const>)>) = 0;
    virtual function_ref<bool(span<bson const>)> value_filter() const noexcept = 0;
};

struct compound_unique_index_interface : public _compound_index_interface {};

struct compound_multi_index_interface : public _compound_index_interface {
    [[nodiscard]] virtual cursor lookup_many(span<bson const>) = 0;
};

template<template<class...> class MapT, class Filter>
class basic_single_field_unique_index final : public single_field_unique_index_interface, private Filter {
    MapT<bson, document*> map_{};
    constexpr bool filter(bson const& val) const noexcept {
        return static_cast<Filter const&>(*this)(val);
    }
public:
    index_insert_result insert(bson const& val, document* const doc) final {
        if (filter(val)) {
            auto const result = map_.try_emplace(val, doc);
            return result.second ? index_insert_result::success : index_insert_result::already_exists;
        }
        return index_insert_result::filter_failed;
    }

    [[nodiscard]] lookup_result<bson, document> lookup_one(bson const& val) final {
        if (auto const it = map_.find(val); it != map_.end())
            return {it->first, *(it->second)};
        return {};
    }

    [[nodiscard]] lookup_result<bson, document const> lookup_one(bson const& val) const final {
        if (auto const it = map_.find(val); it != map_.end())
            return {it->first, *(it->second)};
        return {};
    }

    [[nodiscard]] cursor lookup_if(function_ref<bool(bson const&)> fn) final {
        std::vector<document*> vec;
        for (auto&& [k, v] : map_) {
            if (fn(k))
                vec.push_back(k);
        }
        if (vec.empty())
            return zero_index_lookup;
        else if (vec.size() == 1)
            return single_index_lookup{vec.front()};
        else
            return multiple_index_lookup_vec{std::move(vec)};
    }

    bool erase(bson const& val) final {
        return map_.erase(val) > 0;
    }

    std::size_t erase_if(function_ref<bool(bson const&)> fn) final {
        std::size_t count = 0;
        for (auto it = map_.begin(); it != map_.end(); ++it) {
            if (fn(it->first)) {
                map_.erase(it);
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool empty() const noexcept final { return map_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept  final { return map_.size(); }
    [[nodiscard]] constexpr std::size_t field_count() const noexcept final { return 1; }
    void clear() { map_.clear(); }
    [[nodiscard]] function_ref<bool(bson const&)> value_filter() const noexcept final {
        return static_cast<Filter&>(*this);
    }
};

template<template<class...> class MapT, class Filter>
class basic_single_field_multi_index : private Filter {
    MapT<bson, document*> map_{};
    using map_iter_t = decltype(map_.begin());
    constexpr bool filter(bson const& val) const {
        return static_cast<Filter const&>(*this)(val);
    }
public:
    index_insert_result insert(bson const& val, document* const doc) final {
        if (Filter{}(val)) {
            map_.emplace(val, doc);
            return index_insert_result::success;
        }
        return index_insert_result::filter_failed;
    }

    [[nodiscard]] lookup_result<bson, document> lookup_one(bson const& val) final {
        if (auto const it = map_.find(val); it != map_.end())
            return {it->first, *(it->second)};
        return {};
    }

    [[nodiscard]] lookup_result<bson, document const> lookup_one(bson const& val) const final {
        if (auto const it = map_.find(val); it != map_.end())
            return {it->first, *(it->second)};
        return {};
    }

    [[nodiscard]] cursor lookup_many(bson const& val) final {
        if (auto const [first, last] = map_.equal_range(val); first != map_.end())
            return multiple_index_lookup_iter<&detail::deref_map_iter<map_iter_t>, map_iter_t>{first, last};
        return zero_index_lookup;
    }

    [[nodiscard]] cursor lookup_if(function_ref<bool(bson const&)> fn) final {
        std::vector<document*> vec;
        for (auto&& [k, v] : map_) {
            if (fn(k))
                vec.push_back(v);
        }

        if (vec.empty())
            return zero_index_lookup;
        else if (vec.size() == 1)
            return single_index_lookup{vec.front()};
        else
            return multiple_index_lookup_vec{std::move(vec)};
    }

    std::size_t erase(bson const& val) final {
        return map_.erase(val);
    }

    bool erase(bson const& val, document* const doc) final {
        if (auto [first, last] = map_.equal_range(val); first != map_.end()) {
            while (first != last) {
                if (first->second == doc) {
                    map_.erase(first);
                    return true;
                }
                ++first;
            }
        }
        return false;
    }

    std::size_t erase_if(function_ref<bool(bson const&)> fn) final {
        std::size_t count = 0;
        for (auto it = map_.begin(); it != map_.end(); ++it) {
            if (fn(it->first)) {
                map_.erase(it);
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool empty() const noexcept final { return map_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept  final { return map_.size(); }
    [[nodiscard]] constexpr std::size_t field_count() const noexcept final { return 1; }
    void clear() { map_.clear(); }
    [[nodiscard]] function_ref<bool(bson const&)> value_filter() const noexcept final {
        return static_cast<Filter&>(*this);
    }
};

struct index_key_compare {
    using is_transparent = void;

    template<std::size_t N>
    constexpr bool operator()(std::array<bson, N> const& a, std::array<bson, N> const& b) const noexcept {
        return a < b;
    }

    template<std::size_t N>
    constexpr bool operator()(std::array<bson, N> const& a, bson const& b) const noexcept {
        return std::get<0>(a) < b;
    }

    template<std::size_t N>
    constexpr bool operator()(bson const& b, std::array<bson, N> const& a) const noexcept {
        return b < std::get<0>(a);
    }

    template<std::size_t N, std::size_t M, std::enable_if_t<(N != M), int> = 0>
    constexpr bool operator()(std::array<bson, N> const& a, std::array<bson, M> const& b) const noexcept {
        constexpr auto min = std::min(N, M);
        for (std::size_t i = 0; i < min; ++i) {
            if (a[i] == b[i])
                continue;
            return a[i] < b[i];
        }
        return false;
    }

    template<std::size_t N>
    constexpr bool operator()(std::array<bson, N> const& a, span<bson> const s) const noexcept {
        auto const min = std::min(N, s.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (a[i] == s[i])
                continue;
            return a[i] < s[i];
        }
        return false;
    }

    template<std::size_t N>
    constexpr bool operator()(span<bson> const s, std::array<bson, N> const& a) const noexcept {
        auto const min = std::min(N, s.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (s[i] == a[i])
                continue;
            return s[i] < a[i];
        }
        return false;
    }
};

template<template<class...> class MapT, std::size_t N, class Func, class Filter>
class basic_compound_unique_index_impl final : public compound_unique_index_interface, private Filter {
    MapT<std::array<bson, N>, document*, Func> map_;
    constexpr bool filter(span<bson const, N> const vals) const {
        return static_cast<Filter const&>(*this)(vals);
    }
public:
    index_insert_result insert(span<bson> vals, document* const doc) final {
        DEBUG_ASSERT(vals.size() == N);
        if (filter(vals)) {
            auto const result = map_.try_emplace(span_to_array<N>(vals), doc);
            return result.second ? index_insert_result::success : index_insert_result::already_exists;
        }
        return index_insert_result::filter_failed;
    }

    [[nodiscard]] lookup_result<span<bson const>, document> lookup_one(span<bson const> const s) final {
        if (auto const found = map_.find(s); found != map_.end())
            return {found->first, found->second};
        return {};
    }

    [[nodiscard]] lookup_result<span<bson const>, document const> lookup_one(span<bson const> const s) const final {
        if (auto const found = map_.find(s); found != map_.end())
            return {found->first, found->second};
        return {};
    }

    [[nodiscard]] cursor lookup_if(function_ref<bool(span<bson const>)> fn) final {
        std::vector<document*> docs;
        for (auto&& [k, v] : map_) 
            if (fn(k))
                docs.push_back(v);
        if (docs.empty())
            return zero_index_lookup;
        else if (docs.size() == 1)
            return single_index_lookup{docs.front()};
        else
            return multiple_index_lookup_vec{std::move(docs)};
    }

    bool erase(span<bson> s) final {
        DEBUG_ASSERT(s.size() == N);
        return map_.erase(span_to_array<N>(s)) > 0;
    }

    std::size_t erase_if(function_ref<bool(span<bson const>)> fn) final {
        std::size_t count = 0;
        for (auto it = map_.begin(); it != map_.end(); ++it) {
            if (fn(it->first)) {
                map_.erase(it);
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool empty() const noexcept final { return map_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept final { return map_.size(); }
    [[nodiscard]] std::size_t field_cound() const noexcept final { return N; }
    void clear() final { map_.clear(); }
    [[nodiscard]] function_ref<bool(span<bson const>)> value_filter() const noexcept final {
        return static_cast<Filter&>(*this);
    }
};

template<template<class...> class MapT, std::size_t N, class Func, class Filter>
class basic_compound_multi_index_impl final : public compound_multi_index_interface, private Filter {
    MapT<std::array<bson, N>, document*, Func> map_;
    using map_iter_t = decltype(map_.begin());
    constexpr bool filter(span<bson const, N> const sp) const {
        return static_cast<Filter const&>(*this)(sp);
    }
public: 
    index_insert_result insert(span<bson> vals, document* const doc) final {
        DEBUG_ASSERT(vals.size() == N);
        if (filter(vals)) {
            map_.emplace(span_to_array<N>(vals), doc);
            return index_insert_result::success;
        }
        return index_insert_result::filter_failed;
    }

    [[nodiscard]] lookup_result<span<bson const>, document> lookup_one(span<bson const> const vals) final {
        if (auto const found = map_.find(vals); found != map_.end())
            return {found->first, found->second};
        return {};
    }

    [[nodiscard]] lookup_result<span<bson const>, document const> lookup_one(span<bson const> const vals) const final {
        if (auto const found = map_.find(vals); found != map_.end())
            return {found->first, found->second};
        return {};
    }

    [[nodiscard]] cursor lookup_many(span<bson const> const vals) final {
        if (auto const [first, last] = map_.equal_range(vals); first != map_.end())
            return multiple_index_lookup_iter<&detail::deref_map_iter<map_iter_t>, map_iter_t>{first, last};
        return zero_index_lookup;
    }

    [[nodiscard]] cursor lookup_if(function_ref<bool(span<bson const>)> fn) final {
        std::vector<document*> docs;
        for (auto&& [k, v] : map_) 
            if (fn(k))
                docs.push_back(v);
        if (docs.empty())
            return zero_index_lookup;
        else if (docs.size() == 1)
            return single_index_lookup{docs.front()};
        else
            return multiple_index_lookup_vec{std::move(docs)};
    }

    std::size_t erase(span<bson const> s) final { // todo maybe take span<bson*> to avoid copy
        DEBUG_ASSERT(s.size() == N);
        return map_.erase(span_to_array<N>(s)); // return array<bson*> ????
    }

    std::size_t erase_if(function_ref<bool(span<bson const>)> fn) final {
        std::size_t count = 0;
        for (auto it = map_.begin(); it != map_.end(); ++it) {
            if (fn(it->first)) {
                map_.erase(it);
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] bool empty() const noexcept final { return map_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept  final { return map_.size(); }
    [[nodiscard]] std::size_t field_count() const noexcept final { return N; }
    void clear() final { map_.clear(); }
    [[nodiscard]] function_ref<bool(span<bson const>)> value_filter() const noexcept final {
        return static_cast<Filter&>(*this);
    }
};

} // namespace nova

namespace std {
    template<std::size_t N>
    struct hash<std::array<nova::bson, N>> {
        [[nodiscard]] constexpr std::size_t operator()(std::array<nova::bson, N> const& arr) const noexcept {
            std::size_t hash{};
            for (auto&& val : arr)
                hash += std::hash<nova::bson>{}(val);
            return hash;
        }
    };
} // namespace std

namespace nova {

template<class Filter = no_filter>
using ordered_single_field_unqiue_index = basic_single_field_unique_index<std::map, Filter>;
template<class Filter = no_filter>
using ordered_single_field_multi_index = basic_single_field_multi_index<std::multimap, Filter>;
template<std::size_t N, class Filter = no_filter>
using ordered_compound_unique_index = basic_compound_unique_index_impl<std::map, N, index_key_compare, Filter>;
template<std::size_t N, class Filter = no_filter>
using ordered_compound_multi_index = basic_compound_multi_index_impl<std::multimap, N, index_key_compare, Filter>;

} // namespace nova

#endif // NOVA_INDEX_HPP