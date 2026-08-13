#pragma once

#include "kstd/allocator.hh"
#include "kstd/array.hh"
#include "kstd/basic.hh"
#include "kstd/pointer_utils.hh"

#include "gameos/advanced_configuration_and_power_interface.hh"
#include "gameos/low_level_io.hh"

namespace tls {

constexpr u32 IA32_FS_BASE = 0xC0000100u;
constexpr usize MAX_DESTRUCTORS = 32;
constexpr usize TLS_ALIGNMENT   = 16;

extern "C" u8 __tls_start[];
extern "C" u8 __tdata_end[];
extern "C" u8 __tls_end[];

using Destructor_Function = auto (*)(void* object) -> void;

struct Block {
    struct Destructor {
        Destructor_Function function;
        void*               object;
        void*               dso_handle;
    };

    void*           allocation = nullptr;
    mem::Allocator* allocator  = nullptr;
    Bounded_Array<Destructor, MAX_DESTRUCTORS> destructors;
};

inline Static_Array<Block*, acpi::MAX_CPUS> idle_blocks;

force_inline auto block_size() -> usize {
    auto image_size = ptr_addr(__tls_end) - ptr_addr(__tls_start);
    return mem::align_up(image_size, TLS_ALIGNMENT);
}

force_inline auto allocation_size() -> usize {
    return block_size() + 2 * sizeof(psize);
}

force_inline auto set_base(void* base) -> void {
    low_level_io::write_model_specific_register(IA32_FS_BASE, ptr_addr(base));
}

force_inline auto base() -> void* {
    return addr_as<void*>(low_level_io::read_model_specific_register(IA32_FS_BASE));
}

auto create() -> Block* {
    auto* allocator = mem::resolve_allocator();
    auto* block     = static_cast<Block*>(allocator->alloc(sizeof(Block), alignof(Block)));
    auto* image     = static_cast<u8*>(allocator->alloc(allocation_size(), TLS_ALIGNMENT));
    kstd_assert(block != nullptr && image != nullptr, "TLS allocation failed");

    kstd_memset(image, 0, allocation_size());
    kstd_memcpy(image, __tls_start, ptr_addr(__tdata_end) - ptr_addr(__tls_start));
    *block = {
        .allocation  = image,
        .allocator   = allocator,
        .destructors = {},
    };
    return block;
}

auto activate(Block* block) -> void {
    kstd_assert(block != nullptr);

    auto* thread_pointer = ptr_offset(block->allocation, block_size());
    *reinterpret_cast<psize*>(thread_pointer) = ptr_addr(thread_pointer);

    auto* block_pointer_storage = ptr_offset(thread_pointer, sizeof(psize));
    *reinterpret_cast<Block**>(block_pointer_storage) = block;

    set_base(thread_pointer);
}

auto destroy(Block* block) -> void {
    if (block == nullptr) return;

    auto* previous_base = base();
    defer(set_base(previous_base));

    activate(block);
    while (block->destructors.size != 0) {
        auto entry = block->destructors.pop_back();
        entry.function(entry.object);
    }

    block->allocator->free(block->allocation, allocation_size(), TLS_ALIGNMENT);
    block->allocator->free(block, sizeof(Block), alignof(Block));
}

extern "C" auto __cxa_thread_atexit(
    Destructor_Function function,
    void*               object,
    void*               dso_handle
) -> int {
    auto* thread_pointer = static_cast<u8*>(base());
    auto* block = *reinterpret_cast<Block**>(thread_pointer + sizeof(psize));
    if (block->destructors.size == MAX_DESTRUCTORS) return -1;

    block->destructors.push_back({function, object, dso_handle});
    return 0;
}

auto initialize_bsp() -> void {
    auto* block = create();
    idle_blocks[0] = block;
    activate(block);
}

auto initialize_application_processor(u32 cpu_index) -> void {
    auto* ap_block = create();
    idle_blocks[cpu_index] = ap_block;
    activate(ap_block);
}

auto idle(u32 cpu_index) -> Block* {
    return idle_blocks[cpu_index];
}

}
