#pragma once

#include "basic.hh"

namespace mem {
struct Allocator;
}

namespace kstd {

struct Formatting_Config {
    bool newline_after_each_array_element = false;
    usize array_element_indent_spaces     = 4;
};

struct Context {
    mem::Allocator*   allocator;
    mem::Allocator*   temporary_allocator;
    Formatting_Config formatting_config;
};

inline thread_local Context context{};

}
