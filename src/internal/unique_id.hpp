#ifndef NOVA_UNIQUE_ID_HPP
#define NOVA_UNIQUE_ID_HPP

#include "../debug.hpp"
#include <fmt/format.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <random>

namespace nova {

struct uint40_t {
    char buf_[5];
    inline static constexpr auto max = std::numeric_limits<std::uint64_t>::max() >> 24;
};

struct uint24_t {
    char buf_[3];
    inline static constexpr auto max = std::numeric_limits<std::uint32_t>::max() >> 8;
};

#pragma pack(push, 1)
class unique_id {
    using dur_t = std::chrono::duration<std::uint32_t>;
    using tp_t = std::chrono::time_point<std::chrono::system_clock, dur_t>;
    struct hash {
        uint40_t rand;
        uint24_t count;
    };
    
    static_assert(sizeof(hash) == sizeof(std::uint64_t));

    tp_t time_{};
    union {
        hash hash_;
        std::uint64_t u64_;
    };

    inline static std::atomic<std::uint32_t> counter{0};

    template<class Clock, class Dur>
    explicit unique_id(std::chrono::time_point<Clock, Dur> const& a, std::uint64_t const b, std::uint32_t const c) noexcept 
        : time_(std::chrono::time_point_cast<dur_t>(a)), hash_{}
    {
        std::memcpy(&hash_.rand, reinterpret_cast<char const*>(&b) + 3, sizeof(hash_.rand));
        std::memcpy(&hash_.count, reinterpret_cast<char const*>(&c) + 1, sizeof(hash_.count));
    }

public:
    constexpr unique_id() noexcept : u64_{} {}
    
    constexpr unique_id(unique_id const& other) noexcept
        : time_(other.time_)
        , u64_(other.u64_)
    {}
    
    constexpr unique_id(unique_id&& other) noexcept
        : time_(other.time_)
        , u64_(other.u64_)
    {}

    constexpr unique_id& operator=(unique_id const& other) noexcept {
        time_ = other.time_;
        u64_ = other.u64_;
        return *this;
    }

    constexpr unique_id& operator=(unique_id&& other) noexcept {
        time_ = other.time_;
        u64_ = other.u64_;
        return *this;
    }

    [[nodiscard]] static unique_id generate() {
        thread_local std::random_device rd{};
        thread_local std::mt19937_64 engine{rd()};
        thread_local std::uniform_int_distribution<std::uint64_t> device{1, uint40_t::max};
        return unique_id{std::chrono::system_clock::now(), device(engine), counter.fetch_add(1, std::memory_order_relaxed)};
    }

    constexpr bool operator==(unique_id const& other) const noexcept {
        return time_ == other.time_ && u64_ == other.u64_;
    }

    constexpr bool operator<(unique_id const& other) const noexcept {
        return time_ < other.time_ && u64_ < other.u64_;
    }

    [[nodiscard]] constexpr auto time_point() const noexcept {
        return time_;
    }

    [[nodiscard]] constexpr std::size_t hash() const noexcept {
        return u64_;
    }

    [[nodiscard]] DEBUG_CXPR bool valid() const noexcept {
        return u64_ > 0 && time_ != tp_t{};
        [[maybe_unused]] auto check_count = [&] {
            std::uint32_t u = 0;
            std::memcpy(reinterpret_cast<char*>(&u) + 1, &hash_.count, sizeof(hash_.count));
            return u;
        };
        DEBUG_ASSERT(check_count() < counter.load(std::memory_order_relaxed));
    }
};
#pragma pack(pop)

} // namespace nova

template<>
struct fmt::formatter<nova::unique_id> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }
    
    template<class FmtCtx>
    auto format(nova::unique_id const& id, FmtCtx& ctx) {
        return fmt::format_to(ctx.out(), "{}{}", id.time_point().time_since_epoch().count(), id.hash());
    }
};

namespace std {
    template<>
    struct hash<nova::unique_id> {
        constexpr std::size_t operator()(nova::unique_id const& doc) const noexcept {
            return doc.hash();
        }
    };
}

#endif // NOVA_UNIQUE_ID_HPP