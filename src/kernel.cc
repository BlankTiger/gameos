#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <cstdint>

#include "kstd.hh"
#include "terminal.hh"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

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

struct Memory_Region {
    uint64_t base;
    uint64_t length;
};

static constexpr uint32_t multiboot_info_has_mmap = 1u << 6;
static constexpr uint32_t multiboot_mmap_usable = 1u;
static constexpr size_t max_memory_regions = 32;

static Memory_Region _usable_regions[max_memory_regions];

static kstd::Array<Memory_Region> usable_regions = {0, _usable_regions};
static uint64_t usable_memory_bytes;

void parse_multiboot_memory_map(const Multiboot_Info* mbi) {
    usable_memory_bytes = 0;

    if ((mbi->flags & multiboot_info_has_mmap) == 0) return;

    auto* entry = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr);
    const auto* end = reinterpret_cast<const Multiboot_MMAP_Entry*>((uintptr_t)mbi->mmap_addr + mbi->mmap_length);

    while (reinterpret_cast<uintptr_t>(entry) < reinterpret_cast<uintptr_t>(end)) {
        const uint64_t entry_length = entry->len;
        if (entry->type == multiboot_mmap_usable && usable_regions.size < max_memory_regions) {
            usable_regions[usable_regions.size++] = {entry->addr, entry_length};
            usable_memory_bytes += entry_length;
        }

        entry = reinterpret_cast<const Multiboot_MMAP_Entry*>(
            reinterpret_cast<uintptr_t>(entry) + entry->size + sizeof(entry->size));
    }
}

extern "C" void kernel_main(uint32_t magic, const Multiboot_Info* mbi) {
    terminal_initialize();

    if (magic != 0x2BADB002) {
        terminal_writestring("Bad multiboot magic\n");
        return;
    }

    parse_multiboot_memory_map(mbi);
    // auto entries = parse_multiboot_memory_map(mbi);

    for (uint32_t i = 0; i < usable_regions.size; i++) {
        terminal_write_hex(usable_regions[i].base);
        terminal_writestring(" ");
        terminal_write_hex(usable_regions[i].length);
        terminal_writestring("\n");
    }

    terminal_writestring("Usable RAM bytes: 0x");
    terminal_write_hex(usable_memory_bytes);
    terminal_writestring("\nRegions: 0x");
    terminal_write_hex(usable_regions.size);
    terminal_writestring("\n");
}
