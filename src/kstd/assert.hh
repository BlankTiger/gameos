#pragma once

#include <source_location>

#include "../basic.hh"

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstdio>
#include <cstdlib>

constexpr force_inline auto kstd_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (predicate) return;
    std::fprintf(
        stderr, "%s:%u:%u: %s\n",
        location.file_name(), location.line(), location.column(),
        message != nullptr ? message : "assertion failed"
    );
    std::abort();
}
#else
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

constexpr force_inline auto kstd_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (!predicate) halt_forever(message, location);
}
#endif

constexpr force_inline auto kstd_debug_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
#ifdef NDEBUG
    return;
#else
    kstd_assert(predicate, message, location);
#endif
}
