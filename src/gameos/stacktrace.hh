#pragma once

#include "kstd/array.hh"
#include "kstd/assert.hh"
#include "kstd/numbers.hh"
#include "kstd/string.hh"

#include "gameos/halt_format.hh"

namespace stacktrace {

struct Stacktrace {
    Stacktrace* rbp;
    psize       rip;
};

struct Frame {
    psize  function_address;
    string function_name;
    u32    line_number;
};

auto get_function_name(psize address) -> string {
    (void)address;
    return "implement me!";
}

// Skip this many traces to get rid of the assert/halt traces.
constexpr auto SKIP_FRAME_COUNT = 0;

// Arbitrary.
constexpr auto DEFAULT_FRAME_COUNT = 10;

auto get_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> Array<Frame> {
    Array<Frame> traces(max_frame_count);

    Stacktrace* stacktrace;
    asm volatile("movq %%rbp,%0" : "=r"(stacktrace) ::);
    for (u32 frame = 0; stacktrace && frame < max_frame_count; ++frame) {
        if (frame >= skip_frame_count) {
            auto function_name = get_function_name(stacktrace->rip);
            traces.push_back({stacktrace->rip, function_name, 0});
        }
        stacktrace = stacktrace->rbp;
    }

    return traces;
}

auto print_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> void {
    Stacktrace* stacktrace;
    asm volatile("movq %%rbp,%0" : "=r"(stacktrace) ::);
    halt::println("Stack trace:");
    for (u32 frame = 0; stacktrace && frame < max_frame_count; ++frame) {
        if (frame >= skip_frame_count) {
            auto function_name = get_function_name(stacktrace->rip);
            halt::println("  % (0x%)", function_name, stacktrace->rip);
        }
        stacktrace = stacktrace->rbp;
    }
}

}
