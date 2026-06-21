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

struct At_Exit_Entry {
    void (*destructor)(void*);
    void* object;
    void* dso_handle;
};

static constexpr usize MAX_AT_EXIT_ENTRIES = 32;
static At_Exit_Entry __at_exit_entries[MAX_AT_EXIT_ENTRIES];
static usize __at_exit_entry_count;

extern "C" {
__attribute__((visibility("hidden"))) void* __dso_handle = nullptr;
}

extern "C" Init_Function __init_array_start[];
extern "C" Init_Function __init_array_end[];
extern "C" Init_Function __ctors_start[];
extern "C" Init_Function __ctors_end[];

extern "C" int __cxa_atexit(void (*destructor)(void*), void* object, void* dso_handle) {
    if (__at_exit_entry_count == MAX_AT_EXIT_ENTRIES) return -1;

    __at_exit_entries[__at_exit_entry_count++] = {destructor, object, dso_handle};
    return 0;
}

extern "C" void __cxa_finalize(void* dso_handle) {
    if (dso_handle == nullptr) {
        while (__at_exit_entry_count != 0) {
            At_Exit_Entry entry = __at_exit_entries[--__at_exit_entry_count];
            if (entry.destructor != nullptr) entry.destructor(entry.object);
        }
        return;
    }

    for (usize i = __at_exit_entry_count; i != 0; --i) {
        At_Exit_Entry& entry = __at_exit_entries[i - 1];
        if (entry.destructor == nullptr || entry.dso_handle != dso_handle) continue;

        auto destructor = entry.destructor;
        entry.destructor = nullptr;
        destructor(entry.object);
    }
}

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

extern "C" auto run_global_destructors() -> void {
    __cxa_finalize(nullptr);
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

extern "C" auto kernel_main(uint32_t magic, const mem::Multiboot_Info* mbi) -> void {
    term::terminal_initialize();

    if (magic != 0x2BADB002) {
        term::terminal_writestring("Bad multiboot magic\n");
        return;
    }

    term::terminal_writestring("mem init\n");
    mem::memory_initialize(mbi);

    __buddy.init();
    __arena.init(&__buddy);
    __global_allocator = &__arena;
}
