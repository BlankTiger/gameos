#pragma once

#include "kstd/array.hh"
#include "kstd/assert.hh"
#include "kstd/numbers.hh"
#include "kstd/string.hh"

#include "gameos/halt_format.hh"

//
// @TODO(blanktiger): To actually make it work in case of a stack
// overflow we have to probably have a utility here that will swap the
// stack pointer to preallocated memory that can be used for a couple
// function calls that will have to be made to construct the
// stack trace.
//
// This is designed to maximize the probability of it working in the
// worst situations. In theory this should work even when it's called
// because of stack overflows or when the memory completely runs out.
// The memory that's used for the returned stack trace is therefore
// preallocated.
//
// If you wish to get a stack trace for any other reason than a crash
// provide an allocator that will be used instead for all the
// allocations.
//
namespace stack_trace {

struct Stack_Trace {
    Stack_Trace* rbp;
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

auto get_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> Array_View<Frame> {
    Array<Frame> traces(max_frame_count);

    Stack_Trace* stack_trace;
    asm volatile("movq %%rbp,%0" : "=r"(stack_trace) ::);
    for (u32 frame = 0; stack_trace && frame < max_frame_count; ++frame) {
        if (frame >= skip_frame_count) {
            auto function_name = get_function_name(stack_trace->rip);
            traces.push_back({stack_trace->rip, function_name, 0});
        }
        stack_trace = stack_trace->rbp;
    }

    return traces;
}

auto print_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> void {
    Stack_Trace* stack_trace;
    asm volatile("movq %%rbp,%0" : "=r"(stack_trace) ::);
    halt::println("Stack trace:");
    for (u32 frame = 0; stack_trace && frame < max_frame_count; ++frame) {
        if (frame >= skip_frame_count) {
            auto function_name = get_function_name(stack_trace->rip);
            halt::println("  % (0x%)", function_name, stack_trace->rip);
        }
        stack_trace = stack_trace->rbp;
    }
}

}
