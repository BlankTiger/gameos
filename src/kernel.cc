#include "kstd.hh"
#include "memory.hh"
#include "terminal.hh"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

namespace term = kstd::term;
namespace mem = kstd::mem;

mem::Allocator* __global_allocator;
mem::Buddy_Allocator __buddy;
mem::Arena_Allocator __arena;

using Init_Function = void (*)();

extern "C" Init_Function __init_array_start[];
extern "C" Init_Function __init_array_end[];
extern "C" Init_Function __ctors_start[];
extern "C" Init_Function __ctors_end[];

extern "C" auto run_global_constructors() -> void {
    for (Init_Function* fn = __init_array_start; fn != __init_array_end; ++fn) {
        (*fn)();
    }

    for (Init_Function* fn = __ctors_end; fn != __ctors_start;) {
        --fn;
        if (*fn == nullptr || *fn == reinterpret_cast<Init_Function>(-1)) continue;
        (*fn)();
    }
}

auto operator new(usize size) -> void* {
    if (void* ptr = __global_allocator->alloc(size)) return ptr;
    kstd::halt_forever("new failed");
}

auto operator new[](usize size) -> void* {
    if (void* ptr = __global_allocator->alloc(size)) return ptr;
    kstd::halt_forever("new[] failed");
}

auto operator delete(void* ptr) noexcept -> void {
    __global_allocator->free(ptr, 0);
}

auto operator delete[](void* ptr) noexcept -> void {
    __global_allocator->free(ptr, 0);
}

auto operator delete(void* ptr, usize size) noexcept -> void {
    __global_allocator->free(ptr, size);
}

auto operator delete[](void* ptr, usize size) noexcept -> void {
    __global_allocator->free(ptr, size);
}

static auto buddy_allocator_smoke_test() -> bool {
    constexpr usize block_size = 64;

    auto* value = new std::uint32_t(0x12345678u);
    if (value == nullptr || *value != 0x12345678u) return false;

    auto* first = static_cast<std::uint8_t*>(::operator new(block_size));
    auto* second = static_cast<std::uint8_t*>(::operator new(block_size));
    if (first == nullptr || second == nullptr) return false;
    if (first == second) return false;

    for (usize i = 0; i < block_size; ++i) {
        first[i] = 0xAA;
        second[i] = 0x55;
    }

    for (usize i = 0; i < block_size; ++i) {
        if (first[i] != 0xAA || second[i] != 0x55) return false;
    }

    ::operator delete(second);
    ::operator delete(first);
    delete value;

    auto* third = static_cast<std::uint8_t*>(::operator new(block_size));
    if (third == nullptr) return false;

    ::operator delete(third);
    return true;
}

extern "C" auto kernel_main(uint32_t magic, const mem::Multiboot_Info* mbi) -> void {
    term::terminal_initialize();

    if (magic != 0x2BADB002) {
        term::terminal_writestring("Bad multiboot magic\n");
        return;
    }

    term::terminal_writestring("mem init\n");
    mem::memory_initialize(mbi);

    __buddy.init();
    __global_allocator = &__buddy;
    __arena.init();
    __global_allocator = &__arena;

}
