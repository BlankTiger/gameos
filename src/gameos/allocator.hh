#pragma once

#include "kstd/allocator.hh"

#include "gameos/assert.hh"
#include "gameos/synchronization.hh"

namespace mem {

//
// Buddy heap over page-aligned physical regions. Call clear(), then
// add_region() for each usable range, before alloc.
//
struct Buddy_Allocator final : Allocator {
    struct Free_Block {
        Free_Block* next;
    };

    struct Allocation_Header {
        u64 block_base;
        u8  order;
        u8  reserved[7];
    };

    static constexpr usize MIN_ORDER = 12;  // 4 KiB pages.
    static constexpr usize MAX_ORDER = 63;

    Free_Block* free_lists[MAX_ORDER + 1]{};
    // It's the main global allocator, so it has to have a lock on `alloc` and `free`.
    synchronization::Spinlock lock;

    static auto floor_pow2(u64 n) -> u64 {
        if (n == 0) return 0;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n & ~(n >> 1);
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
        while (block_size < required_size && block_size < (1ull << MAX_ORDER)) {
            block_size <<= 1;
        }
        return block_size;
    }

    auto clear() -> void {
        for (usize i = 0; i <= MAX_ORDER; ++i) free_lists[i] = nullptr;
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
            u64 block_size = floor_pow2(end - current);
            while (block_size > mem::PAGE_SIZE && (current & (block_size - 1)) != 0) {
                block_size >>= 1;
            }

            if (block_size < mem::PAGE_SIZE) block_size = mem::PAGE_SIZE;

            const usize order = order_for_block_size(block_size);
            push_free_block(current, order);
            current += block_size;
        }
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;
        if (size == 0) size = 1;

        const u64 required_size = static_cast<u64>(size) + static_cast<u64>(alignment) + sizeof(Allocation_Header);
        if (required_size < size) return nullptr;

        auto scoped_lock = lock.scoped_irq_lock();

        const u64 target_block_size = next_block_size(required_size);
        if (target_block_size < mem::PAGE_SIZE) return nullptr;

        const usize target_order = order_for_block_size(target_block_size);

        usize order = target_order;
        while (order <= MAX_ORDER && free_lists[order] == nullptr) order++;
        if (order > MAX_ORDER) return nullptr;

        u64 block_base = ptr_addr(free_lists[order]);
        free_lists[order] = free_lists[order]->next;

        while (order > target_order) {
            order--;
            const u64 split_size = block_size_for_order(order);
            push_free_block(block_base + split_size, order);
        }

        const u64 block_size = block_size_for_order(order);
        const u64 user_ptr   = align_up(block_base + sizeof(Allocation_Header), static_cast<u64>(alignment));
        if (user_ptr + size > block_base + block_size) {
            push_free_block(block_base, order);
            return nullptr;
        }

        auto* header = reinterpret_cast<Allocation_Header*>(user_ptr - sizeof(Allocation_Header));
        *header = Allocation_Header{};
        header->block_base = block_base;
        header->order      = static_cast<u8>(order);
        return reinterpret_cast<void*>(user_ptr);
    }

    auto free(void* pointer, usize, usize alignment = alignof(std::max_align_t)) -> void override {
        (void)alignment;
        if (pointer == nullptr) return;

        auto scoped_lock = lock.scoped_irq_lock();

        Allocation_Header header{};
        kstd_memcpy(&header, reinterpret_cast<void*>(ptr_addr(pointer) - sizeof(Allocation_Header)), sizeof(header));

        u64   block_base = header.block_base;
        usize order      = header.order;

        while (order < MAX_ORDER) {
            const u64 block_size = block_size_for_order(order);
            const u64 buddy_base = block_base ^ block_size;
            if (!remove_free_block(order, buddy_base)) break;

            if (buddy_base < block_base) block_base = buddy_base;
            order++;
        }

        push_free_block(block_base, order);
    }
};


inline Buddy_Allocator buddy{};

}  // namespace mem
