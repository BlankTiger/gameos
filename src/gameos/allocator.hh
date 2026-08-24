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
    static constexpr Allocator_Features FEATURES = THREADSAFE | FREE | INFO | GENERAL_HEAP_ALLOCATOR;

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
        u8               reserved[6];
    };

    static constexpr usize MIN_ORDER = 12;
    static constexpr usize MAX_ORDER = 63;

    Free_Block* free_lists[MAX_ORDER + 1]{};
    synchronization::Spinlock lock;

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = this };
    }

    static auto block_size_for_order(usize order) -> usize {
        return 1ull << order;
    }

    static auto order_for_block_size(u64 block_size) -> usize {
        usize order   = MIN_ORDER;
        u64   current = mem::PAGE_SIZE;
        while (current < block_size && order < MAX_ORDER) {
            current <<= 1;
            order++;
        }
        return order;
    }

    static auto next_block_size(u64 required_size) -> u64 {
        u64 block_size = mem::PAGE_SIZE;
        while (block_size < required_size && block_size < (1ull << MAX_ORDER))
            block_size <<= 1;
        return block_size;
    }

    auto clear() -> void {
        for (usize i = 0; i <= MAX_ORDER; ++i)
            free_lists[i] = nullptr;
    }

    auto push_free_block(u64 base, usize order) -> void {
        auto* block = reinterpret_cast<Free_Block*>(base);
        block->next = free_lists[order];
        free_lists[order] = block;
    }

    auto remove_free_block(usize order, u64 base) -> bool {
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

    auto add_region(u64 base, u64 size) -> void {
        const u64 start = align_up(base, static_cast<u64>(mem::PAGE_SIZE));
        const u64 end   = align_down(base + size, static_cast<u64>(mem::PAGE_SIZE));
        if (start >= end) return;

        u64 current = start;
        while (current < end) {
            u64 block_size = math::floor_pow2(end - current);
            while (block_size > mem::PAGE_SIZE && (current & (block_size - 1)) != 0)
                block_size >>= 1;

            if (block_size < mem::PAGE_SIZE)
                block_size = mem::PAGE_SIZE;

            const usize order = order_for_block_size(block_size);
            push_free_block(current, order);
            current += block_size;
        }
    }

    static auto proc(Allocator_Mode mode, s64 size, s64 alignment, s64, void* old_memory, void* buddy_state) -> Allocator_Result {
        auto* state = static_cast<Buddy_Allocator_State*>(buddy_state);

        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                const u64 requested_size      = static_cast<u64>(size);
                const u64 requested_alignment = static_cast<u64>(alignment);
                const u64 required_size       = requested_size + requested_alignment + sizeof(Allocation_Header);
                if (required_size < requested_size)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                auto scoped_lock = state->lock.scoped_irq_lock();
                const u64 target_block_size = next_block_size(required_size);
                if (target_block_size < mem::PAGE_SIZE) return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                const usize target_order = order_for_block_size(target_block_size);
                usize order = target_order;
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
                const u64 user_ptr = align_up(block_base + sizeof(Allocation_Header), requested_alignment);
                if (user_ptr + requested_size < user_ptr || user_ptr + requested_size > block_base + block_size) {
                    state->push_free_block(block_base, order);
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);
                }

                auto* header = reinterpret_cast<Allocation_Header*>(user_ptr - sizeof(Allocation_Header));
                new (header) Allocation_Header {
                    .block_base = block_base,
                    .order      = static_cast<u8>(order),
                    .state      = Allocation_State::LIVE,
                    .reserved   = {},
                };
                return result(reinterpret_cast<void*>(user_ptr));
            } break;

            case Allocator_Mode::FREE: {
                if (old_memory == nullptr) return result(nullptr);

                auto scoped_lock = state->lock.scoped_irq_lock();
                auto* header_pointer = reinterpret_cast<Allocation_Header*>(ptr_addr(old_memory) - sizeof(Allocation_Header));
                Allocation_Header header{};
                kstd_memcpy(&header, header_pointer, sizeof(header));
                kstd_assert(header.state == Allocation_State::LIVE, "Buddy allocator: double free");
                header_pointer->state = Allocation_State::FREED;

                u64 block_base = header.block_base;
                usize order = header.order;
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

            case Allocator_Mode::FEATURES: {
                auto* features = static_cast<Allocator_Features*>(old_memory);
                if (features != nullptr) *features = FEATURES;
                else                     return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                return result(nullptr);
            } break;

            // @TODO(blanktiger): Can and should be implemented.
            case Allocator_Mode::IS_THIS_YOURS:
                return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);

            case Allocator_Mode::INFO: {
                auto* info = static_cast<Allocator_Info*>(old_memory);
                if (info == nullptr || info->pointer == nullptr)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                auto scoped_lock = state->lock.scoped_irq_lock();
                Allocation_Header header{};
                kstd_memcpy(&header, reinterpret_cast<void*>(ptr_addr(info->pointer) - sizeof(Allocation_Header)), sizeof(header));
                info->size      = static_cast<s64>(block_size_for_order(header.order));
                info->alignment = static_cast<s64>(mem::PAGE_SIZE);

                return result(nullptr);
            } break;

            default: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
        }

        unreachable();
    }
};

inline Buddy_Allocator_State buddy{};

}  // namespace mem
