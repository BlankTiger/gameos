#pragma once

#include "basic.hh"

namespace mem {
struct Allocator;
}

namespace kstd {

struct Context {
    mem::Allocator* allocator;
    mem::Allocator* temporary_allocator;
};

inline thread_local Context context{};

}
