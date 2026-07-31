#pragma once

#include <source_location>

#include "basic.hh"

#if HOSTED

#include <cstdio>
#include <cstdlib>

constexpr force_inline auto kstd_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (predicate) return;
    const char* msg = message ? message : "assertion failed";
    std::fprintf(
        stderr, "%s:%u:%u: %s\n",
        location.file_name(), location.line(), location.column(), msg
    );
    std::abort();
}

#else

// serial_port only: full serial.hh pulls format.hh -> string.hh -> assert.hh.
#include "serial_port.hh"

namespace halt {

using Halt_Print_Fn = auto (*)(char) -> void;
constexpr auto MAX_HALT_PRINT_COUNT = 10;

namespace hidden {
    inline bool panicking = false;

    inline auto current_halt_print_count = 0;
    inline Halt_Print_Fn halt_print_fns[MAX_HALT_PRINT_COUNT];
}

// @TODO(blanktiger): ehhh, this is wrong, backend should always fully print to one source before printing to the next one.
struct Halt_Printer_Backend {
    static auto put_char(char c) -> void {
        serial::put_char(c);
        for (int idx = 0; idx < hidden::current_halt_print_count; ++idx) {
            hidden::halt_print_fns[idx](c);
        }
    }

    static auto new_line() -> void {
        Halt_Printer_Backend::put_char('\n');
    }
};

namespace hidden {
    inline Halt_Printer_Backend halt_backend;
}

force_inline auto put_char(char c) -> void {
    hidden::halt_backend.put_char(c);
}

force_inline auto put_cstr(const char* s) -> void {
    if (s == nullptr) return;
    while (*s != '\0') put_char(*s++);
}

force_inline auto put_u32(u32 value) -> void {
    char buf[10];
    u32  i = 0;
    if (value == 0) {
        put_char('0');
        return;
    }
    while (value > 0) {
        buf[i++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    while (i > 0) put_char(buf[--i]);
}

// Formatted halt::print / println: end of format.hh (needs fmt after this header).

[[noreturn]] static auto
forever(const char* message, const std::source_location& location = std::source_location::current()) -> void {
    if (hidden::panicking) {
        for (;;) asm volatile("hlt");
    }
    hidden::panicking = true;

    put_cstr(location.file_name());
    put_char(':');
    put_u32(location.line());
    put_char(':');
    put_u32(location.column());
    if (message != nullptr) {
        put_cstr(": ");
        put_cstr(message);
    }
    put_char('\n');

    for (;;) asm volatile("hlt");
}

force_inline auto add_printer(Halt_Print_Fn fn) -> void {
    if (hidden::current_halt_print_count >= MAX_HALT_PRINT_COUNT) forever("Reached the maximum halt printer count.");
    hidden::halt_print_fns[hidden::current_halt_print_count++] = fn;
}

} // namespace halt


constexpr force_inline auto kstd_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (!predicate) halt::forever(message, location);
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

constexpr force_inline auto unimplemented(
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    kstd_assert(false, message ? message : "unimplemented", location);
}

constexpr force_inline auto unreachable(
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    kstd_assert(false, message ? message : "unreachable", location);
}
