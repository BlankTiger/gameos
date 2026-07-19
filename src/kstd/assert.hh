#pragma once

#include <source_location>

constexpr force_inline auto assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (!predicate) halt_forever(message, location);
}

constexpr force_inline auto debug_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
#ifdef NDEBUG
    return;
#else
    assert(predicate, message, location);
#endif
}
