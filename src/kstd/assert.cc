#include <source_location>

#include "basic.hh"
#include "cstring.hh"
#include "assert.hh"
#include "halt.hh"
#include "serial.hh"
#include "string_view.hh"

//
// Implementation of the panic machinery declared in assert.hh. It lives in its
// own translation unit on purpose: reporting a panic needs the printing stack
// (halt.hh -> serial.hh -> format.hh -> string_view.hh), while string_view.hh
// and everything above it needs to assert. A decl/def split across a TU
// boundary is the only way to have both without an include cycle.
//

namespace {

constexpr auto MAX_HALT_PRINT_COUNT = 10;

auto current_halt_print_count = 0;
Halt_Print_Fn halt_print_fns[MAX_HALT_PRINT_COUNT];

bool panicking = false;

} // namespace

auto halt_put_char(char c) -> void {
    serial::put_char(c);
    for (int idx = 0; idx < current_halt_print_count; ++idx) {
        halt_print_fns[idx](c);
    }
}

[[noreturn]] auto halt_forever(const char* message, usize size, const std::source_location& location) -> void {
    if (panicking) {
        for (;;) asm volatile("hlt");
    }
    panicking = true;

    halt::print("%:%:%", location.file_name(), location.line(), location.column());
    if (message != nullptr) halt::println(": %", string_view(message, size));

    for (;;) asm volatile("hlt");
}

[[noreturn]] auto halt_forever(const char* message, const std::source_location& location) -> void {
    halt_forever(message, message != nullptr ? kstd_strlen(message) : 0, location);
}

auto halt_add_printer(Halt_Print_Fn fn) -> void {
    if (current_halt_print_count >= MAX_HALT_PRINT_COUNT) halt_forever("Reached the maximum halt printer count.");
    halt_print_fns[current_halt_print_count++] = fn;
}
