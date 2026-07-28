#pragma once

#include "numbers.hh"

#if defined(__GNUC__) || defined(__clang__)
#define force_inline inline __attribute__((always_inline))
#else
#define force_inline inline
#endif

// Marks a header-defined function that must have exactly one
// definition across TUs (comdat/weak). Use instead of plain `inline` when
// a second TU would otherwise see a duplicate strong symbol.
#define kstd_h inline
