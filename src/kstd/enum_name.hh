#pragma once

#include "basic.hh"
#include "string.hh"

// Slap @enum_to_string on an Enum type and it will automatically
// generate a constexpr enum_to_string(E) function for you.

template <typename E>
constexpr auto enum_to_string(E) -> string { return string(); }
