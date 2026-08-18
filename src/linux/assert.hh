#pragma once

#include <cstdio>
#include <cstdlib>

#include "kstd/assert.hh"

namespace kstd {

[[noreturn]] inline auto assertion_failure(
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

}
