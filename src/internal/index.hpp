#ifndef INDEX_H
#define INDEX_H

#include "bson.hpp"
#include "document.hpp"

#include <functional>
#include <map>

namespace nova {

template<template<class> class Comp = std::less>
using ordered_unique_index = std::map<bson, document*, Comp<bson>>;

template<template<class> class Comp = std::less>
using ordered_single_field_index = std::multimap<bson, document*, Comp<bson>>;

using unordered_unique_index = std::unordered_map<bson, document*>;
using unordered_single_field_index = std::unordered_multimap<bson, document*>;

} // namespace nova

#endif // INDEX_H