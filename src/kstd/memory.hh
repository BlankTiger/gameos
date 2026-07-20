#pragma once

#include <cstddef>

template <typename T>
inline auto ptr_addr(const T* pointer) -> uintptr_t {
    return reinterpret_cast<uintptr_t>(pointer);
}

#include "multiboot2.hh"

namespace mem {

static constexpr int MULTIBOOT_MMAP_USABLE = 1u;
static constexpr int PAGE_SIZE = 4096;
static constexpr int MAX_MEMORY_REGIONS = 32;

struct Memory_Region {
    u64 base;
    u64 size;
};

using Memory_Regions = Bounded_Array<Memory_Region, MAX_MEMORY_REGIONS>;

extern "C" u8 __kernel_start;
extern "C" u8 __kernel_end;

static auto align_up(u64 value, u64 alignment) -> u64 {
    return (value + alignment - 1) & ~(alignment - 1);
}

static auto align_down(u64 value, u64 alignment) -> u64 {
    return value & ~(alignment - 1);
}

static auto add_usable_region(Memory_Regions& regions, u64 base, u64 length) -> void {
    regions.push_back({base, length});
}

static auto reserve_range(Memory_Regions& regions, u64 start, u64 end) -> void {
    if (start >= end) return;

    start = align_down(start, PAGE_SIZE);
    end = align_up(end, PAGE_SIZE);

    for (usize i = 0; i < regions.size;) {
        const u64 region_start = regions[i].base;
        const u64 region_end = region_start + regions[i].size;

        if (end <= region_start || start >= region_end) {
            i++;
            continue;
        }

        if (start <= region_start && end >= region_end) {
            regions[i] = regions[--regions.size];
            continue;
        }

        if (start <= region_start) {
            regions[i].base = end;
            regions[i].size = region_end - end;
            i++;
            continue;
        }

        if (end >= region_end) {
            regions[i].size = start - region_start;
            i++;
            continue;
        }

        if (regions.size < MAX_MEMORY_REGIONS) {
            regions[regions.size++] = {end, region_end - end};
            regions[i].size = start - region_start;
            i++;
        } else {
            regions[i].size = start - region_start;
            i++;
        }
    }
}

static auto parse_multiboot2_memory_map(Memory_Regions& regions, const boot::Multiboot2_Info* mbi) -> void {
    auto* tag = mbi->first_tag();
    const auto* end = mbi->end_tag();

    while (ptr_addr(tag) < ptr_addr(end)) {
        if (tag->type == boot::Multiboot2_Tag_Type::MEMORY_MAP) {
            const auto* mmap_tag = tag->as<boot::Multiboot2_Memory_Map_Tag>();
            const auto* entry = mmap_tag->first_entry();
            const auto* tag_end = mmap_tag->end_entry();

            while (ptr_addr(entry) < ptr_addr(tag_end)) {
                if (entry->type == MULTIBOOT_MMAP_USABLE) add_usable_region(regions, entry->addr, entry->len);
                entry = reinterpret_cast<const boot::Multiboot2_Memory_Map_Entry*>(ptr_addr(entry) + mmap_tag->entry_size);
            }
        }

        tag = tag->next();
    }
}

static auto reserve_multiboot2_data(Memory_Regions& regions, const boot::Multiboot2_Info* mbi) -> void {
    reserve_range(regions, 0, PAGE_SIZE);
    reserve_range(regions, ptr_addr(mbi), ptr_addr(mbi) + mbi->total_size);
    reserve_range(regions, ptr_addr(&__kernel_start), ptr_addr(&__kernel_end));

    auto* tag = mbi->first_tag();
    const auto* end = mbi->end_tag();

    while (ptr_addr(tag) < ptr_addr(end)) {
        const auto tag_type = static_cast<boot::Multiboot2_Tag_Type>(tag->type);
        if (tag_type == boot::Multiboot2_Tag_Type::CMDLINE || tag_type == boot::Multiboot2_Tag_Type::BOOT_LOADER_NAME) {
            const auto* text = tag->payload_as<char>();
            reserve_range(regions, ptr_addr(text), ptr_addr(text) + strlen(text) + 1);
        } else if (tag_type == boot::Multiboot2_Tag_Type::MODULE) {
            const auto* module = tag->as<boot::Multiboot2_Module_Tag>();
            reserve_range(regions, module->mod_start, module->mod_end);
            if (module->string != 0) {
                reserve_range(regions, module->string, module->string + strlen(module->string_ptr()) + 1);
            }
        }

        tag = tag->next();
    }
}

auto floor_pow2(u64 n) -> u64 {
    if (n == 0) return 0;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n & ~(n >> 1);
}

static Memory_Regions __regions{};

struct Allocator {
    virtual auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* = 0;
    virtual auto free(void* pointer, usize size, usize alignment = alignof(std::max_align_t)) -> void = 0;
    virtual ~Allocator() = default;
};

struct Buddy_Allocator final : Allocator {
    struct Free_Block {
        Free_Block* next;
    };

    struct Allocation_Header {
        u64 block_base;
        u8 order;
        Static_Array<u8, 7> reserved;
    };

    static constexpr usize MIN_ORDER = 12;  // 4 KiB pages
    static constexpr usize MAX_ORDER = 63;

    Static_Array<Free_Block*, MAX_ORDER + 1> free_lists{};

    static auto block_size_for_order(usize order) -> usize {
        return 1ull << order;
    }

    static auto order_for_block_size(u64 block_size) -> usize {
        usize order = MIN_ORDER;
        u64 current = PAGE_SIZE;
        while (current < block_size && order < MAX_ORDER) {
            current <<= 1;
            order++;
        }
        return order;
    }

    static auto next_block_size(u64 required_size) -> u64 {
        u64 block_size = PAGE_SIZE;
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
        const u64 start = align_up(base, PAGE_SIZE);
        const u64 end = align_down(base + size, PAGE_SIZE);
        if (start >= end) return;

        u64 current = start;
        while (current < end) {
            u64 block_size = floor_pow2(end - current);
            while (block_size > PAGE_SIZE && (current & (block_size - 1)) != 0) {
                block_size >>= 1;
            }

            if (block_size < PAGE_SIZE) block_size = PAGE_SIZE;

            const usize order = order_for_block_size(block_size);
            push_free_block(current, order);
            current += block_size;
        }
    }

    auto init() -> void {
        clear();

        for (const auto& region : __regions) {
            add_region(region.base, region.size);
        }
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;
        if (size == 0) size = 1;

        const u64 required_size = static_cast<u64>(size) + static_cast<u64>(alignment) + sizeof(Allocation_Header);
        if (required_size < size) return nullptr;

        const u64 target_block_size = next_block_size(required_size);
        if (target_block_size < PAGE_SIZE) return nullptr;

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
        const u64 user_ptr = align_up(block_base + sizeof(Allocation_Header), alignment);
        if (user_ptr + size > block_base + block_size) {
            push_free_block(block_base, order);
            return nullptr;
        }

        auto* header = reinterpret_cast<Allocation_Header*>(user_ptr - sizeof(Allocation_Header));
        header->block_base = block_base;
        header->order = static_cast<u8>(order);
        header->reserved[0] = 0;
        header->reserved[1] = 0;
        header->reserved[2] = 0;
        header->reserved[3] = 0;
        header->reserved[4] = 0;
        header->reserved[5] = 0;
        header->reserved[6] = 0;

        return reinterpret_cast<void*>(user_ptr);
    }

    auto free(void* pointer, usize, usize alignment = alignof(std::max_align_t)) -> void override {
        (void)alignment;
        if (pointer == nullptr) return;

        Allocation_Header header{};
        memcpy(&header, reinterpret_cast<void*>(ptr_addr(pointer) - sizeof(Allocation_Header)), sizeof(header));

        u64 block_base = header.block_base;
        usize order = header.order;

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

constexpr usize DEFAULT_RESERVE_SIZE = 1 * 1024 * 1024;
constexpr usize DEFAULT_PAGE_SIZE = 4096;

template <bool DEBUG = false>
struct Arena_Allocator final : Allocator {
    // @TODO: unique_ptr?
    std::byte* memory_base;
    std::byte* current_point;
    std::byte* address_limit;

    usize allocated_;
    Allocator* backing_allocator_;

    auto init(Allocator* backing_allocator, usize reserve = DEFAULT_RESERVE_SIZE) -> void {
        allocated_ = align_up(reserve, DEFAULT_PAGE_SIZE);
        memory_base = static_cast<std::byte*>(backing_allocator->alloc(allocated_));
        assert(memory_base != nullptr);
        current_point = memory_base;
        address_limit = memory_base + allocated_;
        backing_allocator_ = backing_allocator;
    }

    ~Arena_Allocator() {
        reset();
        backing_allocator_->free(memory_base, allocated_);
    }

    auto reset() -> void {
        if constexpr (DEBUG) {
            const auto STAMP = 0xCC;
            memset(memory_base, STAMP, current_point - memory_base);
        }
        current_point = memory_base;
    };

    auto bytes_left() -> usize {
        return address_limit - current_point;
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;

        auto* aligned_point = reinterpret_cast<std::byte*>(align_up(ptr_addr(current_point), alignment));
        auto* new_point = aligned_point + size;

        if (new_point > address_limit) return nullptr;
        current_point = new_point;

        return static_cast<void*>(aligned_point);
    };

    auto free(void* pointer, usize size, usize alignment = alignof(std::max_align_t)) -> void override {
        (void)pointer;
        (void)size;
        (void)alignment;
    };
};

mem::Allocator* __global_allocator;
mem::Buddy_Allocator __buddy;

auto initialize(const boot::Multiboot2_Info* mbi) -> void {
    parse_multiboot2_memory_map(__regions, mbi);
    reserve_multiboot2_data(__regions, mbi);

    __buddy.init();
    __global_allocator = &__buddy;
}

}  // namespace mem
