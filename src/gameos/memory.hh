#pragma once

#include <cstddef>
#include <new>

#include "kstd/array.hh"
#include "kstd/assert.hh"
#include "kstd/allocator.hh"

#include "gameos/multiboot2.hh"

namespace mem {

static constexpr int MULTIBOOT_MMAP_USABLE = 1u;
static constexpr int MAX_MEMORY_REGIONS    = 128;

struct Memory_Region {
    u64 base;
    u64 size;
};

using Memory_Regions = Bounded_Array<Memory_Region, MAX_MEMORY_REGIONS>;

extern "C" u8 __kernel_start;
extern "C" u8 __kernel_end;

auto add_usable_region(Memory_Regions& regions, u64 base, u64 length) -> void {
    regions.push_back({base, length});
}

auto reserve_range(Memory_Regions& regions, u64 start, u64 end) -> void {
    if (start >= end) return;

    start = align_down(start, static_cast<u64>(mem::PAGE_SIZE));
    end   = align_up(end,     static_cast<u64>(mem::PAGE_SIZE));

    for (usize i = 0; i < regions.size;) {
        const u64 region_start = regions[i].base;
        const u64 region_end   = region_start + regions[i].size;

        if (end <= region_start || start >= region_end) {
            i++;
            continue;
        }

        if (start <= region_start && end >= region_end) {
            const usize last = regions.size - 1;
            if (i != last) regions[i] = regions[last];
            regions.size--;
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
            kstd_assert(false, "Memory_Regions overflow, increase MAX_MEMORY_REGIONS");
            regions[i].size = start - region_start;
            i++;
        }
    }
}

auto parse_multiboot2_memory_map(Memory_Regions& regions, const boot::Multiboot2_Info* mbi) -> void {
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

auto reserve_multiboot2_data(Memory_Regions& regions, const boot::Multiboot2_Info* mbi) -> void {
    //
    // Reserve all conventional low memory (0 to 1 MiB).
    //
    // GRUB leaves live structures here. We never asked for them. The multiboot
    // memory map does not track them as reserved. This includes its own
    // temporary GDT. If the buddy allocator gives a page in this range, the
    // first heap write corrupts that GDT. CS stays valid until the next
    // interrupt tries to reload it. Then you get a GP, then DF, then triple
    // fault. This looks like a random bootloop with no useful backtrace. By
    // the time it appears, the write that corrupts it is long gone.
    //
    reserve_range(regions, 0, 0x100000);
    reserve_range(regions, ptr_addr(mbi), ptr_addr(mbi) + mbi->total_size);
    reserve_range(regions, ptr_addr(&__kernel_start), ptr_addr(&__kernel_end));

    auto* tag = mbi->first_tag();
    const auto* end = mbi->end_tag();

    while (ptr_addr(tag) < ptr_addr(end)) {
        const auto tag_type = static_cast<boot::Multiboot2_Tag_Type>(tag->type);
        if (tag_type == boot::Multiboot2_Tag_Type::CMDLINE || tag_type == boot::Multiboot2_Tag_Type::BOOT_LOADER_NAME) {
            const auto* text = tag->payload_as<char>();
            reserve_range(regions, ptr_addr(text), ptr_addr(text) + kstd_strlen(text) + 1);
        } else if (tag_type == boot::Multiboot2_Tag_Type::MODULE) {
            const auto* module = tag->as<boot::Multiboot2_Module_Tag>();
            reserve_range(regions, module->mod_start, module->mod_end);
            if (module->string != 0) {
                reserve_range(regions, module->string, module->string + kstd_strlen(module->string_ptr()) + 1);
            }
        }

        tag = tag->next();
    }
}

namespace hidden {
    inline Memory_Regions regions{};
}

auto initialize(const boot::Multiboot2_Info* mbi) -> void {
    using namespace hidden;

    parse_multiboot2_memory_map(regions, mbi);
    reserve_multiboot2_data(regions, mbi);

    buddy.clear();
    for (const auto& region : regions) {
        buddy.add_region(region.base, region.size);
    }
    set_allocator(buddy.get_allocator());

    constexpr usize TEMPORARY_ALLOCATOR_SIZE = 1 * 1024 * 1024;
    auto temporary_allocation = mem::alloc(TEMPORARY_ALLOCATOR_SIZE);
    void* temporary_memory = temporary_allocation.memory;
    kstd_assert(temporary_memory != nullptr, "Failed to allocate temporary allocator backing memory.");
    temporary_allocator = Temporary_Allocator_State{
        temporary_memory,
        TEMPORARY_ALLOCATOR_SIZE
    };
    set_temporary_allocator(&temporary_allocator);
}

}  // namespace mem

auto operator new(usize size) -> void* {
    if (void* ptr = mem::alloc(size).memory) return ptr;
    halt::forever("new failed");
}

auto operator new[](usize size) -> void* {
    if (void* ptr = mem::alloc(size).memory) return ptr;
    halt::forever("new[] failed");
}

auto operator new(usize size, std::align_val_t alignment) -> void* {
    if (void* ptr = mem::alloc(size, static_cast<usize>(alignment)).memory) return ptr;
    halt::forever("aligned new failed");
}

auto operator new[](usize size, std::align_val_t alignment) -> void* {
    if (void* ptr = mem::alloc(size, static_cast<usize>(alignment)).memory) return ptr;
    halt::forever("aligned new[] failed");
}

auto operator delete(void* ptr) noexcept -> void {
    (void)mem::free(ptr, 0);
}

auto operator delete[](void* ptr) noexcept -> void {
    (void)mem::free(ptr, 0);
}

auto operator delete(void* ptr, usize size) noexcept -> void {
    (void)mem::free(ptr, size);
}

auto operator delete[](void* ptr, usize size) noexcept -> void {
    (void)mem::free(ptr, size);
}

auto operator delete(void* ptr, std::align_val_t) noexcept -> void {
    (void)mem::free(ptr, 0);
}

auto operator delete[](void* ptr, std::align_val_t) noexcept -> void {
    (void)mem::free(ptr, 0);
}

auto operator delete(void* ptr, usize size, std::align_val_t) noexcept -> void {
    (void)mem::free(ptr, size);
}

auto operator delete[](void* ptr, usize size, std::align_val_t) noexcept -> void {
    (void)mem::free(ptr, size);
}
