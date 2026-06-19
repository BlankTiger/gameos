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

void* operator new(usize size) {
    term::terminal_writestring("new ");
    term::terminal_writeint(size);
    term::terminal_writestring("\n");
    term::terminal_writestring("alloc call ");
    term::terminal_write_hex((u64)(uintptr_t)__global_allocator);
    term::terminal_writestring("\n");
    term::terminal_writestring("alloc enter\n");
    if (void* ptr = __global_allocator->alloc(size)) return ptr;

    term::terminal_writestring("alloc exit\n");

    kstd::halt_forever("new failed");
}

void* operator new[](usize size) {
    term::terminal_writestring("new[] ");
    term::terminal_writeint(size);
    term::terminal_writestring("\n");
    term::terminal_writestring("alloc call ");
    term::terminal_write_hex((u64)(uintptr_t)__global_allocator);
    term::terminal_writestring("\n");
    term::terminal_writestring("alloc enter\n");
    if (void* ptr = __global_allocator->alloc(size)) return ptr;

    term::terminal_writestring("alloc exit\n");

    kstd::halt_forever("new[] failed");
}

void operator delete(void* ptr) noexcept {
    __global_allocator->free(ptr, 0);
}

void operator delete[](void* ptr) noexcept {
    __global_allocator->free(ptr, 0);
}

void operator delete(void* ptr, usize size) noexcept {
    __global_allocator->free(ptr, size);
}

void operator delete[](void* ptr, usize size) noexcept {
    __global_allocator->free(ptr, size);
}

static bool buddy_allocator_smoke_test() {
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

extern "C" void kernel_main(uint32_t magic, const mem::Multiboot_Info* mbi) {
    term::terminal_initialize();

    if (magic != 0x2BADB002) {
        term::terminal_writestring("Bad multiboot magic\n");
        return;
    }

    term::terminal_writestring("mem init\n");
    mem::memory_initialize(mbi);

    term::terminal_writestring("buddy init\n");
    __buddy.init();

    term::terminal_writestring("set buddy global\n");
    __global_allocator = &__buddy;

    term::terminal_writestring("arena init\n");
    __arena.init();

    term::terminal_writestring("arena ready\n");

    term::terminal_writestring("set arena global\n");
    __global_allocator = &__arena;

    term::terminal_writestring("arena direct test\n");
    void* arena_probe = __arena.alloc(64);
    if (arena_probe == nullptr) {
        kstd::halt_forever("arena direct alloc failed");
    }
    term::terminal_writestring("arena direct ok\n");
    __arena.free(arena_probe, 64);

    term::terminal_writestring("smoke test\n");
    if (buddy_allocator_smoke_test()) {
        term::terminal_writestring("pass\n");
    } else {
        kstd::halt_forever("fail");
    }
}
