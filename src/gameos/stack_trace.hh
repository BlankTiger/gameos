#pragma once

#include "kstd/array.hh"
#include "kstd/allocator.hh"
#include "kstd/basic.hh"
#include "kstd/string.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/assert.hh"
#include "kstd/dwarf.hh"

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
    s32    line_number;

    auto format() const -> string {
        return sprint("Trace{ %, %, %, % }", function_address, function_name, file_name, line_number);
    }
};

// Skip this many traces to get rid of the assert/halt traces.
constexpr auto SKIP_FRAME_COUNT = 0;

// Arbitrary.
constexpr auto DEFAULT_FRAME_COUNT = 10;

auto get_current_frame() -> Stack_Frame* {
    Stack_Frame* frame;
    asm volatile("movq %%rbp,%0" : "=r"(frame) ::);
    return frame;
}

auto get_stack_trace_from(Stack_Frame* start_frame, u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> Array<Trace> {
    Array<Trace> traces(max_frame_count);

    auto* frame = start_frame;
    for (u32 frame_index = 0; frame && frame_index < max_frame_count; ++frame_index) {
        if (frame_index >= skip_frame_count) {
            auto function_name = dwarf::function_name_for_address(frame->rip);
            auto row_result    = dwarf::source_for_address(frame->rip);
            traces.push_back({ frame->rip, function_name, row_result.row.file_name, row_result.row.line });
        }
        frame = frame->rbp;
    }

    return traces;
}

auto get_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> Array<Trace> {
    auto* frame = get_current_frame();
    auto stack_trace = get_stack_trace_from(frame, max_frame_count, skip_frame_count);
    return stack_trace;
}

auto print_stack_trace_from(Stack_Frame* start_frame, u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> void {
    halt::println("Stack trace:");
    auto stack_trace = get_stack_trace_from(start_frame, max_frame_count, skip_frame_count);
    for (const auto& trace : stack_trace) {
        halt::println("%:% % (%)", trace.file_name, trace.line_number, trace.function_name, reinterpret_cast<void*>(trace.function_address));
    }
}

auto print_stack_trace(u32 max_frame_count = DEFAULT_FRAME_COUNT, u32 skip_frame_count = SKIP_FRAME_COUNT) -> void {
    auto* frame = get_current_frame();
    print_stack_trace_from(frame, max_frame_count, skip_frame_count);
}

}
