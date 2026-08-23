#pragma once

#include <source_location>

#include "kstd/basic.hh"

namespace kstd {

[[noreturn]] auto assertion_failure(
    const char* message,
    const std::source_location& location
) -> void;

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

[[noreturn]] constexpr force_inline auto unimplemented(
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    kstd::assertion_failure(message ? message : "unimplemented", location);
}

[[noreturn]] constexpr force_inline auto unreachable(
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()
) -> void {
    kstd::assertion_failure(message ? message : "unreachable", location);
}

#if OS == GAMEOS
#include "gameos/assert.hh"
#elif OS == LINUX
#include "linux/assert.hh"
#else
#error "Unsupported OS"
#endif
