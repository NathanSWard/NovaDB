#ifndef NOVA_INDEX_HPP
#define NOVA_INDEX_HPP

#include <absl/container/btree_map.h>
#include <absl/container/node_hash_map.h>

#include "cursor.hpp"
#include "document.hpp"
#include "util/function_ref.hpp"
#include "util/inplace_function.hpp"
#include "util/span.hpp"

#include <numeric>
#include <utility>
#include <vector>

namespace nova {

namespace detail {

struct no_filter {
    template<class T>
    constexpr bool operator()(T&&) const noexcept { return true; }
};

struct compound_index_key_cmp {
    using is_transparent = void;

    template<class T, std::size_t N>
    constexpr bool operator()(std::array<T, N> const& a, std::array<T, N> const& b) const noexcept {
        return a < b;
    }

    template<class T, std::size_t N>
    constexpr bool operator()(std::array<T, N> const& a, T const& b) const noexcept {
        return std::get<0>(a) < b;
    }

    template<class T, std::size_t N>
    constexpr bool operator()(T const& b, std::array<T, N> const& a) const noexcept {
        return b < std::get<0>(a);
    }

    template<class T, std::size_t N, std::size_t M>
    constexpr bool operator()(std::array<T, N> const& a, std::array<T, M> const& b) const noexcept {
        constexpr auto min = std::min(N, M);
        for (std::size_t i = 0; i < min; ++i) {
            if (a[i] == b[i])
                continue;
            return a[i] < b[i];
        }
        return false;
    }

    template<class T, std::size_t N, class U, std::size_t M>
    constexpr bool operator()(std::array<T, N> const& a, std::array<U, M> const& b) const noexcept {
        constexpr auto min = std::min(N, M);
        for (std::size_t i = 0; i < min; ++i) {
            if (a[i] == b[i])
                continue;
            return a[i] < b[i];
        }
        return false;
    }

    template<class T, std::size_t N, class U, std::size_t M>
    constexpr bool operator()(std::array<T, N> const& a, std::array<non_null_ptr<U>, M> const& b) const noexcept {
        constexpr auto min = std::min(N, M);
        for (std::size_t i = 0; i < min; ++i) {
            if (a[i] == *b[i])
                continue;
            return a[i] < *b[i];
        }
        return false;
    }

    template<class U, std::size_t N, class T, std::size_t M>
    constexpr bool operator()(std::array<non_null_ptr<U>, N> const& a, std::array<T, M> const& b) const noexcept {
        constexpr auto min = std::min(N, M);
        for (std::size_t i = 0; i < min; ++i) {
            if (*a[i] == b[i])
                continue;
            return *a[i] < b[i];
        }
        return false;
    }

    template<class T, std::size_t N, class U, std::size_t M>
    constexpr bool operator()(std::array<T, N> const& a, span<U, M> const s) const noexcept {
        auto const min = std::min(N, s.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (a[i] == s[i])
                continue;
            return a[i] < s[i];
        }
        return false;
    }

    template<class U, std::size_t N, class T, std::size_t M>
    constexpr bool operator()(span<U, M> const s, std::array<T, N> const& a) const noexcept {
        auto const min = std::min(N, s.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (s[i] == a[i])
                continue;
            return s[i] < a[i];
        }
        return false;
    }

    template<class T, std::size_t N, class U, std::size_t M>
    constexpr bool operator()(std::array<T, N> const& a, span<non_null_ptr<U>, M> const s) const noexcept {
        auto const min = std::min(N, s.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (a[i] == *s[i])
                continue;
            return a[i] < *s[i];
        }
        return false;
    }

    template<class U, std::size_t N, class T, std::size_t M>
    constexpr bool operator()(span<non_null_ptr<U>, M> const s, std::array<T, N> const& a) const noexcept {
        auto const min = std::min(N, s.size());
        for (std::size_t i = 0; i < min; ++i) {
            if (*s[i] == a[i])
                continue;
            return *s[i] < a[i];
        }
        return false;
    }
};

template<class Filter>
struct filter_wrapper : public Filter {
    template<class... Args, std::enable_if_t<std::is_constructible_v<Filter, Args...>, int> = 0>
    constexpr filter_wrapper(Args&&... args)
        : Filter(std::forward<Args>(args)...)
    {}

    template<class T>
    constexpr bool filter(T const& t) const noexcept {
        return static_cast<Filter const&>(*this)(t);
    }
};

struct deref_map_iter_second {
    template<class It>
    constexpr decltype(auto) operator()(It&& it) const noexcept {
        return *(it->second);
    }
};

struct sf_cursor_deref {
    template<class It>
    constexpr decltype(auto) operator()(It&& it) const noexcept {
        return *std::forward<It>(it);
    }
};

struct cmp_cursor_deref {
    template<class It>
    constexpr auto operator()(It&& it) const noexcept {
        return std::pair<span<bson const>, non_null_ptr<document>>(it->first, it->second);
    }
};

struct sf_const_cursor_deref {
    template<class It>
    constexpr decltype(auto) operator()(It&& it) const noexcept {
        return *reinterpret_cast<std::pair<bson const, non_null_ptr<const document>> const*>(std::addressof(*it));
    }
};

struct cmp_const_cursor_deref {
    template<class It>
    constexpr auto operator()(It&& it) const noexcept {
        return std::pair<span<bson const>, non_null_ptr<document const>>(it->first, it->second);
    }
};

} // namespace detail

namespace {
    inline static constexpr auto max_iter_size = std::max(sizeof(typename absl::btree_map<std::array<int, 10>, std::string>::iterator) 
                                                          , sizeof(typename absl::btree_multimap<int, int>::iterator));
}

using sf_index_cursor = basic_cursor<std::pair<bson const, non_null_ptr<document>>&>;
using cmp_index_cursor = basic_cursor<std::pair<span<bson const>, non_null_ptr<document>>>;
using sf_index_const_cursor = basic_cursor<std::pair<bson const, non_null_ptr<document const>> const&>;
using cmp_index_const_cursor = basic_cursor<std::pair<span<bson const>, non_null_ptr<document const>>>;

enum class index_insert_result : std::uint8_t {
    success,
    already_exists,
    filter_failed,
};

//
// index base classes:
//
struct _base_index_interface {
    [[nodiscard]] virtual bool empty() const noexcept = 0;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual std::size_t field_count() const noexcept = 0;
    [[nodiscard]] virtual bool contains_doc(non_null_ptr<document const> const doc) const = 0;
    virtual void clear() = 0;
    virtual ~_base_index_interface() = default;
};

struct _single_field_index_interface : public _base_index_interface {
    virtual index_insert_result insert(bson const&, non_null_ptr<document> const) = 0;
    [[nodiscard]] virtual bool contains(bson const&) const = 0;
    [[nodiscard]] virtual lookup_result<bson, document> lookup_one(bson const&) = 0;
    [[nodiscard]] virtual lookup_result<bson, document const> lookup_one(bson const&) const = 0;
    [[nodiscard]] virtual cursor lookup_if(function_ref<bool(bson const&)>) = 0;
    [[nodiscard]] virtual const_cursor lookup_if(function_ref<bool(bson const&)>) const = 0;
    virtual std::size_t erase(bson const&) = 0;
    virtual std::size_t erase_if(function_ref<bool(bson const&)>) = 0;
    [[nodiscard]] virtual function_ref<bool(bson const&)> value_filter() const noexcept = 0;
    [[nodiscard]] virtual sf_index_cursor iterate() = 0;
    [[nodiscard]] virtual sf_index_const_cursor iterate() const = 0;
    virtual ~_single_field_index_interface() = default;
};

struct single_field_unique_index_interface : public _single_field_index_interface {
    virtual ~single_field_unique_index_interface() = default;
};

struct single_field_multi_index_interface : public _single_field_index_interface {
    [[nodiscard]] virtual cursor lookup_many(bson const&) = 0;
    [[nodiscard]] virtual const_cursor lookup_many(bson const&) const = 0;
    virtual bool erase(bson const&, non_null_ptr<document const> const) = 0;
    virtual ~single_field_multi_index_interface() = default;
};

struct _compound_index_interface : public _base_index_interface {
    virtual index_insert_result insert(span<bson const>, non_null_ptr<document> const) = 0;
    virtual index_insert_result insert(span<non_null_ptr<bson const>>, non_null_ptr<document> const) = 0;
    // [[nodiscard]] virtual bool contains(span<bson const> const) const = 0;
    // [[nodiscard]] virtual bool conatins(span<non_null_ptr<bson const>> const) const = 0;
    [[nodiscard]] virtual lookup_result<span<bson const>, document> lookup_one(span<bson const>) = 0;
    [[nodiscard]] virtual lookup_result<span<bson const>, document const> lookup_one(span<bson const>) const = 0;
    [[nodiscard]] virtual cursor lookup_if(function_ref<bool(span<bson const>)>) = 0;
    [[nodiscard]] virtual const_cursor lookup_if(function_ref<bool(span<bson const>)>) const = 0;
    virtual std::size_t erase(span<bson const>) = 0;
    virtual std::size_t erase(span<non_null_ptr<bson const>> const) = 0;
    virtual std::size_t erase_if(function_ref<bool(span<bson const>)>) = 0;
    [[nodiscard]] virtual function_ref<bool(span<bson const>)> value_filter() const noexcept = 0;
    [[nodiscard]] virtual cmp_index_cursor iterate() = 0;
    [[nodiscard]] virtual cmp_index_const_cursor iterate() const = 0;
    virtual ~_compound_index_interface() = default;
};

struct compound_unique_index_interface : public _compound_index_interface {
    virtual ~compound_unique_index_interface() = default;
};

struct compound_multi_index_interface : public _compound_index_interface {
    [[nodiscard]] virtual cursor lookup_many(span<bson const>) = 0;
    [[nodiscard]] virtual const_cursor lookup_many(span<bson const>) const = 0;
    virtual bool erase(span<non_null_ptr<bson const>> const, non_null_ptr<document const> const) = 0;
    virtual bool erase(span<bson const>, non_null_ptr<document const> const) = 0;
    virtual ~compound_multi_index_interface() = default;
};

//
// index concrete class implementations:
//
template<template<class...> class MapT, class Filter>
class basic_single_field_unique_index final : public single_field_unique_index_interface, private detail::filter_wrapper<Filter> {
    MapT<bson, non_null_ptr<document>> map_{};
    using map_iter_t = decltype(map_.begin());
    using const_map_iter_t = decltype(map_.cbegin());
public:
    basic_single_field_unique_index() = default;
    
    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, basic_single_field_unique_index>, int> = 0>
    basic_single_field_unique_index(Fn&& fn)
        : detail::filter_wrapper<Filter>(std::forward<Fn>(fn))
    {}

    ~basic_single_field_unique_index() = default;

    [[nodiscard]] bool contains(bson const& val) const final {
        return map_.contains(val);
    }

    [[nodiscard]] bool contains_doc(non_null_ptr<document const> const doc) const final {
        for (auto&& [val, doc_ptr] : map_) {
            if (doc == doc_ptr)
                return true;
        }
        return false;
    }

    index_insert_result insert(bson const& val, non_null_ptr<document> const doc) final {
        if (this->filter(val)) {
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
        std::vector<non_null_ptr<document>> vec;
        for (auto&& [k, v] : map_) {
            if (fn(k))
                vec.push_back(v);
        }
        if (vec.empty())
            return zero_index_lookup<document>;
        else if (vec.size() == 1)
            return single_index_lookup{vec.front()};
        else
            return multiple_index_lookup_vec{std::move(vec)};
    }

    [[nodiscard]] const_cursor lookup_if(function_ref<bool(bson const&)> fn) const final {
        std::vector<non_null_ptr<document const>> vec;
        for (auto&& [k, v] : map_) {
            if (fn(k))
                vec.push_back(v);
        }
        if (vec.empty())
            return zero_index_lookup<document const>;
        else if (vec.size() == 1)
            return single_index_lookup{vec.front()};
        else
            return multiple_index_lookup_vec{std::move(vec)};
    }

    std::size_t erase(bson const& val) final {
        return map_.erase(val);
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

    void clear() final { map_.clear(); }

    [[nodiscard]] function_ref<bool(bson const&)> value_filter() const noexcept final {
        return static_cast<Filter const&>(*this);
    }

    [[nodiscard]] sf_index_cursor iterate() final {
        return multiple_index_lookup_iter<typename sf_index_cursor::value_type, 
            detail::sf_cursor_deref, map_iter_t>{map_.begin(), map_.end()};
    }

    [[nodiscard]] sf_index_const_cursor iterate() const final {
        return multiple_index_lookup_iter<typename sf_index_const_cursor::value_type, 
            detail::sf_const_cursor_deref, const_map_iter_t>{map_.cbegin(), map_.cend()};
    }
};

template<template<class...> class MapT, class Filter>
class basic_single_field_multi_index final : public single_field_multi_index_interface, private detail::filter_wrapper<Filter> {
    MapT<bson, non_null_ptr<document>> map_{};
    using map_iter_t = decltype(map_.begin());
    using const_map_iter_t = decltype(map_.cbegin());
public:
    basic_single_field_multi_index() = default; 
    ~basic_single_field_multi_index() = default;

    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, basic_single_field_multi_index>, int> = 0>
    basic_single_field_multi_index(Fn&& fn)
        : detail::filter_wrapper<Filter>(std::forward<Fn>(fn))
    {}

    [[nodiscard]] bool contains(bson const& val) const final {
        return map_.contains(val);
    }

    [[nodiscard]] bool contains_doc(non_null_ptr<document const> const doc) const final {
        for (auto&& [val, doc_ptr] : map_) {
            if (doc == doc_ptr)
                return true;
        }
        return false;
    }

    index_insert_result insert(bson const& val, non_null_ptr<document> const doc) final {
        if (this->filter(val)) {
            map_.emplace(std::make_pair(val, doc));
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
            return multiple_index_lookup_iter<document&, detail::deref_map_iter_second, map_iter_t>{first, last};
        return zero_index_lookup<document>;
    }

    [[nodiscard]] const_cursor lookup_many(bson const& val) const final {
        if (auto const [first, last] = map_.equal_range(val); first != map_.end())
            return multiple_index_lookup_iter<document const&, detail::deref_map_iter_second, const_map_iter_t>{first, last};
        return zero_index_lookup<document const>;
    }

    [[nodiscard]] cursor lookup_if(function_ref<bool(bson const&)> fn) final {
        std::vector<non_null_ptr<document>> vec;
        for (auto&& [k, v] : map_) {
            if (fn(k))
                vec.push_back(v);
        }

        if (vec.empty())
            return zero_index_lookup<document>;
        else if (vec.size() == 1)
            return single_index_lookup{vec.front()};
        else
            return multiple_index_lookup_vec{std::move(vec)};
    }

    [[nodiscard]] const_cursor lookup_if(function_ref<bool(bson const&)> fn) const final {
        std::vector<non_null_ptr<document const>> vec;
        for (auto&& [k, v] : map_) {
            if (fn(k))
                vec.push_back(v);
        }

        if (vec.empty())
            return zero_index_lookup<document const>;
        else if (vec.size() == 1)
            return single_index_lookup{vec.front()};
        else
            return multiple_index_lookup_vec{std::move(vec)};
    }

    std::size_t erase(bson const& val) final {
        return map_.erase(val);
    }

    bool erase(bson const& val, non_null_ptr<document const> const doc) final {
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

    void clear() final { map_.clear(); }

    [[nodiscard]] function_ref<bool(bson const&)> value_filter() const noexcept final {
        return static_cast<Filter const&>(*this);
    }

    [[nodiscard]] sf_index_cursor iterate() final {
        return multiple_index_lookup_iter<typename sf_index_cursor::value_type, 
            detail::sf_cursor_deref, map_iter_t>{map_.begin(), map_.end()};
    }

    [[nodiscard]] sf_index_const_cursor iterate() const final {
        return multiple_index_lookup_iter<typename sf_index_const_cursor::value_type, 
            detail::sf_const_cursor_deref, const_map_iter_t>{map_.cbegin(), map_.cend()};
    }
};

template<template<class...> class MapT, std::size_t N, class Func, class Filter>
class basic_compound_unique_index final : public compound_unique_index_interface, private detail::filter_wrapper<Filter> {
    MapT<std::array<bson, N>, non_null_ptr<document>, Func> map_;
    using map_iter_t = decltype(map_.begin());
    using const_map_iter_t = decltype(map_.cbegin());
public:
    basic_compound_unique_index() = default;
    ~basic_compound_unique_index() = default;

    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, basic_compound_unique_index>, int> = 0>
    basic_compound_unique_index(Fn&& fn)
        : detail::filter_wrapper<Filter>(std::forward<Fn>(fn))
    {}

    [[nodiscard]] bool contains_doc(non_null_ptr<document const> const doc) const final {
        for (auto&& [fields, doc_ptr] : map_) {
            if (doc == doc_ptr)
                return true;
        }
        return false;
    }

    index_insert_result insert(span<bson const> vals, non_null_ptr<document> const doc) final {
        DEBUG_ASSERT(vals.size() == N);
        if (this->filter(vals)) {
            auto const result = map_.try_emplace(span_to_array<bson, N>(vals), doc);
            return result.second ? index_insert_result::success : index_insert_result::already_exists;
        }
        return index_insert_result::filter_failed;
    }

    index_insert_result insert(span<non_null_ptr<bson const>> vals, non_null_ptr<document> const doc) final {
        DEBUG_ASSERT(vals.size() == N);
        if (this->filter(vals)) {
            auto const result = map_.try_emplace(span_to_array_deref<bson, N>(vals), doc);
            return result.second ? index_insert_result::success : index_insert_result::already_exists;
        }
        return index_insert_result::filter_failed;
    }

    [[nodiscard]] lookup_result<span<bson const>, document> lookup_one(span<bson const> const s) final {
        if (auto const found = map_.find(s); found != map_.end())
            return {found->first, *(found->second)};
        return {};
    }

    [[nodiscard]] lookup_result<span<bson const>, document const> lookup_one(span<bson const> const s) const final {
        if (auto const found = map_.find(s); found != map_.end())
            return {found->first, *(found->second)};
        return {};
    }

    [[nodiscard]] cursor lookup_if(function_ref<bool(span<bson const>)> fn) final {
        std::vector<non_null_ptr<document>> docs;
        for (auto&& [k, v] : map_) 
            if (fn(k))
                docs.push_back(v);
        if (docs.empty())
            return zero_index_lookup<document>;
        else if (docs.size() == 1)
            return single_index_lookup{docs.front()};
        else
            return multiple_index_lookup_vec{std::move(docs)};
    }

    [[nodiscard]] const_cursor lookup_if(function_ref<bool(span<bson const>)> fn) const final {
        std::vector<non_null_ptr<document const>> docs;
        for (auto&& [k, v] : map_) 
            if (fn(k))
                docs.push_back(v);
        if (docs.empty())
            return zero_index_lookup<document const>;
        else if (docs.size() == 1)
            return single_index_lookup{docs.front()};
        else
            return multiple_index_lookup_vec{std::move(docs)};
    }

    std::size_t erase(span<bson const> s) final {
        DEBUG_ASSERT(s.size() == N);
        return map_.erase(span_to_array<bson, N>(s));
    }

    std::size_t erase(span<non_null_ptr<bson const>> const s) final {
        DEBUG_ASSERT(s.size() == N);
        return map_.erase(span_to_array_deref<bson, N>(s));
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

    [[nodiscard]] std::size_t field_count() const noexcept final { return N; }

    void clear() final { map_.clear(); }

    [[nodiscard]] function_ref<bool(span<bson const>)> value_filter() const noexcept final {
        return static_cast<Filter const&>(*this);
    }

    [[nodiscard]] cmp_index_cursor iterate() final {
        return multiple_index_lookup_iter<typename cmp_index_cursor::value_type, 
            detail::cmp_cursor_deref, map_iter_t>{map_.begin(), map_.end()};
    }

    [[nodiscard]] cmp_index_const_cursor iterate() const final {
        return multiple_index_lookup_iter<typename cmp_index_const_cursor::value_type, 
            detail::cmp_const_cursor_deref, const_map_iter_t>{map_.cbegin(), map_.cend()};
    }
};

template<template<class...> class MapT, std::size_t N, class Func, class Filter>
class basic_compound_multi_index final : public compound_multi_index_interface, private detail::filter_wrapper<Filter> {
    MapT<std::array<bson, N>, non_null_ptr<document>, Func> map_;
    using map_iter_t = decltype(map_.begin());
    using const_map_iter_t = decltype(map_.cbegin());
public: 
    basic_compound_multi_index() = default;
    ~basic_compound_multi_index() = default;

    template<class Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, basic_compound_multi_index>, int> = 0>
    basic_compound_multi_index(Fn&& fn)
        : detail::filter_wrapper<Filter>(std::forward<Fn>(fn))
    {}

    [[nodiscard]] bool contains_doc(non_null_ptr<document const> const doc) const final {
        for (auto&& [fields, doc_ptr] : map_) {
            if (doc == doc_ptr)
                return true;
        }
        return false;
    }

    index_insert_result insert(span<bson const> vals, non_null_ptr<document> const doc) final {
        DEBUG_ASSERT(vals.size() == N);
        if (this->filter(vals)) {
            map_.emplace(std::make_pair(span_to_array<bson, N>(vals), doc));
            return index_insert_result::success;
        }
        return index_insert_result::filter_failed;
    }

    index_insert_result insert(span<non_null_ptr<bson const>> vals, non_null_ptr<document> const doc) final {
        DEBUG_ASSERT(vals.size() == N);
        if (this->filter(vals)) {
            map_.emplace(std::make_pair(span_to_array_deref<bson, N>(vals), doc));
            return index_insert_result::success;
        }
        return index_insert_result::filter_failed;
    }

    [[nodiscard]] lookup_result<span<bson const>, document> lookup_one(span<bson const> const vals) final {
        if (auto const found = map_.find(vals); found != map_.end())
            return {found->first, *(found->second)};
        return {};
    }

    [[nodiscard]] lookup_result<span<bson const>, document const> lookup_one(span<bson const> const vals) const final {
        if (auto const found = map_.find(vals); found != map_.end())
            return {found->first, *(found->second)};
        return {};
    }

    [[nodiscard]] cursor lookup_many(span<bson const> const vals) final {
        if (auto const [first, last] = map_.equal_range(vals); first != map_.end())
            return multiple_index_lookup_iter<document&, detail::deref_map_iter_second, map_iter_t>{first, last};
        return zero_index_lookup<document>;
    }

    [[nodiscard]] const_cursor lookup_many(span<bson const> const vals) const final {
        if (auto const [first, last] = map_.equal_range(vals); first != map_.end())
            return multiple_index_lookup_iter<document const&, detail::deref_map_iter_second, const_map_iter_t>{first, last};
        return zero_index_lookup<document const>;
    }

    [[nodiscard]] cursor lookup_if(function_ref<bool(span<bson const>)> fn) final {
        std::vector<non_null_ptr<document>> docs;
        for (auto&& [k, v] : map_) 
            if (fn(k))
                docs.push_back(v);
        if (docs.empty())
            return zero_index_lookup<document>;
        else if (docs.size() == 1)
            return single_index_lookup{docs.front()};
        else
            return multiple_index_lookup_vec{std::move(docs)};
    }

    [[nodiscard]] const_cursor lookup_if(function_ref<bool(span<bson const>)> fn) const final {
        std::vector<non_null_ptr<document const>> docs;
        for (auto&& [k, v] : map_) 
            if (fn(k))
                docs.push_back(v);
        if (docs.empty())
            return zero_index_lookup<document const>;
        else if (docs.size() == 1)
            return single_index_lookup{docs.front()};
        else
            return multiple_index_lookup_vec{std::move(docs)};
    }

    std::size_t erase(span<bson const> s) final { // todo maybe take span<bson*> to avoid copy
        DEBUG_ASSERT(s.size() == N);
        return map_.erase(span_to_array<bson, N>(s)); // return array<bson*> ????
    }

    std::size_t erase(span<non_null_ptr<bson const>> const s) final {
        DEBUG_ASSERT(s.size() == N);
        return map_.erase(span_to_array_deref<bson, N>(s));
    }

    bool erase(span<bson const> const vals, non_null_ptr<document const> const doc) final {
        if (auto [first, last] = map_.equal_range(vals); first != map_.end()) {
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

    bool erase(span<non_null_ptr<bson const>> const vals, non_null_ptr<document const> const doc) final {
        if (auto [first, last] = map_.equal_range(vals); first != map_.end()) {
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
        return static_cast<Filter const&>(*this);
    }

    [[nodiscard]] cmp_index_cursor iterate() final {
        return multiple_index_lookup_iter<typename cmp_index_cursor::value_type, 
            detail::cmp_cursor_deref, map_iter_t>{map_.begin(), map_.end()};
    }

    [[nodiscard]] cmp_index_const_cursor iterate() const final {
        return multiple_index_lookup_iter<typename cmp_index_const_cursor::value_type, 
            detail::cmp_const_cursor_deref, const_map_iter_t>{map_.cbegin(), map_.cend()};
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

template<class Filter = detail::no_filter>
using ordered_single_field_unique_index = basic_single_field_unique_index<absl::btree_map, Filter>;

template<class Filter = detail::no_filter>
using ordered_single_field_multi_index = basic_single_field_multi_index<absl::btree_multimap, Filter>;

template<std::size_t N, class Filter = detail::no_filter>
using ordered_compound_unique_index = basic_compound_unique_index<absl::btree_map, N, detail::compound_index_key_cmp, Filter>;

template<std::size_t N, class Filter = detail::no_filter>
using ordered_compound_multi_index = basic_compound_multi_index<absl::btree_multimap, N, detail::compound_index_key_cmp, Filter>;

} // namespace nova

#endif // NOVA_INDEX_HPP