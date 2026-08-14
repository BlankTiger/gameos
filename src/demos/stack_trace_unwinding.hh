#pragma once

#include "kstd/basic.hh"
#include "gameos/stack_trace.hh"

#include "stack_trace_unwinding/file_a.hh"

noinline auto function_call_into_other_files() -> void {
    call_function_from_file_b();
}

auto stack_trace_main() -> void {
    function_call_into_other_files();
}
