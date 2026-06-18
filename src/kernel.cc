#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <cstdint>

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

void* operator new(std::size_t size) {
    if (void* ptr = __global_allocator->alloc(size)) return ptr;

    term::terminal_writestring("\nnew failed");
    for (;;) {
        asm volatile("hlt");
    }
}

void* operator new[](std::size_t size) {
    if (void* ptr = __global_allocator->alloc(size)) return ptr;

    term::terminal_writestring("\nnew[] failed");
    for (;;) {
        asm volatile("hlt");
    }
}

void operator delete(void* ptr) noexcept {
    __global_allocator->free(ptr, 0);
}

void operator delete[](void* ptr) noexcept {
    __global_allocator->free(ptr, 0);
}

void operator delete(void* ptr, std::size_t size) noexcept {
    __global_allocator->free(ptr, size);
}

void operator delete[](void* ptr, std::size_t size) noexcept {
    __global_allocator->free(ptr, size);
}

static bool buddy_allocator_smoke_test() {
    constexpr std::size_t block_size = 64;

    auto* value = new std::uint32_t(0x12345678u);
    if (value == nullptr || *value != 0x12345678u) return false;

    auto* first = static_cast<std::uint8_t*>(::operator new(block_size));
    auto* second = static_cast<std::uint8_t*>(::operator new(block_size));
    if (first == nullptr || second == nullptr) return false;
    if (first == second) return false;

    for (std::size_t i = 0; i < block_size; ++i) {
        first[i] = 0xAA;
        second[i] = 0x55;
    }

    for (std::size_t i = 0; i < block_size; ++i) {
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

    mem::memory_initialize(mbi);
    __buddy.init();
    __arena.init();
    __global_allocator = &__buddy;
    __global_allocator = &__arena;

    term::terminal_writestring("buddy allocator smoke test: \n");
    if (buddy_allocator_smoke_test()) {
        term::terminal_writestring("pass\n");
    } else {
        term::terminal_writestring("fail\n");
        for (;;) asm volatile("hlt");
    }
}
