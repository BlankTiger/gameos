#pragma once

#include <cstdio>
#include <cstdlib>
#include <execinfo.h>

#include "kstd/basic.hh"
#include "kstd/array.hh"

namespace stack_trace {

inline auto print_stack_trace(u32 max_frame_count = 10, u32 skip_frame_count = 0) -> void {
    constexpr u32 MAX_FRAME_COUNT = 64;
    Static_Array<void*, MAX_FRAME_COUNT> frames{};
    const auto frame_count = backtrace(frames.data, static_cast<int>(MAX_FRAME_COUNT));
    char**     symbols     = backtrace_symbols(frames.data, frame_count);

    if (symbols == nullptr) return;

    std::fprintf(stderr, "Stack trace:\n");
    const auto end = frame_count < static_cast<int>(skip_frame_count + max_frame_count)
        ? frame_count
        : static_cast<int>(skip_frame_count + max_frame_count);
    for (int index = static_cast<int>(skip_frame_count); index < end; ++index) {
        std::fprintf(stderr, "  %s\n", symbols[index]);
    }

    std::free(symbols);
}

}
