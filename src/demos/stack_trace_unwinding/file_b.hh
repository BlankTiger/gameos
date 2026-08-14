#pragma once

#include "kstd/basic.hh"
#include "gameos/stack_trace.hh"

noinline auto function_from_file_b() -> Array_View<stack_trace::Frame> {
    auto a = 6;
    auto b = 7;
    auto c = a + b;

    auto stack_trace = stack_trace::get_stack_trace();

    return stack_trace;
}
