#pragma once

#ifdef NDEBUG

#define DEBUG_ASSERT(...) static_cast<void>(0)
#define DEBUG_CXPR constexpr
#define DEBUG_THROW(...) static_cast<void>(0)

#else // NDEBUG

#include <cassert>

#define DEBUG_ASSERT(b) assert(b) 
#define DEBUG_CXPR
#define DEBUG_THROW(x) throw x

#endif // NDEBUG