#pragma once

#include <array>
#include <type_traits>
#include <utility>

#include "../basic.hh"
#include "string.hh"

//
// Enum value name printing (e.g. "Direction::UP"), with no per-enum boilerplate:
// the qualified enumerator name is recovered at compile time from the compiler's
// __PRETTY_FUNCTION__ output for a template instantiated with that value.
//
// Only values within [Enum_Range<E>::min, Enum_Range<E>::max] are considered
// (default 0-255, i.e. any byte-sized enum works with no extra setup). Specialize
// Enum_Range for enums whose values fall outside that:
//
//   template <> struct Enum_Range<Big_Enum> { static constexpr s64 min = 0, max = 4095; };
//
// Values with no matching enumerator (out of range, or an in-range integer that
// just isn't a named case, i.e. a gap) come back as an empty String_View; format.hh
// falls back to printing the underlying integer in that case.
//

template <typename E>
struct Enum_Range {
    static constexpr s64 min = 0;
    static constexpr s64 max = 255;
};

namespace enum_name_detail {

template <typename E, E V>
constexpr auto value_name() -> String_View {
    const char* s  = __PRETTY_FUNCTION__;
    const char* eq = nullptr;
    for (const char* p = s; *p != '\0'; ++p) {
        if (*p == '=') eq = p;
    }
    if (eq == nullptr) return String_View();

    const char* start = eq + 2;  // skip "= "
    const char* end   = start;
    while (*end != ']') ++end;

    // Not a named enumerator, e.g. "(Direction)77".
    if (*start == '(') return String_View();

    return String_View(start, (usize)(end - start));
}

template <typename E, s64... Is>
constexpr auto make_table(std::integer_sequence<s64, Is...>) -> std::array<String_View, sizeof...(Is)> {
    return { value_name<E, (E)(Enum_Range<E>::min + Is)>()... };
}

}  // namespace enum_name_detail

template <typename E>
    requires std::is_enum_v<E>
constexpr auto enum_name(E value) -> String_View {
    using Range = Enum_Range<E>;
    constexpr auto table = enum_name_detail::make_table<E>(
        std::make_integer_sequence<s64, Range::max - Range::min + 1>{}
    );

    s64 v = (s64)value;
    if (v < Range::min || v > Range::max) return String_View();
    return table[(usize)(v - Range::min)];
}
