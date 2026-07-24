#pragma once

#include "string_view.hh"
#include "array.hh"
#include "assert.hh"
#include "int.hh"

namespace stacktrace {

struct Stacktrace {
    Stacktrace* ebp;
    psize       eip;
};

struct Frame {
    psize       function_address;
    string_view function_name;
    u32         line_number;
};

auto get_function_name(psize address) -> string_view {
    return "implement me!";
}

// Skip this many traces to get rid of the assert/halt traces.
constexpr auto SKIP_FRAME_COUNT = 0;

// Arbitrary.
constexpr auto DEFAULT_FRAME_COUNT = 10;

auto get_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> Array<Frame> {
    Array<Frame> traces(max_frame_count);

    Stacktrace* stacktrace;
    asm volatile("movl %%ebp,%0" : "=r"(stacktrace) ::);
    for (u32 frame = 0; stacktrace && frame < max_frame_count; ++frame) {
        if (frame >= skip_frame_count) {
            auto function_name = get_function_name(stacktrace->eip);
            traces.push_back({stacktrace->eip, function_name, 0});
        }
        stacktrace = stacktrace->ebp;
    }

    return traces;
}

auto print_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> void {
    Stacktrace* stacktrace;
    asm volatile("movl %%ebp,%0" : "=r"(stacktrace) ::);
    halt::println("Stack trace:");
    for (u32 frame = 0; stacktrace && frame < max_frame_count; ++frame) {
        if (frame >= skip_frame_count) {
            auto function_name = get_function_name(stacktrace->eip);
            halt::println("  % (0x%)", function_name, stacktrace->eip);
        }
        stacktrace = stacktrace->ebp;
    }
}

}
