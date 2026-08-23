#pragma once

#include "basic.hh"

namespace mem {
struct Allocator;
struct Temporary_Allocator;
}

namespace ctx {

struct Formatting_Config {
    bool  newline_after_each_array_element = false;
    usize array_element_indent_spaces      = 4;
};

} // namespace context

struct Context {
    mem::Allocator*           allocator;
    mem::Temporary_Allocator* temporary_allocator;

    ctx::Formatting_Config    formatting_config;
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
