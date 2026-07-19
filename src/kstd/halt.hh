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
        auto println() -> int;                                              \
        auto println(const char* format) -> int;                            \
                                                                            \
        template <typename T, typename... Rest>                             \
        auto println(const char* format, T&& value, Rest&&... rest) -> int; \
    }

#if defined(USE_FB_BACKEND)

namespace fb { DECLARATIONS() }
namespace term = fb::term;

#elif defined(USE_VGA_BACKEND)

namespace vga { DECLARATIONS() }
namespace term = vga::term;

#endif

[[noreturn]] static auto 
halt_forever(const char* message, const std::source_location& location = std::source_location::current()) -> void {
    if (__panicking) {
        for (;;) asm volatile("hlt");
    }
    __panicking = true;

    // Always safe: no dependency on mem/gfx/term.
    serial::print(message != nullptr ? message : "(no message)");
    serial::put_char('\n');
    serial::print(location.file_name());
    serial::put_char('\n');

    // Only safe once term has actually confirmed init succeeded.
    if (__term_ready) {
        if (message != nullptr) term::println("%", message);
        term::println("%:%:%", location.file_name(), location.line(), location.column());
    }

    for (;;) asm volatile("hlt");
}
