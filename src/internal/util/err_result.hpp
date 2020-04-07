#ifndef NOVA_ERR_RESULT_HPP
#define NOVA_ERR_RESULT_HPP

#include <type_traits>
#include <utility>

namespace nova {

template<class T, class E>
class err_result;

struct err_tag_t {};
inline static constexpr err_tag_t err_tag{};

template<class T, class E>
class err_result<T&, E> {

    union {
        T* val_ptr_;
        E err_;
    };
    bool is_ok_;

public:
    template<class U, std::enable_if_t<!std::is_same_v<std::decay_t<U>, err_result>, int> = 0>
    constexpr err_result(U&& u) 
        : val_ptr_(std::addressof(u))
        , is_ok_(true)
    {
        static_assert(std::is_lvalue_reference_v<U>);
    }

    template<class... Args>
    constexpr err_result(err_tag_t, Args&&... args)
        : err_(std::forward<Args>(args)...)
        , is_ok_(false)
    {}

    constexpr explicit operator bool() const noexcept { return is_ok_; }
    constexpr bool is_ok() const noexcept { return is_ok_; }
    constexpr bool is_err() const noexcept { return !is_ok_; }

    constexpr T& ok() const noexcept {
        return *val_ptr_;
    }

    constexpr E& err() & noexcept {
        return err_;
    }
    constexpr E const& err() const& noexcept {
        return err_;
    }
    constexpr E&& err() && noexcept {
        return std::move(err_);
    }
    constexpr E const&& err() const&& noexcept {
        return std::move(err_);
    }
};

} // namespace nova

#endif // NOVA_ERR_RESULT_HPP