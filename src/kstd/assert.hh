#pragma once

#include <source_location>

#include "kstd/basic.hh"

#if HOSTED
#include <cstdio>
#include <cstdlib>
#endif

namespace kstd {

#if HOSTED

[[noreturn]] constexpr force_inline auto assertion_failure(
    const char* message,
    const std::source_location& location
) -> void {
    const char* msg = message ? message : "assertion failed";
    std::fprintf(
        stderr, "%s:%u:%u: %s\n",
        location.file_name(), location.line(), location.column(), msg
    );
    std::abort();
}

#else

// Freestanding users provide platform-specific assertion failure behavior.
[[noreturn]] auto assertion_failure(
    const char* message,
    const std::source_location& location
) -> void;

#endif

} // namespace kstd

constexpr force_inline auto kstd_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    if (predicate) return;
    kstd::assertion_failure(message, location);
}

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
