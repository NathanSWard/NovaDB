#ifndef NOVA_MULTI_STRING_HPP
#define NOVA_MULTI_STRING_HPP

#include "../detail.hpp"
#include "../../debug.hpp"

#include <string_view>
#include <vector>

namespace nova {

struct string_args_t {};
inline static constexpr string_args_t string_args{};

class multi_string {
    char* storage_ = nullptr;
    std::size_t size_ = 0;
public:
    constexpr multi_string() noexcept = default;

    template<class... Views>
    explicit multi_string(string_args_t, Views&&... views)
        : size_(sizeof...(Views))
    {
        static_assert(std::conjunction_v<std::is_constructible<std::string_view, Views>...>);
        constexpr auto size_of_positions = (sizeof...(Views) + 1) * sizeof(char**); 
        storage_ = new char[(std::string_view{views}.size() + ...) + size_of_positions];

        char** pos = reinterpret_cast<char**>(storage_);
        char* string = reinterpret_cast<char*>(reinterpret_cast<char**>(storage_) + size_of_positions);

        auto lambda = [&](std::string_view view) {
            *pos = string;
            std::memcpy(string, view.data(), view.size());
            string += view.size();
            ++pos;
        };

        (lambda(views), ...);
        *pos = string;
    }

    multi_string(multi_string const& other)
        : size_(other.size_)
    {
        auto const size_of_positions = sizeof(char**) * (size_ + 1);
        auto const size_of_chars = reinterpret_cast<char**>(other.storage_)[size_] - reinterpret_cast<char**>(other.storage_)[0];

        storage_ = new char[size_of_positions + size_of_chars];

        char const** pos = reinterpret_cast<char const**>(storage_);
        char const* str = reinterpret_cast<char const*>(reinterpret_cast<char**>(storage_) + size_of_positions);
        char const* const* other_start = reinterpret_cast<char const* const*>(other.storage_);
        char const* const* other_end = reinterpret_cast<char const* const*>(other.storage_) + 1;
        for (std::size_t i = 0; i < size_ + 1; ++i, ++pos, ++other_start, ++other_end) {
            *pos = str;
            str += (*other_end - *other_start);
        }
        std::memcpy(reinterpret_cast<char**>(storage_) + size_of_positions, reinterpret_cast<char**>(other.storage_) + size_of_positions, size_of_chars);
    }

    multi_string(multi_string&& other) noexcept
        : storage_(std::exchange(other.storage_, nullptr))
        , size_(other.size_)
    {}

    multi_string& operator=(multi_string&& other) noexcept {
        delete[] storage_;
        storage_ = std::exchange(other.storage_, nullptr);
        size_ = other.size_;
        return *this;
    }

    ~multi_string() { delete[] storage_; }

    std::string_view operator[](std::size_t const pos) const noexcept {
        DEBUG_ASSERT(pos < size_);
        std::size_t const size = reinterpret_cast<char const**>(storage_)[pos + 1] - 
            reinterpret_cast<char const**>(storage_)[pos];
        return std::string_view{reinterpret_cast<char**>(storage_)[pos], size};
    }

    std::size_t size() const noexcept { return size_; }

    struct sentinel{};

    class iterator {
        multi_string const* ref_;
        std::size_t pos_{0};
    public:
        constexpr iterator(multi_string const& ref) noexcept : ref_(std::addressof(ref)) {}
        auto operator*() { return ref_->operator[](pos_); }
        constexpr iterator& operator++() noexcept { ++pos_; return *this; }

        bool operator!=(sentinel const) const noexcept {
            return pos_ < ref_->size();
        }
    };

    auto begin() const noexcept { return iterator{*this}; }
    constexpr auto end() const noexcept { return sentinel{}; }

    bool operator==(multi_string const& other) const noexcept {
        return size() != other.size() ? false : detail::equal(begin(), end(), other.begin());
    }
    bool operator!=(multi_string const& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace nova

#endif // NOVA_MULTI_STRING_HPP