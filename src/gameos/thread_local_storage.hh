#pragma once

#include "kstd/allocator.hh"
#include "kstd/array.hh"
#include "kstd/basic.hh"
#include "kstd/pointer_utils.hh"

#include "gameos/advanced_configuration_and_power_interface.hh"
#include "gameos/low_level_io.hh"

namespace tls {

constexpr u32   IA32_FS_BASE    = 0xC0000100;
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

    void*          allocation = nullptr;
    mem::Allocator allocator{};

    mem::Temporary_Allocator_State temporary_state{};
    Context context{};

    Bounded_Array<Destructor, MAX_DESTRUCTORS> destructors;
};

inline Static_Array<Block*, acpi::MAX_CPUS> idle_blocks;

force_inline auto block_size() -> usize {
    auto image_size = ptr_addr(__tls_end) - ptr_addr(__tls_start);
    return mem::align_up(image_size, TLS_ALIGNMENT);
}

force_inline auto allocation_size() -> usize {
    return block_size() + 2 * size_of(psize);
}

force_inline auto set_base(void* base) -> void {
    low_level_io::write_model_specific_register(IA32_FS_BASE, ptr_addr(base));
}

force_inline auto base() -> void* {
    return addr_as<void*>(low_level_io::read_model_specific_register(IA32_FS_BASE));
}

auto create(const Context& inherited_context) -> Block* {
    auto allocator = inherited_context.allocator;
    kstd_assert(allocator.valid());

    auto block_allocation = mem::alloc(size_of(Block), align_of(Block), allocator);
    auto image_allocation = mem::alloc(allocation_size(), TLS_ALIGNMENT, allocator);
    kstd_assert(block_allocation.error == mem::Allocator_Error::NONE);
    kstd_assert(image_allocation.error == mem::Allocator_Error::NONE);

    auto* block = cast(Block*)block_allocation.memory;
    auto* image = cast(u8*)image_allocation.memory;

    void* temporary_storage = nullptr;
    if (inherited_context.temporary_state != nullptr) {
        kstd_assert(inherited_context.temporary_state->base != nullptr);

        auto temporary_allocation = mem::alloc(mem::TEMPORARY_STORAGE_SIZE, TLS_ALIGNMENT, allocator);
        kstd_assert(temporary_allocation.error == mem::Allocator_Error::NONE);
        temporary_storage = temporary_allocation.memory;
    }

    kstd_assert(
        block != nullptr &&
        image != nullptr &&
        (
            inherited_context.temporary_state == nullptr ||
            temporary_storage != nullptr
        ),
        "TLS allocation failed"
    );

    kstd_memset(image, 0, allocation_size());
    kstd_memcpy(image, __tls_start, ptr_addr(__tdata_end) - ptr_addr(__tls_start));

    auto new_context = inherited_context;
    if (temporary_storage != nullptr) {
        auto state = mem::Temporary_Allocator_State{ temporary_storage, mem::TEMPORARY_STORAGE_SIZE };
        new_context.temporary_allocator = {};
        new_context.temporary_state     = nullptr;

        new (block) Block {
            .allocation      = image,
            .allocator       = allocator,
            .temporary_state = std::move(state),
            .context         = new_context,
            .destructors     = {},
        };
        block->context.temporary_state     = &block->temporary_state;
        block->context.temporary_allocator = block->temporary_state.get_allocator();
    } else {
        new_context.temporary_allocator = {};
        new_context.temporary_state     = nullptr;

        new (block) Block {
            .allocation      = image,
            .allocator       = allocator,
            .temporary_state = {},
            .context         = new_context,
            .destructors     = {},
        };
    }

    return block;
}

auto activate(Block* block) -> void {
    kstd_assert(block != nullptr);

    auto* thread_pointer = ptr_offset(block->allocation, block_size());
    *cast(psize*)thread_pointer = ptr_addr(thread_pointer);

    auto* block_pointer_storage = ptr_offset(thread_pointer, size_of(psize));
    *cast(Block**)block_pointer_storage = block;

    set_base(thread_pointer);
    context = block->context;
}

auto destroy(Block* block) -> void {
    if (block == nullptr) return;

    auto* previous_base    = base();
    auto  previous_context = context;
    defer({
        set_base(previous_base);
        context = previous_context;
    });

    activate(block);
    while (block->destructors.size != 0) {
        auto entry = block->destructors.pop_back();
        entry.function(entry.object);
    }

    auto allocator = block->allocator;
    auto* temporary_storage = block->context.temporary_state != nullptr ? block->context.temporary_state->base : nullptr;

    auto error_free_temp = mem::free(temporary_storage, mem::TEMPORARY_STORAGE_SIZE, TLS_ALIGNMENT, allocator);
    kstd_debug_assert(error_free_temp == mem::Allocator_Error::NONE);

    auto error_free_alloc = mem::free(block->allocation, allocation_size(), TLS_ALIGNMENT, allocator);
    kstd_debug_assert(error_free_alloc == mem::Allocator_Error::NONE);

    block->~Block();
    auto error_free_block = mem::free(block, size_of(Block), align_of(Block), allocator);
    kstd_debug_assert(error_free_block == mem::Allocator_Error::NONE);
}

extern "C" auto __cxa_thread_atexit(
    Destructor_Function function,
    void*               object,
    void*               dso_handle
) -> int {
    auto* thread_pointer = cast(u8*)base();
    auto* block = *cast(Block**)(thread_pointer + size_of(psize));
    if (block->destructors.size == MAX_DESTRUCTORS) return -1;

    block->destructors.push_back({function, object, dso_handle});
    return 0;
}

auto initialize_bsp(const Context& inherited_context) -> void {
    auto* block = create(inherited_context);
    idle_blocks[0] = block;
    activate(block);
}

auto initialize_application_processor(u32 cpu_index, const Context& inherited_context) -> void {
    auto* ap_block = create(inherited_context);
    idle_blocks[cpu_index] = ap_block;
    activate(ap_block);
}

auto idle(u32 cpu_index) -> Block* {
    return idle_blocks[cpu_index];
}

}
