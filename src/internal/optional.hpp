#ifndef OPTIONAL_H
#define OPTIONAL_H

#include "../../deps/optional/optional.hpp"

namespace nova {

template<class T>
using optional = tl::optional<T>;

} // namespace nova

#endif // OPTIONAL_H