#pragma once

#include "numbers.hh"

#if defined(__GNUC__) || defined(__clang__)
#define force_inline inline __attribute__((always_inline))
#else
#define force_inline inline
#endif

// Extent value meaning "Array_View's size is only known at runtime"
// (as opposed to a compile-time N). Declared here rather than in
// array.hh since string_view.hh needs it for a forward declaration
// before array.hh itself is reachable (see string_view.hh comment).
inline constexpr usize DYNAMIC_EXTENT = static_cast<usize>(-1);
