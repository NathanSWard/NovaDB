#ifndef NOVA_BSON_HPP
#define NOVA_BSON_HPP

// TODO: add rest of bson types

#include <fmt/format.h>
#include "detail.hpp"
#include "util/optional.hpp"
#include "unique_id.hpp"

#include <functional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace nova {

class document;

template<class T>
struct bson_type_t{};

template<class T>
inline static constexpr bson_type_t<T> bson_type{};

class bson {
public:
    using array_t = std::vector<bson>;
    struct null_t{
        constexpr bool operator==(null_t const&) const noexcept { return true; }
    };
    enum class types : std::uint8_t {
        UniqueID, Null, Bool, Int32, Int64, uInt32, uInt64, Float, Double, String, Array, Document
    };
private:
    std::variant<unique_id, null_t, bool, std::int32_t, std::int64_t, std::uint32_t, std::uint64_t, 
        float, double, std::string, array_t, detail::recursive_wrapper<document>> storage_;
    friend struct std::hash<bson>;
    friend struct fmt::formatter<bson>;

    template<class T>
    static constexpr bool is_valid_type = std::disjunction_v<std::is_same<T, unique_id>,
            std::is_same<T, null_t>, std::is_same<T, bool>, 
            std::is_same<T, std::int32_t>, std::is_same<T, std::int64_t>,
            std::is_same<T, std::uint32_t>, std::is_same<T, std::uint64_t>,
            std::is_same<T, float>, std::is_same<T, double>, 
            std::is_same<T, std::string>, std::is_same<T, bson::array_t>,
            std::is_same<T, document>>;

public:
    template<class T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, bson> && std::is_constructible_v<std::string, T>, int> = 0>
    bson(T&& str) : bson(bson_type<std::string>, std::forward<T>(str)) {}

    template<class T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, bson> && !std::is_constructible_v<std::string, T>, int> = 0>
    bson(T&& t) : bson(bson_type<T>, std::forward<T>(t)) {}

    template<class T, class... Args>
    bson(bson_type_t<T>, Args&&... args) 
        : storage_(std::in_place_type<detail::check_recursive_t<std::decay_t<T>, document>>, std::forward<Args>(args)...)
    {
        static_assert(is_valid_type<std::remove_cv_t<std::remove_reference_t<T>>>);
    }

    bson(bson&&) = default;
    bson(bson const&) = default;
    bson& operator=(bson&&) = default;
    bson& operator=(bson const&) = default;

    [[nodiscard]] constexpr types type() const noexcept {
        return static_cast<types>(storage_.index());
    }

    template<class T>
    [[nodiscard]] optional<T&> as() noexcept {
        static_assert(is_valid_type<T>);
        if (auto const ptr = std::get_if<T>(&storage_); ptr)
            return {*ptr};
        return {};
    }

    template<class T>
    [[nodiscard]] optional<T const&> as() const noexcept {
        static_assert(is_valid_type<T>);
        if (auto const ptr = std::get_if<T>(&storage_); ptr)
            return {*ptr};
        return {};
    }

    template<class T>
    [[nodiscard]] bool equals_strong(T const& t) const noexcept {
        static_assert(is_valid_type<T>);
        if (auto const val = as<T>(); val)
            return *val == t;
        return false;
    }

    template<class T>
    [[nodiscard]] constexpr bool equals_weak(T const& t) const noexcept {
        if constexpr (std::is_arithmetic_v<T>) {
            switch (type()) {
                case types::Bool: return std::get<bool>(storage_) == t;
                case types::Int32: return std::get<std::int32_t>(storage_) == t;
                case types::Int64: return std::get<std::int64_t>(storage_) == t;
                case types::uInt32: return std::get<std::uint32_t>(storage_) == t;
                case types::uInt64: return std::get<std::uint64_t>(storage_) == t;
                case types::Float: return std::get<float>(storage_) == t;
                case types::Double: return std::get<double>(storage_) == t;
                default: return false;
            }
        }
        else if constexpr (std::is_same_v<unique_id, T>) {
            switch(type()) {
                case types::UniqueID: return std::get<unique_id>(storage_) == t;
                default: return false;
            }
        }
        else if constexpr (std::is_same_v<null_t, T>) {
            switch(type()) {
                case types::Null: return true;
                default: return false;
            }
        }
        else if constexpr (std::is_same_v<array_t, T>) {
            switch (type()) {
                case types::Array: return std::get<array_t>(storage_) == t;
                default: return false;
            }
        }
        else if constexpr (std::is_same_v<document, T>) {
            switch (type()) {
                case types::Document: return std::get<detail::recursive_wrapper<document>>(storage_).get() == t;
                default: return false;
            }
        }
        else if constexpr (detail::is_string_comparable_v<T>) {
            switch (type()) {
                case types::String: return std::get<std::string>(storage_) == t;
                default: return false;
            }
        }
        else {
            static_assert(detail::always_false<T>::value);
            return false;
        }
    }

    constexpr bool operator==(bson const& other) const noexcept {
        return storage_ == other.storage_;
    }

    constexpr bool operator<(bson const& other) const noexcept {
        DEBUG_ASSERT(type() == other.type());
        std::visit(detail::overloaded{
            [](null_t) { DEBUG_ASSERT(false); return true; },
            [](detail::recursive_wrapper<document> const&) { DEBUG_ASSERT(false); return true; },
            [&other](auto&& val) { 
                return val < std::get<std::remove_cv_t<std::remove_reference_t<decltype(val)>>>(other.storage_); 
            }
        }, storage_);
    }
};

} // namespace nova

template<>
struct fmt::formatter<nova::bson> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

    template<class FmtCtx>
    auto format(nova::bson const& b, FmtCtx& ctx) {
        return std::visit(nova::detail::overloaded{
                [&](nova::bson::null_t){
                    return fmt::format_to(ctx.out(), "null");
                }, 
                [&](nova::bson::array_t const& arr){
                    fmt::format_to(ctx.out(), "[");
                    auto first = true;
                    for (auto&& val : arr) {
                        if (!first)
                            fmt::format_to(ctx.out(), ", ");
                        fmt::format_to(ctx.out(), "{}", val);
                        first = false;
                    }
                    return fmt::format_to(ctx.out(), "]");
                }, 
                [&](nova::detail::recursive_wrapper<nova::document> const& doc){
                    return fmt::format_to(ctx.out(), "{}", doc.get());
                },
                [&](std::string const& str) {
                    return fmt::format_to(ctx.out(), "{}", str);
                }, 
                [&](nova::unique_id const& id) {
                    return fmt::format_to(ctx.out(), "{}", id);
                },
                [&](bool const b) {
                    return fmt::format_to(ctx.out(), "{}", b);
                },
                [&](auto&& val) {
                    return fmt::format_to(ctx.out(), "{}", val);
                }
            }, b.storage_);
    }
};

namespace std {
    template<>
    struct hash<nova::bson> {
        constexpr ::std::size_t operator()(nova::bson const& val) const noexcept {
            return ::std::visit(nova::detail::overloaded{
                [](nova::bson::null_t){
                    DEBUG_ASSERT(false);
                    return static_cast<::std::size_t>(0);
                }, 
                [](nova::bson::array_t const&){
                    DEBUG_ASSERT(false);
                    return static_cast<::std::size_t>(0);
                }, 
                [](nova::detail::recursive_wrapper<nova::document> const&){
                    DEBUG_ASSERT(false);
                    return static_cast<::std::size_t>(0);
                },
                [](::std::string const& str) {
                    return ::std::hash<::std::string>{}(str);
                }, 
                [](nova::unique_id const& id) {
                    return id.hash();
                },
                [](auto&& val) {
                    return static_cast<size_t>(val);
                }
            }, val.storage_);
        }
    };
} // namespace std

#endif // NOVA_BSON_HPP

