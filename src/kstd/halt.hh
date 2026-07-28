#pragma once

#include "basic.hh"
#include "assert.hh"

//
// Panic-time printing. Goes to serial plus every printer registered through
// halt_add_printer (see assert.cc), so a panic still shows up on screen once
// term is alive, and never depends on mem/gfx/term being alive.
//
// Only the templated print helpers live here; they cannot live in assert.cc.
// Everything non-template (halt_forever, the printer table, halt_put_char)
// is defined in assert.cc, which is what keeps assert.hh - and therefore
// string_view.hh, array.hh and friends - free of the printing stack.
//

#if !defined(__STDC_HOSTED__) || !__STDC_HOSTED__

#include "serial.hh"

namespace halt {

struct Halt_Printer_Backend {
    static auto put_char(char c) -> void {
        halt_put_char(c);
    }

    static auto new_line() -> void {
        halt_put_char('\n');
    }
};

inline Halt_Printer_Backend backend;

inline auto print(const char* format) -> int {
    return fmt::print(backend, format);
}

template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::print(backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

inline auto println() -> int {
    return fmt::println(backend);
}

template <typename T>
auto print(T&& value) -> int {
    return fmt::print(backend, std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    return fmt::println(backend, std::forward<T>(value));
}

inline auto println(const char* format) -> int {
    return fmt::println(backend, format);
}

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::println(backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

} // namespace halt

#endif
