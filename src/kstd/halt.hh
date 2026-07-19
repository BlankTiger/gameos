#pragma once

#include <source_location>

static bool __panicking = false;
static bool __term_ready = false;  

// Must only be called after term::initialize() succeeds.
force_inline auto
halt_set_term_ready() -> void {
    __term_ready = true;
}

// Forward declarations of term::print functions.
#define DECLARATIONS()                                                      \
    namespace term {                                                        \
        auto print(const char* format) -> int;                              \
                                                                            \
        template <typename T, typename... Rest>                             \
        auto print(const char* format, T&& value, Rest&&... rest) -> int;   \
                                                                            \
        auto println() -> int;                                              \
        auto println(const char* format) -> int;                            \
                                                                            \
        template <typename T, typename... Rest>                             \
        auto println(const char* format, T&& value, Rest&&... rest) -> int; \
    }

namespace fb { DECLARATIONS() }
namespace term = fb::term;

[[noreturn]] static auto 
halt_forever(const char* message, const std::source_location& location = std::source_location::current()) -> void {
    if (__panicking) {
        for (;;) asm volatile("hlt");
    }
    __panicking = true;

    // Always safe: no dependency on mem/gfx/term.
    serial::print("%:%:%", location.file_name(), location.line(), location.column());
    if (message != nullptr) serial::println(": %", message);

    // Only safe once term has actually confirmed init succeeded.
    if (__term_ready) {
        term::print("%:%:%", location.file_name(), location.line(), location.column());
        if (message != nullptr) term::println(": %", message);
    }

    for (;;) asm volatile("hlt");
}
