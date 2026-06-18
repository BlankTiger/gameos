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

    for (size_t i = 0; i < regions.size;) {
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

// struct Frame_Allocator {
//     Frame_Allocator() {}
//
//     void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
//         if (size == 0) size = 1;
//         if (alignment < alignof(void*)) alignment = alignof(void*);
//
//         for (size_t i = 0; i < __regions.size; i++) {
//             const u64 region_start = __regions[i].base;
//             const u64 region_end = region_start + __regions[i].length;
//             const u64 aligned_start = align_up(region_start, alignment);
//             if (region_end <= aligned_start) continue;
//             const u64 available = region_end - aligned_start;
//
//             if (available < size) continue;
//
//             __regions[i].base = aligned_start + size;
//             __regions[i].length = region_end - __regions[i].base;
//             return reinterpret_cast<void*>((uintptr_t)aligned_start);
//         }
//
//         return nullptr;
//     }
//
//     void free(void* ptr, size_t alignment = alignof(std::max_align_t)) {
//         (void)ptr;
//         (void)alignment;
//     }
// };

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

// static auto align_up(std::byte* p, size_t alignment) -> std::byte* {
//     auto addr = reinterpret_cast<std::uintptr_t>(p);
//     auto aligned = (addr + alignment - 1) & ~(alignment - 1);
//     return reinterpret_cast<std::byte*>(aligned);
// }

struct Allocator {
    virtual auto alloc(size_t size, size_t alignment = alignof(std::max_align_t)) -> void* = 0;
    virtual auto free(void* pointer, size_t size, size_t alignment = alignof(std::max_align_t)) -> void = 0;
    virtual ~Allocator() = default;
};

struct Buddy_Block {
    u64 size_;
    bool free_;

    auto next() -> Buddy_Block* {
        return reinterpret_cast<Buddy_Block*>(reinterpret_cast<u8*>(this) + size_);
    }

    auto split(size_t size) -> Buddy_Block* {
        auto* current_block = this;
        debug_assert(current_block != nullptr, nullptr, std::source_location::current());

        if (size != 0) {
            while (size < current_block->size_) {
                auto new_size = size_ >> 1;
                current_block->size_ = new_size;
                current_block = current_block->next();
                *current_block = Buddy_Block(new_size, true);
            }

            if (size <= current_block->size_) {
                return current_block;
            }
        }

        return nullptr;
    }

    static auto find_best(Buddy_Block* head, Buddy_Block* tail, size_t size) -> Buddy_Block* {
        Buddy_Block* best_block = nullptr;
        auto* block = head;
        auto* buddy = block->next();

        // The entire memory section between head and tail is free, just call
        // 'split' to get the allocation.
        if (buddy == tail && block->free_) {
            return block->split(size);
        }

        // Find the block which is the 'best_block' to requested allocation sized
        // @TODO: Coalesce on neighboring free buddies.
        while (block < tail && buddy < tail) {
            // If both buddies are free, coalesce them together
            // NOTE: this is an optimization to reduce fragmentation
            //       this could be completely ignored
            if (block->free_ && buddy->free_ && block->size_ == buddy->size_) {
                block->size_ <<= 1;
                if (size <= block->size_ && (best_block == nullptr || block->size_ <= best_block->size_)) {
                    best_block = block;
                }

                block = buddy->next();
                if (block < tail) {
                    // Delay the buddy block for the next iteration
                    buddy = block->next();
                }
                continue;
            }

            if (block->free_ && size <= block->size_ && (best_block == nullptr || block->size_ <= best_block->size_)) {
                best_block = block;
            }

            if (buddy->free_ && size <= buddy->size_ && (best_block == nullptr || buddy->size_ < best_block->size_)) {
                // If each buddy are the same size, then it makes more sense
                // to pick the buddy as it "bounces around" less
                best_block = buddy;
            }

            if (block->size_ <= buddy->size_) {
                block = buddy->next();
                if (block < tail) {
                    // Delay the buddy block for the next iteration
                    buddy = block->next();
                }
            } else {
                // Buddy was split into smaller blocks
                block = buddy;
                buddy = buddy->next();
            }
        }

        if (best_block != nullptr) {
            // This will handle the case if the 'best_block' is also the perfect fit
            return best_block->split(size);
        }

        // Maybe out of memory
        return nullptr;
    }

    auto coalescence(Buddy_Block* head, Buddy_Block* tail) -> void {
        for (;;) {
            // Keep looping until there are no more buddies to coalesce

            auto* block = head;
            auto* buddy = block->next();

            auto no_coalescence = true;
            while (block < tail && buddy < tail) {
                if (block->free_ && buddy->free_ && block->size_ == buddy->size_) {
                    // Coalesce buddies into one
                    block->size_ <<= 1;
                    block = block->next();
                    if (block < tail) {
                        buddy = block->next();
                        no_coalescence = false;
                    }
                } else if (block->size_ < buddy->size_) {
                    // The buddy block is split into smaller blocks
                    block = buddy;
                    buddy = buddy->next();
                } else {
                    block = buddy->next();
                    if (block < tail) {
                        // Leave the buddy block for the next iteration
                        buddy = block->next();
                    }
                }
            }

            if (no_coalescence) {
                return;
            }
        }
    }

    static auto size_required(size_t size) -> size_t {
        // @TODO: alignment??
        size += sizeof(Buddy_Block);
        return size;
    }

};

struct Buddy_Source_Block {
    Buddy_Block* head;
    Buddy_Block* tail;
    // @TODO: is this necessary?
    // size_t alignment;
};

struct Buddy_Allocator : Allocator {
    Buddy_Source_Block __source_blocks[MAX_MEMORY_REGIONS];
    Array<Buddy_Source_Block> source_blocks_;
    bool initted_;

    Buddy_Allocator() {}

    auto init() -> void {
        source_blocks_ = {__regions.size, __source_blocks};

        for (u64 i = 0; i < __regions.size; ++i) {
            auto& region = __regions[i];
            auto* block = reinterpret_cast<Buddy_Block*>(region.base);
            *block = Buddy_Block(floor_pow2(region.size), true);
            source_blocks_[i] = Buddy_Source_Block{block, block->next()};
        }

        initted_ = true;
    }

    auto alloc(size_t size, size_t alignment = alignof(std::max_align_t)) -> void* override {
        debug_assert(initted_, nullptr, std::source_location::current());

        if (size != 0) {
            auto actual_size = Buddy_Block::size_required(size);
            actual_size = align_up(actual_size, alignment);

            for (auto& source_block : source_blocks_) {
                auto* found = Buddy_Block::find_best(source_block.head, source_block.tail, actual_size);
                if (found == nullptr) continue;

                found->free_ = false;
                // @TODO: alignment??
                auto* result = static_cast<void*>(reinterpret_cast<std::byte*>(found) + sizeof(Buddy_Block));
                term::terminal_writestring("Alloced: ");
                term::terminal_write_hex(reinterpret_cast<u64>(result));
                term::terminal_writestring("\n");
                return result;
            }
        }
        return nullptr;
    }

    auto free(void* pointer, size_t size, size_t alignment = alignof(std::max_align_t)) -> void override {
        (void)size;
        (void)alignment;

        debug_assert(initted_, nullptr, std::source_location::current());
        assert(pointer != nullptr, nullptr, std::source_location::current());

        Buddy_Source_Block source;
        auto found_source = false;
        for (auto& source_block : source_blocks_) {
            if (static_cast<void*>(source_block.head) <= pointer && static_cast<void*>(source_block.tail) > pointer) {
                found_source = true;
                source = source_block;
                break;
            }
        }
        assert(found_source, nullptr, std::source_location::current());

        auto* block = reinterpret_cast<Buddy_Block*>(static_cast<std::byte*>(pointer) - sizeof(Buddy_Block));
        block->free_ = true;
        term::terminal_writestring("Freed: ");
        term::terminal_write_hex(reinterpret_cast<u64>(pointer));
        term::terminal_writestring("\n");
    }
};

}  // namespace mem
