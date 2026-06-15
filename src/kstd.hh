#pragma once

namespace kstd {
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

template <typename T>
struct Array {
    uint64_t size;
    T* data;

    T& operator[](int index) {
        return data[index];
    }
};

struct Memory_Region {
    uint64_t base;
    uint64_t length;
};

struct Multiboot_Info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed));

struct Multiboot_MMAP_Entry {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

struct Multiboot_Module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} __attribute__((packed));

static constexpr uint32_t multiboot_info_has_mmap = 1u << 6;
static constexpr uint32_t multiboot_mmap_usable = 1u;
static constexpr uint64_t page_size = 4096;
static constexpr size_t max_memory_regions = 32;

extern "C" uint8_t __kernel_start;
extern "C" uint8_t __kernel_end;

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static uint64_t align_down(uint64_t value, uint64_t alignment) {
    return value & ~(alignment - 1);
}

static void add_usable_region(Array<Memory_Region>& regions, uint64_t base, uint64_t length) {
    if (length == 0 || regions.size >= max_memory_regions) return;
    regions[regions.size++] = {base, length};
}

static void reserve_range(Array<Memory_Region>& regions, uint64_t start, uint64_t end) {
    if (start >= end) return;

    start = align_down(start, page_size);
    end = align_up(end, page_size);

    for (size_t i = 0; i < regions.size;) {
        const uint64_t region_start = regions[i].base;
        const uint64_t region_end = region_start + regions[i].length;

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
            regions[i].length = region_end - end;
            i++;
            continue;
        }

        if (end >= region_end) {
            regions[i].length = start - region_start;
            i++;
            continue;
        }

        if (regions.size < max_memory_regions) {
            regions[regions.size++] = {end, region_end - end};
            regions[i].length = start - region_start;
            i++;
        } else {
            regions[i].length = start - region_start;
            i++;
        }
    }
}

static void parse_multiboot_memory_map(Array<Memory_Region>& regions, const Multiboot_Info* mbi) {
    if ((mbi->flags & multiboot_info_has_mmap) == 0) return;

    auto* entry = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr);
    const auto* end = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr + mbi->mmap_length);

    while (reinterpret_cast<uintptr_t>(entry) < reinterpret_cast<uintptr_t>(end)) {
        const uint64_t entry_length = entry->len;
        if (entry->type == multiboot_mmap_usable) add_usable_region(regions, entry->addr, entry_length);

        entry = reinterpret_cast<const Multiboot_MMAP_Entry*>(
            reinterpret_cast<uintptr_t>(entry) + entry->size + sizeof(entry->size));
    }
}

static void reserve_multiboot_data(Array<Memory_Region>& regions, const Multiboot_Info* mbi) {
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

    for (uint32_t i = 0; i < mbi->mods_count; i++) {
        reserve_range(regions, modules[i].mod_start, modules[i].mod_end);
        if (modules[i].string != 0) {
            reserve_range(
                regions, modules[i].string, modules[i].string + strlen((const char*)(uintptr_t)modules[i].string) + 1);
        }
    }
}

static kstd::Memory_Region _usable_regions[max_memory_regions];

struct Frame_Allocator {
    Array<Memory_Region> regions_;

    Frame_Allocator(const Multiboot_Info* mbi)
        : regions_({0, _usable_regions}) {
        parse_multiboot_memory_map(regions_, mbi);
        reserve_multiboot_data(regions_, mbi);
    }
};

}  // namespace kstd
