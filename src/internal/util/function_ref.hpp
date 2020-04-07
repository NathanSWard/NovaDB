#ifndef NOVA_FUNCTION_REF_HPP
#define NOVA_FUNCTION_REF_HPP

#include <type_traits>
#include <utility>

namespace nova {

template<typename Fn>
class function_ref;

template<typename R, typename ...Args>
class function_ref<R(Args...)> {
    R (*cb_)(std::intptr_t callable, Args... args) = nullptr;

    std::intptr_t ptr_ {};

    template<typename Fn>
    static R callback_fn(std::intptr_t callable, Args... args) {
        return (*reinterpret_cast<Fn*>(callable))(std::forward<Args>(args)...);
    }

public:
    constexpr function_ref() noexcept = default;

    constexpr explicit function_ref(std::nullptr_t) noexcept {}

    template<typename Fn, std::enable_if_t<!std::is_same_v<std::decay_t<Fn>, function_ref>, int> = 0>
    function_ref(Fn&& callable) noexcept
        : cb_(callback_fn<std::remove_reference_t<Fn>>)
        , ptr_(reinterpret_cast<std::intptr_t>(std::addressof(callable))) 
    {
        static_assert(std::is_invocable_r_v<R, Fn, Args...>);
    }

    R operator()(Args... args) const {
        return cb_(ptr_, std::forward<Args>(args)...);
    }

    constexpr explicit operator bool() const noexcept { return cb_; }
};

} // namespace nova

#endif // NOVA_FUNCTION_REF_HPP