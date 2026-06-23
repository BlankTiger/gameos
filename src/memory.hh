#pragma once

#include <cstddef>

#include "int.hh"
#include "vga.hh"

namespace mem {

enum class Multiboot_Info_Flag : u32 {
    HAS_MMAP = 1u << 6,
    HAS_VBE = 1u << 11,
    HAS_FRAMEBUFFER = 1u << 12,
};

constexpr u32 MULTIBOOT2_MAGIC = 0x36D76289;

static constexpr int MULTIBOOT_MMAP_USABLE = 1u;
static constexpr int PAGE_SIZE = 4096;
static constexpr int MAX_MEMORY_REGIONS = 32;

struct Memory_Region {
    u64 base;
    u64 size;
};

using Memory_Regions = Bounded_Array<Memory_Region, MAX_MEMORY_REGIONS>;

struct Multiboot2_Info {
    u32 total_size;
    u32 reserved;
} __attribute__((packed));

struct Multiboot2_Tag {
    u16 type;
    u16 flags;
    u32 size;
} __attribute__((packed));

enum class Multiboot2_Tag_Type : u16 {
    END = 0,
    CMDLINE = 1,
    BOOT_LOADER_NAME = 2,
    MODULE = 3,
    BASIC_MEMINFO = 4,
    BOOT_DEVICE = 5,
    MEMORY_MAP = 6,
    VBE = 7,
    FRAMEBUFFER = 8,
};

struct Multiboot2_Memory_Map_Tag {
    Multiboot2_Tag tag;
    u32 entry_size;
    u32 entry_version;
} __attribute__((packed));

struct Multiboot2_Memory_Map_Entry {
    u64 addr;
    u64 len;
    u32 type;
    u32 zero;
} __attribute__((packed));

struct Multiboot2_Module_Tag {
    Multiboot2_Tag tag;
    u32 mod_start;
    u32 mod_end;
    u32 string;
    u32 reserved;
} __attribute__((packed));

struct Multiboot2_Framebuffer_Tag {
    Multiboot2_Tag tag;
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8 framebuffer_bpp;
    u8 framebuffer_type;
    u16 reserved;
    union {
        struct {
            u32 framebuffer_palette_addr;
            u16 framebuffer_palette_num_colors;
        } palette;
        struct {
            u8 framebuffer_red_field_position;
            u8 framebuffer_red_mask_size;
            u8 framebuffer_green_field_position;
            u8 framebuffer_green_mask_size;
            u8 framebuffer_blue_field_position;
            u8 framebuffer_blue_mask_size;
        } direct_color;
    } framebuffer_info;
} __attribute__((packed));

struct Multiboot_Info {
    Multiboot_Info_Flag flags;
    u32 mem_lower;
    u32 mem_upper;
    u32 boot_device;
    u32 cmdline;
    u32 mods_count;
    u32 mods_addr;
    Static_Array<u32, 4> syms;
    u32 mmap_length;
    u32 mmap_addr;
    u32 drives_length;
    u32 drives_addr;
    u32 config_table;
    u32 boot_loader_name;
    u32 apm_table;
    u32 vbe_control_info;
    u32 vbe_mode_info;
    u16 vbe_mode;
    u16 vbe_interface_seg;
    u16 vbe_interface_off;
    u16 vbe_interface_len;
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8 framebuffer_bpp;
    u8 framebuffer_type;
    union {
        struct {
            u32 framebuffer_palette_addr;
            u16 framebuffer_palette_num_colors;
        } palette;
        struct {
            u8 framebuffer_red_field_position;
            u8 framebuffer_red_mask_size;
            u8 framebuffer_green_field_position;
            u8 framebuffer_green_mask_size;
            u8 framebuffer_blue_field_position;
            u8 framebuffer_blue_mask_size;
        } direct_color;
    } framebuffer_info;
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

// static auto parse_multiboot_memory_map(Memory_Regions& regions, const Multiboot_Info* mbi) -> void {
//     if (!has_flag(mbi->flags, Multiboot_Info_Flag::HAS_MMAP)) return;
//
//     auto* entry = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr);
//     const auto* end = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr + mbi->mmap_length);
//
//     while (reinterpret_cast<uintptr_t>(entry) < reinterpret_cast<uintptr_t>(end)) {
//         const u64 entry_length = entry->len;
//         if (entry->type == MULTIBOOT_MMAP_USABLE) add_usable_region(regions, entry->addr, entry_length);
//
//         entry = reinterpret_cast<const Multiboot_MMAP_Entry*>(
//             reinterpret_cast<uintptr_t>(entry) + entry->size + sizeof(entry->size));
//     }
// }

// static auto reserve_multiboot_data(Memory_Regions& regions, const Multiboot_Info* mbi) -> void {
//     reserve_range(regions, 0, PAGE_SIZE);
//     reserve_range(regions, reinterpret_cast<uintptr_t>(mbi), reinterpret_cast<uintptr_t>(mbi) + sizeof(Multiboot_Info));
//     reserve_range(regions, reinterpret_cast<uintptr_t>(&__kernel_start), reinterpret_cast<uintptr_t>(&__kernel_end));
//     reserve_range(regions, (uintptr_t)mbi->mmap_addr, (uintptr_t)mbi->mmap_addr + mbi->mmap_length);
//
//     if (mbi->cmdline != 0) {
//         reserve_range(
//             regions,
//             (uintptr_t)mbi->cmdline,
//             (uintptr_t)mbi->cmdline + strlen((const char*)(uintptr_t)mbi->cmdline) + 1);
//     }
//
//     if (mbi->mods_count == 0 || mbi->mods_addr == 0) return;
//
//     auto* modules = reinterpret_cast<const Multiboot_Module*>((uintptr_t)mbi->mods_addr);
//     reserve_range(regions, (uintptr_t)modules, (uintptr_t)modules + mbi->mods_count * sizeof(Multiboot_Module));
//
//     for (u32 i = 0; i < mbi->mods_count; i++) {
//         reserve_range(regions, modules[i].mod_start, modules[i].mod_end);
//         if (modules[i].string != 0) {
//             reserve_range(
//                 regions, modules[i].string, modules[i].string + strlen((const char*)(uintptr_t)modules[i].string) + 1);
//         }
//     }
// }

static auto parse_multiboot2_memory_map(Memory_Regions& regions, const Multiboot2_Info* mbi) -> void {
    auto* tag = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(mbi) + sizeof(Multiboot2_Info));
    const auto* end = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(mbi) + mbi->total_size);

    while (reinterpret_cast<uintptr_t>(tag) < reinterpret_cast<uintptr_t>(end)) {
        const auto tag_type = static_cast<Multiboot2_Tag_Type>(tag->type);
        if (tag_type == Multiboot2_Tag_Type::MEMORY_MAP) {
            const auto* mmap_tag = reinterpret_cast<const Multiboot2_Memory_Map_Tag*>(tag);
            const auto* entry = reinterpret_cast<const Multiboot2_Memory_Map_Entry*>(
                reinterpret_cast<uintptr_t>(mmap_tag) + sizeof(Multiboot2_Memory_Map_Tag));
            const auto* tag_end = reinterpret_cast<const Multiboot2_Memory_Map_Entry*>(
                reinterpret_cast<uintptr_t>(mmap_tag) + mmap_tag->tag.size);

            while (reinterpret_cast<uintptr_t>(entry) < reinterpret_cast<uintptr_t>(tag_end)) {
                if (entry->type == MULTIBOOT_MMAP_USABLE) add_usable_region(regions, entry->addr, entry->len);
                entry = reinterpret_cast<const Multiboot2_Memory_Map_Entry*>(
                    reinterpret_cast<uintptr_t>(entry) + mmap_tag->entry_size);
            }
        }

        tag = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(tag) + align_up(tag->size, 8));
    }
}

static auto reserve_multiboot2_data(Memory_Regions& regions, const Multiboot2_Info* mbi) -> void {
    reserve_range(regions, 0, PAGE_SIZE);
    reserve_range(regions, reinterpret_cast<uintptr_t>(mbi), reinterpret_cast<uintptr_t>(mbi) + mbi->total_size);
    reserve_range(regions, reinterpret_cast<uintptr_t>(&__kernel_start), reinterpret_cast<uintptr_t>(&__kernel_end));

    auto* tag = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(mbi) + sizeof(Multiboot2_Info));
    const auto* end = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(mbi) + mbi->total_size);

    while (reinterpret_cast<uintptr_t>(tag) < reinterpret_cast<uintptr_t>(end)) {
        const auto tag_type = static_cast<Multiboot2_Tag_Type>(tag->type);
        if (tag_type == Multiboot2_Tag_Type::CMDLINE || tag_type == Multiboot2_Tag_Type::BOOT_LOADER_NAME) {
            const auto* text = reinterpret_cast<const char*>(reinterpret_cast<uintptr_t>(tag) + sizeof(Multiboot2_Tag));
            reserve_range(regions, reinterpret_cast<uintptr_t>(text), reinterpret_cast<uintptr_t>(text) + strlen(text) + 1);
        } else if (tag_type == Multiboot2_Tag_Type::MODULE) {
            const auto* module = reinterpret_cast<const Multiboot2_Module_Tag*>(tag);
            reserve_range(regions, module->mod_start, module->mod_end);
            if (module->string != 0) {
                reserve_range(
                    regions,
                    module->string,
                    module->string + strlen((const char*)(uintptr_t)module->string) + 1);
            }
        }

        tag = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(tag) + align_up(tag->size, 8));
    }
}

auto find_multiboot2_framebuffer_tag(const Multiboot2_Info* mbi) -> const Multiboot2_Framebuffer_Tag* {
    auto* tag = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(mbi) + sizeof(Multiboot2_Info));
    const auto* end = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(mbi) + mbi->total_size);

    while (reinterpret_cast<uintptr_t>(tag) < reinterpret_cast<uintptr_t>(end)) {
        if (static_cast<Multiboot2_Tag_Type>(tag->type) == Multiboot2_Tag_Type::FRAMEBUFFER) {
            return reinterpret_cast<const Multiboot2_Framebuffer_Tag*>(tag);
        }

        tag = reinterpret_cast<const Multiboot2_Tag*>(reinterpret_cast<uintptr_t>(tag) + align_up(tag->size, 8));
    }

    return nullptr;
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
        auto* block = reinterpret_cast<Free_Block*>((uintptr_t)base);
        block->next = free_lists[order];
        free_lists[order] = block;
    }

    auto remove_free_block(usize order, u64 base) -> bool {
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

        return reinterpret_cast<void*>((uintptr_t)user_ptr);
    }

    auto free(void* pointer, usize, usize alignment = alignof(std::max_align_t)) -> void override {
        (void)alignment;
        if (pointer == nullptr) return;

        Allocation_Header header{};
        memcpy(
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
            // std::memset(memory_base, STAMP, current_point - memory_base);
        }
        current_point = memory_base;
    };

    auto bytes_left() -> usize {
        return address_limit - current_point;
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;

        auto* aligned_point =
            reinterpret_cast<std::byte*>(align_up(reinterpret_cast<uintptr_t>(current_point), alignment));
        auto* new_point = aligned_point + size;

        assert(new_point <= address_limit);
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

auto memory_initialize(const Multiboot2_Info* mbi) -> void {
    parse_multiboot2_memory_map(__regions, mbi);
    reserve_multiboot2_data(__regions, mbi);

    __buddy.init();
    __global_allocator = &__buddy;
}

}  // namespace mem
