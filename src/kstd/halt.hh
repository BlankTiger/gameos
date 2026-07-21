#pragma once

#include <source_location>

#include "serial.hh"

using Halt_Fn = auto (*)(const char* file, u32 line, u32 col, const char* message) -> void;
inline Halt_Fn __halt_print;

force_inline auto halt_set_print(Halt_Fn fn) -> void {
    __halt_print = fn;
}

static bool __panicking = false;

[[noreturn]] static auto
halt_forever(const char* message, const std::source_location& location = std::source_location::current()) -> void {
    if (__panicking) {
        for (;;) asm volatile("hlt");
    }
    __panicking = true;

    // Always safe: no dependency on mem/gfx/term.
    serial::print("%:%:%", location.file_name(), location.line(), location.column());
    if (message != nullptr) serial::println(": %", message);

    if (__halt_print) {
        __halt_print(location.file_name(), location.line(), location.column(), message);
    }

    for (;;) asm volatile("hlt");
}
