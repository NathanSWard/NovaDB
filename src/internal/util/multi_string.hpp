#ifndef NOVA_MULTI_STRING_HPP
#define NOVA_MULTI_STRING_HPP

#include "../detail.hpp"
#include "../../debug.hpp"

#include <numeric>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace nova {

// A multi_string is a container which can hold 'N' constant strings in a single allocation.
class multi_string {
    char* storage_ = nullptr;
    std::size_t size_ = 0;
    
    // pointer to the metadata (char pointer array)
    char const* const* ptrs_ptr() const noexcept { return reinterpret_cast<char const* const*>(storage_); }
    char const** ptrs_ptr() noexcept { return reinterpret_cast<char const**>(storage_); }

    void copy_from_other(multi_string const& other) {
        // allocate new storage
        auto const size_of_ptrs = sizeof(char**) * (size_ + 1);
        auto const size_of_strs = other.total_str_sizes();
        storage_ = new char[size_of_ptrs + size_of_strs];

        // assign pointer values
        *ptrs_ptr() = reinterpret_cast<char const*>(ptrs_ptr() + size_of_ptrs);
        for (std::size_t i = 0; i < size_ + 1; ++i)
            ptrs_ptr()[i + 1] = ptrs_ptr()[i] + (other.ptrs_ptr()[i + 1] - other.ptrs_ptr()[i]);

        // copy over characters
        std::memcpy(ptrs_ptr() + size_of_ptrs, other.ptrs_ptr() + size_of_ptrs, size_of_strs);
    }

public:
    // default constructor
    constexpr multi_string() noexcept = default;

    // constructor
    // params: N string-like values (must be able to construct a std::string_view)
    template<class... Views, std::enable_if_t<(sizeof...(Views) > 1) || 
        (std::is_same_v<std::decay_t<Views>, multi_string> || ...), int> = 0>
    explicit multi_string(Views const&... views)
        : size_(sizeof...(Views))
    {
        static_assert(std::conjunction_v<std::is_constructible<std::string_view, Views>...>);

        // create tuple of string_views
        auto const sv_tpl = std::make_tuple(std::string_view{views}...);

        // allocate storage
        std::size_t const size_of_strs = std::apply([](auto&&... sv) { return (sv.size() + ...); }, sv_tpl);
        constexpr std::size_t size_of_ptrs = (sizeof...(Views) + 1) * sizeof(char**); 
        storage_ = new char[size_of_ptrs + size_of_strs];

        char** ptrs_ptr = reinterpret_cast<char**>(storage_);
        char* strs_ptr = reinterpret_cast<char*>(ptrs_ptr + size_of_ptrs);

        // assign pointer value and copy string characters
        auto create_strings = [&](std::string_view const view) {
            *ptrs_ptr = strs_ptr;
            std::copy(view.begin(), view.end(), strs_ptr);
            strs_ptr += view.size();
            ++ptrs_ptr;
        };

        std::apply([&](auto&&... sv) { (create_strings(sv), ...); }, sv_tpl);
        *ptrs_ptr = strs_ptr; // past the end ptr
    }

    // move constructor
    multi_string(multi_string&& other) noexcept
        : storage_(std::exchange(other.storage_, nullptr))
        , size_(other.size_)
    {}

    // move assignment
    multi_string& operator=(multi_string&& other) noexcept {
        DEBUG_ASSERT(this != std::addressof(other));
        delete[] storage_;
        storage_ = std::exchange(other.storage_, nullptr);
        size_ = other.size_;
        return *this;
    }

    // copy constructor
    multi_string(multi_string const& other)
        : size_(other.size_)
    {
        copy_from_other(other);
    }

    // copy assignment
    multi_string& operator=(multi_string const& other) {
        if (this != std::addressof(other)) {
            delete[] storage_;
            size_ = other.size_;
            copy_from_other(other);
        }
        return *this;
    }

    // destructor
    ~multi_string() { delete[] storage_; }

    // index operator
    std::string_view operator[](std::size_t const pos) const noexcept {
        DEBUG_ASSERT(pos < size_);
        std::size_t const size = ptrs_ptr()[pos + 1] - ptrs_ptr()[pos];
        return {ptrs_ptr()[pos], size};
    }
    
    // return: number of strings stored
    constexpr std::size_t size() const noexcept { 
        return size_; 
    }

    // return: size of a specific string
    std::size_t str_size(std::size_t const pos) const noexcept {
        DEBUG_ASSERT(pos < size_);
        return ptrs_ptr()[pos + 1] - ptrs_ptr()[pos];
    }

    // reuturn: accumulated size of strings
    std::size_t total_str_sizes() const noexcept {
        return ptrs_ptr()[size_] - ptrs_ptr()[0];
    }

    struct sentinel{};
    class iterator {
        multi_string const* parent_;
        std::size_t pos_;
    public:
        constexpr iterator(multi_string const* parent, std::size_t pos = 0) noexcept 
            : parent_(parent) 
            , pos_(pos)
        {}

        std::string_view operator*() const noexcept {
            DEBUG_ASSERT(*this != multi_string::sentinel{}); 
            return parent_->operator[](pos_); 
        }

        iterator& operator++() noexcept { 
            ++pos_; 
            return *this; 
        }

        iterator operator++(int) noexcept {
            iterator it{parent_, pos_};
            ++pos_;
            return it;
        }

        bool operator==(iterator const& other) const noexcept {
            return parent_ == other.parent_ && pos_ == other.pos_;
        }

        bool operator!=(iterator const& other) const noexcept {
            return !(*this == other);
        }

        bool operator==(sentinel const) const noexcept {
            return pos_ >= parent_->size();
        }

        bool operator!=(sentinel const sent) const noexcept {
            return !(*this == sent);
        }
    };

    iterator begin() const noexcept { 
        return iterator{this}; 
    }

    constexpr sentinel end() const noexcept { 
        return sentinel{}; 
    }

    bool operator==(multi_string const& other) const noexcept {
        return size() != other.size() ? false : detail::equal(begin(), end(), other.begin());
    }

    bool operator!=(multi_string const& other) const noexcept {
        return !(*this == other);
    }
};

} // namespace nova

#endif // NOVA_MULTI_STRING_HPP