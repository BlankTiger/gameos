#pragma once

#include "kstd/basic.hh"
#include "gameos/stack_trace.hh"

noinline auto function_from_file_b() -> int {
    auto a = 6;
    auto b = 7;
    auto stack_trace = stack_trace::get_stack_trace();

    auto c = a + b;
    return c;
}
