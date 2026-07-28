#pragma once

#include <new>

#include "assert.hh"
#include "memory.hh"

// Include exactly once in the program (main.cc does). Unlike every other kstd
// header these definitions cannot be `inline`: [replacement.functions] forbids
// it for replacement allocation functions. Would be a .cc if the allocator it
// forwards to weren't header-only.

auto operator new(usize size) -> void* {
    if (void* ptr = mem::__global_allocator->alloc(size)) return ptr;
    halt_forever("new failed");
}

auto operator new[](usize size) -> void* {
    if (void* ptr = mem::__global_allocator->alloc(size)) return ptr;
    halt_forever("new[] failed");
}

auto operator delete(void* ptr) noexcept -> void {
    mem::__global_allocator->free(ptr, 0);
}

auto operator delete[](void* ptr) noexcept -> void {
    mem::__global_allocator->free(ptr, 0);
}

auto operator delete(void* ptr, usize size) noexcept -> void {
    mem::__global_allocator->free(ptr, size);
}

auto operator delete[](void* ptr, usize size) noexcept -> void {
    mem::__global_allocator->free(ptr, size);
}
