#pragma once

#include <source_location>

constexpr force_inline auto assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()) -> void {
    if (!predicate) {
        if (message) {
            term::println(
                "%:%:%: assertion failed: %", location.file_name(), location.line(), location.column(), message);
        } else {
            term::println("%:%:%: assertion failed", location.file_name(), location.line(), location.column());
        }

        for (;;) asm volatile("hlt");
    }
}

constexpr force_inline auto debug_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()) -> void {
#ifdef NDEBUG
    return;
#else
    assert(predicate, message, location);
#endif
}
