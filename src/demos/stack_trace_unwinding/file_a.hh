#pragma once

#include "kstd/basic.hh"
#include "kstd/stack_trace.hh"

#include "file_b.hh"

noinline auto call_function_from_file_b() -> Array_View<stack_trace::Trace> {
    auto stack_trace = function_from_file_b();
    return stack_trace;
}
