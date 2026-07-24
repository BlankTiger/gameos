#pragma once

#include <source_location>

#include "basic.hh"
#include "string_view.hh"

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstdio>
#include <cstdlib>

constexpr force_inline auto kstd_assert(
    bool predicate,
    const string_view message,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (predicate) return;
    std::fprintf(
        stderr, "%s:%u:%u: %s\n",
        location.file_name(), location.line(), location.column(),
        message ? message : "assertion failed"
    );
    std::abort();
}
#else
#include "serial.hh"

using Halt_Print_Fn = auto (*)(char) -> void;

constexpr auto MAX_HALT_PRINT_COUNT     = 10;
inline    auto current_halt_print_count = 0;
inline Halt_Print_Fn __halt_print_fns[MAX_HALT_PRINT_COUNT];

namespace halt {

struct Halt_Printer_Backend {
    static auto put_char(char c) -> void {
        serial::print(c);
        for (int idx = 0; idx < current_halt_print_count; ++idx) {
            __halt_print_fns[idx](c);
        }
    }

    static auto new_line() -> void {
        Halt_Printer_Backend::put_char('\n');
    }
};

auto print(const char* format) -> int {
    return fmt::print<Halt_Printer_Backend>(format);
}

template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::print<Halt_Printer_Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

auto println() -> int {
    return fmt::println<Halt_Printer_Backend>();
}

template <typename T>
auto print(T&& value) -> int {
    return fmt::print<Halt_Printer_Backend>(std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    return fmt::println<Halt_Printer_Backend>(std::forward<T>(value));
}

auto println(const char* format) -> int {
    return fmt::println<Halt_Printer_Backend>(format);
}

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::println<Halt_Printer_Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

} // namespace halt

static bool __panicking = false;

[[noreturn]] static auto
halt_forever(const string_view message, const std::source_location& location = std::source_location::current()) -> void {
    if (__panicking) {
        for (;;) asm volatile("hlt");
    }
    __panicking = true;

    // Always safe: no dependency on mem/gfx/term.
    halt::print("%:%:%", location.file_name(), location.line(), location.column());
    if (message != nullptr) halt::println(": %", message);

    for (;;) asm volatile("hlt");
}

constexpr force_inline auto kstd_assert(
    bool predicate,
    const string_view message = {},
    const std::source_location& location = std::source_location::current()
) -> void {
    if (!predicate) halt_forever(message, location);
}

force_inline auto halt_add_printer(Halt_Print_Fn fn) -> void {
    if (current_halt_print_count >= MAX_HALT_PRINT_COUNT) halt_forever("Reached the maximum halt printer count.");
    __halt_print_fns[current_halt_print_count++] = fn;
}

#endif

constexpr force_inline auto kstd_debug_assert(
    bool predicate,
    const string_view message = {},
    const std::source_location& location = std::source_location::current()
) -> void {
#ifdef NDEBUG
    return;
#else
    kstd_assert(predicate, message, location);
#endif
}

constexpr force_inline auto unimplemented(
    const string_view message = {},
    const std::source_location& location = std::source_location::current()
) -> void {
    kstd_assert(false, message ? message : "unimplemented", location);
}
