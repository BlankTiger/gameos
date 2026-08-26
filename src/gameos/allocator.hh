#pragma once

#include "kstd/allocator.hh"
#include "kstd/assert.hh"
#include "kstd/math.hh"

#include "gameos/synchronization.hh"

namespace mem {

//
// Buddy heap over page-aligned physical regions. Call clear(), then
// add_region() for each usable range, before using it.
//
struct Buddy_Allocator_State {
    using enum Allocator_Features;
    static constexpr Allocator_Features FEATURES = THREADSAFE | FREE | ACTUALLY_RESIZE | INFO | GENERAL_HEAP_ALLOCATOR;

    struct Free_Block {
        Free_Block* next;
    };

    enum struct Allocation_State : u8 {
        LIVE  = 0xA5,
        FREED = 0x5A,
    };

    struct Allocation_Header {
        u64              block_base;
        u8               order;
        Allocation_State state;
        u8               reserved[4];
        u64              requested_size;
        u64              requested_alignment;
    };
    static_assert(size_of(Allocation_Header) == 4 * size_of(u64));

    static constexpr int MIN_ORDER = 12;  // 4 KiB pages.
    static constexpr int MAX_ORDER = 63;

    Free_Block* free_lists[MAX_ORDER + 1]{};
    // It's the main global allocator, so it has to have a lock on `alloc` and `free`.
    synchronization::Spinlock lock;

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = this };
    }

    static auto block_size_for_order(int order) -> u64 {
        return u64{1} << order;
    }

    static auto order_for_block_size(u64 block_size) -> int {
        int order   = MIN_ORDER;
        int current = mem::PAGE_SSIZE;
        while (current < block_size && order < MAX_ORDER) {
            current <<= 1;
            order++;
        }
        return order;
    }

    static auto next_block_size(u64 required_size) -> u64 {
        u64 block_size = mem::PAGE_USIZE;
        while (block_size < required_size && block_size < (u64{1} << MAX_ORDER))
            block_size <<= 1;
        return block_size;
    }

    static force_inline auto header_from_pointer(void* memory) -> Allocation_Header* {
        return cast(Allocation_Header*)(ptr_addr(memory) - size_of(Allocation_Header));
    }

    auto clear() -> void {
        for (int i = 0; i <= MAX_ORDER; ++i)
            free_lists[i] = nullptr;
    }

    auto push_free_block(u64 base, int order) -> void {
        auto* block = cast(Free_Block*)base;
        block->next = free_lists[order];
        free_lists[order] = block;
    }

    auto remove_free_block(int order, u64 base) -> bool {
        auto** link = &free_lists[order];
        while (*link != nullptr) {
            if (ptr_addr(*link) == base) {
                *link = (*link)->next;
                return true;
            }
            link = &((*link)->next);
        }
        return false;
    }

    auto is_free_block(int order, u64 base) const -> bool {
        for (auto* block = free_lists[order]; block != nullptr; block = block->next)
            if (ptr_addr(block) == base) return true;
        return false;
    }

    auto add_region(u64 base, u64 size) -> void {
        const u64 start = align_up(base, mem::PAGE_USIZE);
        const u64 end   = align_down(base + size, mem::PAGE_USIZE);
        if (start >= end) return;

        u64 current = start;
        while (current < end) {
            u64 block_size = math::floor_pow2(end - current);
            while (block_size > mem::PAGE_USIZE && (current & (block_size - 1)) != 0)
                block_size >>= 1;

            if (block_size < mem::PAGE_USIZE)
                block_size = mem::PAGE_USIZE;

            const int order = order_for_block_size(block_size);
            push_free_block(current, order);
            current += block_size;
        }
    }

    static auto proc(Allocator_Mode mode, ssize size, ssize alignment, ssize old_size, void* old_memory, void* buddy_state) -> Allocator_Result {
        auto* state = cast(Buddy_Allocator_State*)buddy_state;

        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (size < 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                const auto requested_size      = cast(u64)size;
                const auto requested_alignment = cast(u64)alignment;
                const u64  required_size       = requested_size + requested_alignment + size_of(Allocation_Header);
                if (required_size < requested_size)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                auto scoped_lock = state->lock.scoped_irq_lock();
                const u64 target_block_size = next_block_size(required_size);
                if (target_block_size < mem::PAGE_USIZE) return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                const int target_order = order_for_block_size(target_block_size);
                int order = target_order;
                while (order <= MAX_ORDER && state->free_lists[order] == nullptr)
                    order++;

                if (order > MAX_ORDER)
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                u64 block_base = ptr_addr(state->free_lists[order]);
                state->free_lists[order] = state->free_lists[order]->next;
                while (order > target_order) {
                    order--;
                    const u64 split_size = block_size_for_order(order);
                    state->push_free_block(block_base + split_size, order);
                }

                const u64 block_size = block_size_for_order(order);
                const u64 user_ptr = align_up(block_base + size_of(Allocation_Header), requested_alignment);
                if (user_ptr + requested_size < user_ptr || user_ptr + requested_size > block_base + block_size) {
                    state->push_free_block(block_base, order);
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);
                }

                auto* header = header_from_pointer(cast(void*)user_ptr);
                new (header) Allocation_Header {
                    .block_base          = block_base,
                    .order               = cast(u8)order,
                    .state               = Allocation_State::LIVE,
                    .reserved            = {},
                    .requested_size      = requested_size,
                    .requested_alignment = requested_alignment,
                };
                return result(cast(void*)user_ptr);
            } break;

            case Allocator_Mode::FREE: {
                if (old_memory == nullptr) return result(nullptr);

                auto scoped_lock = state->lock.scoped_irq_lock();
                auto* header_pointer = header_from_pointer(old_memory);
                Allocation_Header header{};
                kstd_memcpy(&header, header_pointer, size_of(header));
                kstd_assert(header.state == Allocation_State::LIVE, "Buddy allocator: double free");
                header_pointer->state = Allocation_State::FREED;

                u64 block_base = header.block_base;
                int order = header.order;
                while (order < MAX_ORDER) {
                    const u64 block_size = block_size_for_order(order);
                    const u64 buddy_base = block_base ^ block_size;
                    if (!state->remove_free_block(order, buddy_base)) break;
                    if (buddy_base < block_base) block_base = buddy_base;
                    order++;
                }

                state->push_free_block(block_base, order);
                return result(nullptr);
            } break;

            case Allocator_Mode::RESIZE: {
                if (size < 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                if (old_memory == nullptr)
                    return proc(Allocator_Mode::ALLOCATE, size, alignment, 0, nullptr, buddy_state);

                // Native resize must return a pointer with the requested alignment.
                if (ptr_addr(old_memory) % alignment != 0)
                    return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);

                auto scoped_lock = state->lock.scoped_irq_lock();

                auto* header = header_from_pointer(old_memory);
                const u64 block_base     = header->block_base;
                const int old_order      = header->order;
                const u64 old_block_size = block_size_for_order(old_order);
                const u64 available      = block_base + old_block_size - ptr_addr(old_memory);

                // Update metadata when the current block already has enough space.
                if (size <= available && alignment == header->requested_alignment) {
                    header->requested_size = size;
                    return result(old_memory);
                }

                // Find the smallest buddy block that can hold the resized allocation.
                const u64 required_size = size + alignment + size_of(Allocation_Header);
                if (required_size < size)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                const int target_order = order_for_block_size(next_block_size(required_size));
                if (target_order <= old_order || target_order > MAX_ORDER)
                    return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);

                // Check every required buddy before removing any free block to avoid partial state mutation.
                u64 new_block_base = block_base;
                int order          = old_order;
                while (order < target_order) {
                    const u64 buddy_base = new_block_base ^ block_size_for_order(order);
                    if (!state->is_free_block(order, buddy_base))
                        return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
                    if (buddy_base < new_block_base)
                        new_block_base = buddy_base;
                    order++;
                }

                // Remove checked buddies and compute the base of the merged block.
                new_block_base = block_base;
                order          = old_order;
                while (order < target_order) {
                    const u64 buddy_base = new_block_base ^ block_size_for_order(order);
                    state->remove_free_block(order, buddy_base);
                    if (buddy_base < new_block_base)
                        new_block_base = buddy_base;
                    order++;
                }

                // Recompute the user pointer because the merged block can start lower.
                const u64 new_user_address = align_up(new_block_base + size_of(Allocation_Header), cast(u64)alignment);
                const u64 new_block_size   = block_size_for_order(target_order);
                if (new_user_address + size < new_user_address || new_user_address + size > new_block_base + new_block_size) {
                    return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
                }

                // Write new metadata, copy the payload, and retire the old header.
                auto* new_header = header_from_pointer(cast(void*)new_user_address);
                new (new_header) Allocation_Header {
                    .block_base = new_block_base,
                    .order      = cast(u8)target_order,
                    .state      = Allocation_State::LIVE,
                    .reserved   = {},
                    .requested_size      = cast(u64)size,
                    .requested_alignment = cast(u64)alignment,
                };
                if (new_user_address != ptr_addr(old_memory) && old_size > 0)
                    kstd_memcpy(cast(void*)new_user_address, old_memory, old_size < size ? old_size : size);
                header->state = Allocation_State::FREED;
                return result(cast(void*)new_user_address);
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = cast(Allocator_Features*)old_memory;
                if (features != nullptr) *features = FEATURES;
                else                     return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                return result(nullptr);
            } break;

            case Allocator_Mode::IS_THIS_YOURS: {
                if (old_memory == nullptr) return result(nullptr);

                auto scoped_lock = state->lock.scoped_irq_lock();

                auto* header = header_from_pointer(old_memory);
                if (header->state != Allocation_State::LIVE)
                    return result(nullptr);

                if (
                    header->order < MIN_ORDER
                    || header->order > MAX_ORDER
                    || !math::is_power_of_two(header->requested_alignment)
                )
                    return result(nullptr);

                const u64 block_base   = header->block_base;
                const u64 block_size   = block_size_for_order(header->order);
                const u64 user_address = align_up(block_base + size_of(Allocation_Header), header->requested_alignment);
                if (
                    ptr_addr(old_memory) != user_address
                    || user_address + header->requested_size < user_address
                    || user_address + header->requested_size > block_base + block_size
                )
                    return result(nullptr);

                return result(old_memory);
            } break;

            case Allocator_Mode::INFO: {
                auto* info = cast(Allocator_Info*)old_memory;
                if (info == nullptr || info->pointer == nullptr)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                auto scoped_lock = state->lock.scoped_irq_lock();
                auto* header = header_from_pointer(info->pointer);
                if (header->state != Allocation_State::LIVE)
                    return result(nullptr, Allocator_Error::INVALID_POINTER);

                info->requested_size      = cast(ssize)header->requested_size;
                info->requested_alignment = cast(ssize)header->requested_alignment;

                return result(nullptr);
            } break;

            default: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
        }

        unreachable();
    }
};

inline Buddy_Allocator_State buddy{};

}  // namespace mem
