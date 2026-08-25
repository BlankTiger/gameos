#pragma once

#include "allocator_types.hh"

namespace mem {
struct Temporary_Allocator_State;
}

namespace ctx {

struct Formatting_Config {
    bool  newline_after_each_array_element = false;
    s64 array_element_indent_spaces        = 4;
};

} // namespace context

struct Context {
    mem::Allocator allocator{};

    // @Important(blanktiger): Changing one of those implies changing the
    // other. `temporary_allocator` is provided for convenience. I might
    // reconsider and remove it in favor of always calling
    // temporary_state->get_allocator(), that however inccurs the cost of a
    // deref and function call each time.. Unclear.
    mem::Temporary_Allocator_State* temporary_state{};
    mem::Allocator                  temporary_allocator{};

    ctx::Formatting_Config formatting_config;
};

inline thread_local Context context{};

struct Push_Context {
    Context previous_context;

    Push_Context() : previous_context(context) {}
    ~Push_Context() {
        context = previous_context;
    }
};

#define PUSH_CONTEXT() Push_Context DEFER_UNIQ(_push_context_)
