#ifndef NOVA_MULTI_STRING_HPP
#define NOVA_MULTI_STRING_HPP

#include "../../debug.hpp"

#include <string_view>
#include <vector>

class multi_string {
    char* storage_ = nullptr;
    std::size_t size_;
public:
    template<class... Views>
    multi_string(Views&&... views)
        : size_(sizeof...(Views))
    {
        constexpr auto size_of_positions = (sizeof...(Views) + 1) * sizeof(char const*); 
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

        static_assert(std::conjunction_v<std::is_constructible<std::string_view, Views>...>);
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

    class iter {
        multi_string const* ref_;
        std::size_t pos_{0};
    public:
        constexpr iter(multi_string const& ref) noexcept : ref_(std::addressof(ref)) {}
        auto operator*() { return ref_->operator[](pos_); }
        constexpr iter& operator++() noexcept { ++pos_; return *this; }

        bool operator!=(sentinel const) const noexcept {
            return pos_ < ref_->size();
        }
    };

    auto begin() const noexcept { return iter{*this}; }
    constexpr auto end() const noexcept { return sentinel{}; }

    bool operator==(multi_string const& other) const noexcept {
        if (size() != other.size()) 
            return false;
        for (std::size_t i = 0; i < size_; ++i) {
            if ((*this)[i] != other[i])
                return false;
        }
        return true;
    }
};

#endif // NOVA_MULTI_STRING_HPP