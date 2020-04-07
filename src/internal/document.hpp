#ifndef NOVA_DOCUMENT_HPP
#define NOVA_DOCUMENT_HPP

#include <fmt/format.h>
#include "bson.hpp"
#include "util/err_result.hpp"
#include "util/map_results.hpp"

#include <cstring>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nova {

enum class value_error : std::uint8_t {
    Missing, WrongType
};

using doc_id = bson;

class doc_values {
    using key_t = std::string;
    using value_t = bson;
    std::unordered_map<key_t, value_t> values_;

public:
    template<class K>
    [[nodiscard]] bool contains(K const& k) const {
        return values_.find(k) != values_.end();
    }

    template<class Key, class... Args>
    insert_result<key_t, value_t> insert(Key&& key, Args&&... args) {
        auto [iter, b] = values_.try_emplace(std::forward<Key>(key), std::forward<Args>(args)...);
        return {iter->first, iter->second, b};
    }

    template<class Key, class T, class... Args>
    insert_result<key_t, value_t> insert(Key&& key, bson_type_t<T>, Args&&... args) {
        auto [iter, b] = values_.try_emplace(std::forward<Key>(key), bson_type<T>, std::forward<Args>(args)...);
        return {iter->first, iter->second, b};
    }

    template<class T, class Key, class... Args>
    update_result<key_t, value_t> update(Key&& key, Args&&... args) {
        auto [iter, b] = values_.insert_or_assign(std::forward<Key>(key), bson{bson_type<T>, std::forward<Args>(args)...});
        return {iter->first, iter->seconds, b};
    }

    template<class Key, class T>
    update_result<key_t, value_t> update(Key&& key, T&& t) {
        auto [iter, b] = values_.insert_or_assign(std::forward<Key>(key), bson{std::forward<T>(t)});
        return {iter->first, iter->seconds, b};
    }

    template<class Key>
    [[nodiscard]] lookup_result<key_t, value_t> get(Key const& key) {
        if (auto const iter = values_.find(key); iter != values_.end())
            return {iter->first, iter->second};
        return {};
    }

    template<class Key>
    [[nodiscard]] lookup_result<key_t, value_t const> get(Key const& key) const {
        if (auto const iter = values_.find(key); iter != values_.end())
            return {iter->first, iter->second};
        return {};
    }

    template<class Key>
    [[nodiscard]] auto operator[](Key const& key) {
        return get(key);
    }

    template<class Key>
    [[nodiscard]] auto operator[](Key const& key) const {
        return get(key);
    }

    template<class T, class Key>
    [[nodiscard]] err_result<T&, value_error> get_as(Key const& key) {
        if (auto const iter = values_.find(key); iter != values_.end()) {
            if (auto opt = iter->second.template as<T>(); opt)
                return {*opt};
            return {err_tag, value_error::WrongType};
        }
        return {err_tag, value_error::Missing};
    }

    template<class T, class Key>
    [[nodiscard]] err_result<T const&, value_error> get_as(Key const& key) const {
        if (auto const iter = values_.find(key); iter != values_.end()) {
            if (auto const opt = iter->second.template as<T>(); opt)
                return {*opt};
            return {err_tag, value_error::WrongType};
        }
        return {err_tag, value_error::Missing};
    }

    [[nodiscard]] auto begin() noexcept { return values_.begin(); }
    [[nodiscard]] auto begin() const noexcept { return values_.begin(); }
    [[nodiscard]] auto end() noexcept { return values_.end(); }
    [[nodiscard]] auto end() const noexcept { return values_.end(); }

    bool operator==(doc_values const& other) const noexcept {
        return values_ == other.values_;
    }
};

class document {
    using key_t = std::string;
    using value_t = bson;
    doc_values values_{};
    doc_id id_;
public:

    template<class T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, document>, int> = 0>
    document(T&& t) : id_(std::forward<T>(t)) {}

    document(document const&) = default;
    document(document&&) = default;

    [[nodiscard]] auto const& id() const noexcept { return id_; }
    [[nodiscard]] auto& values() noexcept { return values_; }
    [[nodiscard]] auto const& values() const noexcept { return values_; }

    bool operator==(document const& other) const noexcept {
        return id_ == other.id_ && values_ == other.values_;
    }
};

using doc_lookup = lookup_result<doc_id, doc_values>;
using const_doc_lookup = lookup_result<doc_id, doc_values const>;
using doc_ref = valid_lookup<doc_id, doc_values>;
using const_doc_ref = valid_lookup<doc_id, doc_values const>;

} // namespace nova

template<class Vals>
struct fmt::formatter<nova::valid_lookup<nova::doc_id, Vals>> {
    constexpr auto parse(fmt::format_parse_context& ctx) const noexcept { return ctx.begin(); }

    template<class FmtCtx>
    auto format(nova::valid_lookup<nova::doc_id, Vals> const& doc, FmtCtx& ctx) const {
        fmt::format_to(ctx.out(), "{{\n  _id: {}", doc.key());
        for (auto&& [k, v] : doc.value())
            fmt::format_to(ctx.out(), ",\n  {}: {}", k, v);
        return fmt::format_to(ctx.out(), "\n}}");
    }
};

template<class Vals>
struct fmt::formatter<nova::lookup_result<nova::doc_id, Vals>> {
    constexpr auto parse(fmt::format_parse_context& ctx) const noexcept { return ctx.begin(); }

    template<class FmtCtx>
    auto format(nova::lookup_result<nova::doc_id, Vals> const& doc, FmtCtx& ctx) const {
        return doc ? fmt::format_to(ctx.out(), "{}", *doc) 
                   : fmt::format_to(ctx.out(), "document does not exist");
    }
};

template<>
struct fmt::formatter<nova::document> {
    constexpr auto parse(fmt::format_parse_context& ctx) const noexcept { return ctx.begin(); }

    template<class FmtCtx>
    auto format(nova::document const& doc, FmtCtx& ctx) const {
        return fmt::format_to(ctx.out(), "{}", nova::const_doc_ref{doc.id(), doc.values()});
    }
};

#endif // NOVA_DOCUMENT_HPP

