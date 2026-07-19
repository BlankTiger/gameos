#pragma once

#if defined(__GNUC__) || defined(__clang__)
#define force_inline inline __attribute__((always_inline))
#else
#define force_inline inline
#endif

#include "cstring.hh"
#include "int.hh"

// clang-format off
#include "kstd/format.hh"
#include "kstd/serial.hh"
#include "kstd/halt.hh"
#include "kstd/assert.hh"
#include "kstd/term.hh"
#include "kstd/enum_flags.hh"
#include "kstd/array.hh"
#include "kstd/memory.hh"
#include "kstd/gfx.hh"
#include "operator_new.hh"
// clang-format on
