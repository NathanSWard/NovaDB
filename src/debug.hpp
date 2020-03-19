#pragma once

#ifdef NDEBUG

#define DEBUG_ASSERT(...) static_cast<void>(0)
#define DEBUG_CXPR constexpr

#else // NDEBUG

#include <cassert>

#define DEBUG_ASSERT(b) assert(b) 
#define DEBUG_CXPR

#endif // NDEBUG