#pragma once

#include "kstd/basic.hh"
#include "kstd/stack_trace.hh"

#include "gameos/serial_format.hh"
#include "gameos/power.hh"

#include "stack_trace_unwinding/file_a.hh"

noinline auto function_call_into_other_files() -> Array_View<stack_trace::Trace> {
    auto stack_trace = call_function_from_file_b();
    return stack_trace;
}

auto stack_trace_main() -> void {
    auto stack_trace = function_call_into_other_files();
    for (const auto& frame: stack_trace) {
        serial::println("  0x%  %:% (%)", frame.function_address, frame.file_name, frame.line_number, frame.function_name);
    }
    power::off();
}
