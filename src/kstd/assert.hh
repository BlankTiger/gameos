#pragma once

#include <source_location>

#include "basic.hh"

namespace halt {
using Halt_Print_Fn = auto (*)(char) -> void;
using Pre_Halt_Hook = auto (*)() -> void;
}

#if HOSTED

#include <cstdio>
#include <cstdlib>

namespace halt {

force_inline auto add_printer(Halt_Print_Fn) -> void {}

}

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

constexpr auto MAX_HALT_PRINT_COUNT = 10;

namespace hidden {
    inline bool panicking = false;

    inline auto current_halt_print_count = 0;
    inline Halt_Print_Fn halt_print_fns[MAX_HALT_PRINT_COUNT];

    inline Pre_Halt_Hook pre_halt_hook = nullptr;

    constexpr auto HALT_PRINT_BUF_SIZE = 1024;
    inline char    halt_print_buf[HALT_PRINT_BUF_SIZE];
    inline usize   halt_print_len = 0;
}

force_inline auto set_pre_halt_hook(Pre_Halt_Hook hook) -> void {
    hidden::pre_halt_hook = hook;
}

// Buffers one message, then dumps full buffer to each backend in turn (serial first).
struct Halt_Printer_Backend {
    static auto put_char(char c) -> void {
        if (hidden::halt_print_len < hidden::HALT_PRINT_BUF_SIZE) {
            hidden::halt_print_buf[hidden::halt_print_len++] = c;
        }
    }

    static auto new_line() -> void {
        Halt_Printer_Backend::put_char('\n');
    }

    static auto flush() -> void {
        auto* buf = hidden::halt_print_buf;
        auto  len = hidden::halt_print_len;

        for (usize i = 0; i < len; ++i) {
            serial::put_char(buf[i]);
        }
        for (int idx = 0; idx < hidden::current_halt_print_count; ++idx) {
            for (usize i = 0; i < len; ++i) {
                hidden::halt_print_fns[idx](buf[i]);
            }
        }
        hidden::halt_print_len = 0;
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

force_inline auto flush() -> void {
    hidden::halt_backend.flush();
}

// Formatted halt::print / println: halt_format.hh (needs fmt after this header).

[[noreturn]] static auto
forever(const char* message, const std::source_location& location = std::source_location::current()) -> void {
    if (hidden::pre_halt_hook) {
        hidden::pre_halt_hook();
    }

    // @TODO(blanktiger): Maybe instead of all of these pre_halt_hook shenanigans just make panicking an atomic.
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
    flush();

    asm volatile("cli" ::: "memory");
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
