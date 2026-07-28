#pragma once

#include <source_location>

#include "basic.hh"

//
// Bottom of the dependency stack: this header knows nothing but basic.hh, so
// *any* header (string_view.hh included) can assert without risking an include
// cycle. Messages are plain `const char*` here.
//
// In a freestanding build the panic machinery is only *declared* here and
// defined in assert.cc, which needs the whole printing stack
// (halt.hh -> serial.hh -> format.hh -> string_view.hh) to report a panic.
//

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstdio>
#include <cstdlib>

[[noreturn]] force_inline auto
halt_forever(const char* message, usize size, const std::source_location& location) -> void {
    std::fprintf(
        stderr, "%s:%u:%u: %.*s\n",
        location.file_name(), location.line(), location.column(),
        static_cast<int>(size), message
    );
    std::abort();
}

[[noreturn]] force_inline auto halt_forever(
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    const char* msg = message != nullptr ? message : "assertion failed";
    usize size = 0;
    while (msg[size] != '\0') ++size;
    halt_forever(msg, size, location);
}
#else

using Halt_Print_Fn = auto (*)(char) -> void;

// Sized overload: takes the message as pointer + length so this header stays
// independent of string_view (which needs to assert itself).
[[noreturn]] auto halt_forever(const char* message, usize size, const std::source_location& location) -> void;

[[noreturn]] auto halt_forever(
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void;

// Registers an extra sink for panic output (term does this once it can draw).
auto halt_add_printer(Halt_Print_Fn fn) -> void;

// Writes one character to serial and to every registered printer. Declared
// here so halt.hh's print helpers can reach it without seeing the printer
// table, which stays private to assert.cc.
auto halt_put_char(char c) -> void;

#endif

constexpr force_inline auto kstd_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (!predicate) halt_forever(message, location);
}

constexpr force_inline auto kstd_debug_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
#ifdef NDEBUG
    (void)predicate;
    (void)message;
    (void)location;
    return;
#else
    kstd_assert(predicate, message, location);
#endif
}

constexpr force_inline auto unimplemented(
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    kstd_assert(false, message != nullptr ? message : "unimplemented", location);
}
