#pragma once

#include <new>

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
