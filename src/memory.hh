#pragma once

#include <cstddef>

#include "int.hh"
#include "terminal.hh"

namespace mem {

struct Memory_Region {
    u64 base;
    u64 size;
};

struct Multiboot_Info {
    u32 flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    u32 syms[4];
    u32 mmap_length;
    u32 mmap_addr;
} __attribute__((packed));

struct Multiboot_MMAP_Entry {
    u32 size;
    u64 addr;
    u64 len;
    u32 type;
} __attribute__((packed));

struct Multiboot_Module {
    u32 mod_start;
    u32 mod_end;
    u32 string;
    u32 reserved;
} __attribute__((packed));

static constexpr int MULTIBOOT_INFO_HAS_MMAP = 1u << 6;
static constexpr int MULTIBOOT_MMAP_USABLE = 1u;
static constexpr int PAGE_SIZE = 4096;
static constexpr int MAX_MEMORY_REGIONS = 32;

extern "C" u8 __kernel_start;
extern "C" u8 __kernel_end;

static u64 align_up(u64 value, u64 alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static u64 align_down(u64 value, u64 alignment) {
    return value & ~(alignment - 1);
}

static void add_usable_region(Array<Memory_Region>& regions, u64 base, u64 length) {
    if (length == 0 || regions.size >= MAX_MEMORY_REGIONS) return;
    regions[regions.size++] = {base, length};
}

static void reserve_range(Array<Memory_Region>& regions, u64 start, u64 end) {
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

static void parse_multiboot_memory_map(Array<Memory_Region>& regions, const Multiboot_Info* mbi) {
    if ((mbi->flags & MULTIBOOT_INFO_HAS_MMAP) == 0) return;

    auto* entry = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr);
    const auto* end = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr + mbi->mmap_length);

    while (reinterpret_cast<uintptr_t>(entry) < reinterpret_cast<uintptr_t>(end)) {
        const u64 entry_length = entry->len;
        if (entry->type == MULTIBOOT_MMAP_USABLE) add_usable_region(regions, entry->addr, entry_length);

        entry = reinterpret_cast<const Multiboot_MMAP_Entry*>(
            reinterpret_cast<uintptr_t>(entry) + entry->size + sizeof(entry->size));
    }
}

static void reserve_multiboot_data(Array<Memory_Region>& regions, const Multiboot_Info* mbi) {
    reserve_range(regions, 0, PAGE_SIZE);
    reserve_range(regions, reinterpret_cast<uintptr_t>(mbi), reinterpret_cast<uintptr_t>(mbi) + sizeof(Multiboot_Info));
    reserve_range(regions, reinterpret_cast<uintptr_t>(&__kernel_start), reinterpret_cast<uintptr_t>(&__kernel_end));
    reserve_range(regions, (uintptr_t)mbi->mmap_addr, (uintptr_t)mbi->mmap_addr + mbi->mmap_length);

    if (mbi->cmdline != 0) {
        reserve_range(
            regions,
            (uintptr_t)mbi->cmdline,
            (uintptr_t)mbi->cmdline + strlen((const char*)(uintptr_t)mbi->cmdline) + 1);
    }

    if (mbi->mods_count == 0 || mbi->mods_addr == 0) return;

    auto* modules = reinterpret_cast<const Multiboot_Module*>((uintptr_t)mbi->mods_addr);
    reserve_range(regions, (uintptr_t)modules, (uintptr_t)modules + mbi->mods_count * sizeof(Multiboot_Module));

    for (u32 i = 0; i < mbi->mods_count; i++) {
        reserve_range(regions, modules[i].mod_start, modules[i].mod_end);
        if (modules[i].string != 0) {
            reserve_range(
                regions, modules[i].string, modules[i].string + strlen((const char*)(uintptr_t)modules[i].string) + 1);
        }
    }
}

static Array<Memory_Region> __regions;

void memory_initialize(const Multiboot_Info* mbi) {
    static Memory_Region __usable_regions[MAX_MEMORY_REGIONS];
    __regions = {0, __usable_regions};
    parse_multiboot_memory_map(__regions, mbi);
    reserve_multiboot_data(__regions, mbi);
}

u64 floor_pow2(u64 n) {
    if (n == 0) return 0;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n & ~(n >> 1);
}

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
        u8 reserved[7];
    };

    static constexpr usize MIN_ORDER = 12;  // 4 KiB pages
    static constexpr usize MAX_ORDER = 63;

    Free_Block* free_lists[MAX_ORDER + 1]{};

    static u64 block_size_for_order(usize order) {
        return 1ull << order;
    }

    static usize order_for_block_size(u64 block_size) {
        usize order = MIN_ORDER;
        u64 current = PAGE_SIZE;
        while (current < block_size && order < MAX_ORDER) {
            current <<= 1;
            order++;
        }
        return order;
    }

    static u64 next_block_size(u64 required_size) {
        u64 block_size = PAGE_SIZE;
        while (block_size < required_size && block_size < (1ull << MAX_ORDER)) {
            block_size <<= 1;
        }
        return block_size;
    }

    void clear() {
        for (usize i = 0; i <= MAX_ORDER; ++i) free_lists[i] = nullptr;
    }

    void push_free_block(u64 base, usize order) {
        auto* block = reinterpret_cast<Free_Block*>((uintptr_t)base);
        block->next = free_lists[order];
        free_lists[order] = block;
    }

    bool remove_free_block(usize order, u64 base) {
        auto** link = &free_lists[order];
        while (*link != nullptr) {
            if (reinterpret_cast<uintptr_t>(*link) == base) {
                *link = (*link)->next;
                return true;
            }
            link = &((*link)->next);
        }

        return false;
    }

    void add_region(u64 base, u64 size) {
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

    void init() {
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

        u64 block_base = reinterpret_cast<uintptr_t>(free_lists[order]);
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

        auto* header = reinterpret_cast<Allocation_Header*>((uintptr_t)(user_ptr - sizeof(Allocation_Header)));
        header->block_base = block_base;
        header->order = static_cast<u8>(order);
        header->reserved[0] = 0;
        header->reserved[1] = 0;
        header->reserved[2] = 0;
        header->reserved[3] = 0;
        header->reserved[4] = 0;
        header->reserved[5] = 0;
        header->reserved[6] = 0;

        term::terminal_writestring("\nbruh: ");
        term::terminal_writeint(user_ptr);
        term::terminal_writestring(", ");
        term::terminal_writeint(size);
        term::terminal_writestring("\n");
        return reinterpret_cast<void*>((uintptr_t)user_ptr);
    }

    auto free(void* pointer, usize, usize alignment = alignof(std::max_align_t)) -> void override {
        (void)alignment;
        if (pointer == nullptr) return;

        Allocation_Header header{};
        __builtin_memcpy(
            &header,
            reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(pointer) - sizeof(Allocation_Header)),
            sizeof(header));

        u64 block_base = header.block_base;
        usize order = header.order;

        while (order < MAX_ORDER) {
            const u64 block_size = block_size_for_order(order);
            const u64 buddy_base = block_base ^ block_size;
            if (!remove_free_block(order, buddy_base)) break;

            if (buddy_base < block_base) block_base = buddy_base;
            order++;
        }

        term::terminal_writestring("\nbruh 2: ");
        term::terminal_writeint(reinterpret_cast<u64>(pointer));
        term::terminal_writestring("\n");
        push_free_block(block_base, order);
    }
};

constexpr usize DEFAULT_RESERVE_SIZE = 1 * 1024 * 1024;
constexpr usize DEFAULT_PAGE_SIZE = 4096;

template <bool DEBUG = false>
struct Arena_Allocator final : Allocator {
    std::byte* memory_base;
    std::byte* current_point;
    std::byte* address_limit;

    void init(usize reserve = DEFAULT_RESERVE_SIZE) {
        reserve = align_up(reserve, DEFAULT_PAGE_SIZE);

        term::terminal_writestring("arena reserve ");
        term::terminal_writeint((s64)reserve);
        term::terminal_writestring("\n");

        memory_base = new std::byte[reserve];
        assert(memory_base != nullptr);
        current_point = memory_base;
        address_limit = memory_base + reserve;

        term::terminal_writestring("arena base ");
        term::terminal_write_hex((u64)(uintptr_t)memory_base);
        term::terminal_writestring("\n");
    }

    ~Arena_Allocator() {
        reset();
    }

    void reset() {
        if constexpr (DEBUG) {
            const auto STAMP = 0xCC;
            __builtin_memset(memory_base, STAMP, current_point - memory_base);
            // std::memset(memory_base, STAMP, current_point - memory_base);
        }
        current_point = memory_base;
    };

    usize bytes_left() {
        return address_limit - current_point;
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;

        term::terminal_writestring("arena alloc ");
        term::terminal_writeint((s64)size);
        term::terminal_writestring(" align ");
        term::terminal_writeint((s64)alignment);
        term::terminal_writestring("\n");

        auto* aligned_point =
            reinterpret_cast<std::byte*>(align_up(reinterpret_cast<uintptr_t>(current_point), alignment));
        auto* new_point = aligned_point + size;

        assert(new_point <= address_limit);
        current_point = new_point;

        term::terminal_writestring("arena out ");
        term::terminal_write_hex((u64)(uintptr_t)aligned_point);
        term::terminal_writestring("\n");

        return static_cast<void*>(aligned_point);
    };

    auto free(void* pointer, usize size, usize alignment = alignof(std::max_align_t)) -> void override {
        (void)pointer;
        (void)size;
        (void)alignment;
    };
};

}  // namespace mem
