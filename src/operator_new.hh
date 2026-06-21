#pragma once

#include "int.hh"

auto operator new(usize size) -> void* {
    if (void* ptr = kstd::mem::__global_allocator->alloc(size)) return ptr;
    kstd::halt_forever("new failed");
}

auto operator new[](usize size) -> void* {
    if (void* ptr = kstd::mem::__global_allocator->alloc(size)) return ptr;
    kstd::halt_forever("new[] failed");
}

auto operator delete(void* ptr) noexcept -> void {
    kstd::mem::__global_allocator->free(ptr, 0);
}

auto operator delete[](void* ptr) noexcept -> void {
    kstd::mem::__global_allocator->free(ptr, 0);
}

auto operator delete(void* ptr, usize size) noexcept -> void {
    kstd::mem::__global_allocator->free(ptr, size);
}

auto operator delete[](void* ptr, usize size) noexcept -> void {
    kstd::mem::__global_allocator->free(ptr, size);
}
