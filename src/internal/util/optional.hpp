#ifndef NOVA_OPTIONAL_HPP
#define NOVA_OPTIONAL_HPP

#include "../../../deps/optional/optional.hpp"

namespace nova {

template<class T>
using optional = tl::optional<T>;

} // namespace nova

#endif // NOVA_OPTIONAL_HPP