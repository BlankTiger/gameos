#pragma once

namespace mem {
struct Allocator;
}

namespace kstd {

struct Context {
    mem::Allocator* global_allocator;
    mem::Allocator* temporary_allocator;
};

inline thread_local Context context{};

} // namespace kstd
