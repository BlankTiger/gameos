#include "basic.hh"
#include "assert.hh"
#include "cstring.hh"
#include "memory.hh"

namespace mem {

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
    //
    // Reserve all of conventional low memory (0 - 1 MiB).
    //
    // GRUB leaves live structures down here that we never asked for and
    // don't track in the multiboot memory map as "reserved" - notably its
    // own temporary GDT. If the buddy allocator hands out a page in this
    // range, the very first heap write into it corrupts that GDT; CS stays
    // "valid" until the next interrupt tries to reload it, at which point
    // you get a #GP -> #DF -> triple fault (looks like a random bootloop
    // with no useful backtrace, since by the time it manifests the actual
    // corrupting write is long gone).
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

static Memory_Regions  regions{};
static Buddy_Allocator buddy;

Allocator* __global_allocator;

auto initialize(const boot::Multiboot2_Info* mbi) -> void {
    parse_multiboot2_memory_map(regions, mbi);
    reserve_multiboot2_data(regions, mbi);

    buddy.init(regions);
    __global_allocator = &buddy;
}

}  // namespace mem
