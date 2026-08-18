#pragma once

#include "kstd/array.hh"
#include "kstd/allocator.hh"
#include "kstd/basic.hh"
#include "kstd/string.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/assert.hh"

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

namespace hidden {
    mem::Arena_Allocator stack_trace_allocator(4096);
}

struct Stack_Frame {
    Stack_Frame* rbp;
    psize        rip;
};

struct Trace {
    psize  function_address;
    string function_name;
    string file_name;
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

auto get_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> Array_View<Trace> {
    Array<Trace> traces(max_frame_count);

    Stack_Frame* frame;
    asm volatile("movq %%rbp,%0" : "=r"(frame) ::);
    for (u32 frame_index = 0; frame && frame_index < max_frame_count; ++frame_index) {
        if (frame_index >= skip_frame_count) {
            auto function_name = get_function_name(frame->rip);
            traces.push_back({frame->rip, function_name, 0});
        }
        frame = frame->rbp;
    }

    return traces;
}

auto is_likely_a_frame(Stack_Frame* current_frame, Stack_Frame* previous_frame) -> bool {
    if (current_frame == nullptr) return false;
    // @TODO(blanktiger): Continue here.
}

auto print_stack_trace_from(Stack_Frame* start_frame, u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> void {
    halt::println("Stack trace:");
    Stack_Frame* frame    = start_frame;
    Stack_Frame* previous = nullptr;
    for (u32 frame_index = 0; frame && frame_index < max_frame_count; ++frame_index) {
        if (frame_index >= skip_frame_count) {
            auto function_name = get_function_name(frame->rip);
            halt::println("  % (0x%)", function_name, frame->rip);
        }
        frame = frame->rbp;
    }
}

auto print_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> void {
    Stack_Frame* frame;
    asm volatile("movq %%rbp,%0" : "=r"(frame) ::);
    print_stack_trace_from(frame, max_frame_count, skip_frame_count);
}

}
