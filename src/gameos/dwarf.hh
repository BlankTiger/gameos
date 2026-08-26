#pragma once

#include "kstd/basic.hh"
#include "kstd/context.hh"
#include "kstd/dwarf.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/string.hh"
#include "kstd/allocator.hh"

#include "gameos/serial_format.hh"

namespace dwarf {

extern "C" const u8 __debug_info_start[];
extern "C" const u8 __debug_info_end[];

extern "C" const u8 __debug_abbrev_start[];
extern "C" const u8 __debug_abbrev_end[];

extern "C" const u8 __debug_str_start[];
extern "C" const u8 __debug_str_end[];

extern "C" const u8 __debug_line_start[];
extern "C" const u8 __debug_line_end[];

extern "C" const u8 __rodata_start[];
extern "C" const u8 __rodata_end[];

extern "C" const u8 __debug_str_offsets_start[];
extern "C" const u8 __debug_str_offsets_end[];

extern "C" const u8 __debug_addr_start[];
extern "C" const u8 __debug_addr_end[];

namespace hidden {
    //
    // This is the arena where everything built will be copied into to make it
    // compact. The building arena will be completely deallocated after that. To
    // start with it gets initialized with size 0 and then it's gonna be resized once we
    // know how much stuff it needs to hold.
    //
    // @TODO(blanktiger): Actually do it.
    // mem::Arena_Allocator_State debug_info_allocator(0);

    mem::Arena_Allocator_State debug_info_building_allocator(10 * 1024 * 1024);
    Array<Subprogram_Info> infos;
    Array<Source_Row>      rows;
}

auto build_debug_info() -> void {
    serial::println("dwarf: Building debug info");

    mem::Temporary_Allocator_State dwarf_temp(5 * 1024 * 1024);
    PUSH_TEMPORARY_ALLOCATOR(&dwarf_temp);

    using namespace hidden;
    PUSH_ALLOCATOR(debug_info_building_allocator.get_allocator());
    defer(serial::println("DWARF parsing uses: % MB", cast(f32)debug_info_building_allocator.bytes_used() / 1024 / 1024));

    auto debug_info_size        = ptr_addr(__debug_info_end)        - ptr_addr(__debug_info_start);
    auto debug_abbrev_size      = ptr_addr(__debug_abbrev_end)      - ptr_addr(__debug_abbrev_start);
    auto debug_line_size        = ptr_addr(__debug_line_end)        - ptr_addr(__debug_line_start);
    auto debug_str_size         = ptr_addr(__debug_str_end)         - ptr_addr(__debug_str_start);
    // Linker merges .debug_line_str strings into .rodata. Use full loaded range.
    auto rodata_size            = ptr_addr(__rodata_end)            - ptr_addr(__rodata_start);
    auto debug_str_offsets_size = ptr_addr(__debug_str_offsets_end) - ptr_addr(__debug_str_offsets_start);
    auto debug_addr_size        = ptr_addr(__debug_addr_end)        - ptr_addr(__debug_addr_start);

    Sections sections{
        .debug_info_bytes        = { debug_info_size,        __debug_info_start        },
        .debug_abbrev_bytes      = { debug_abbrev_size,      __debug_abbrev_start      },
        .debug_line_bytes        = { debug_line_size,        __debug_line_start        },
        .debug_str_bytes         = { debug_str_size,         __debug_str_start         },
        .debug_line_str_bytes    = { rodata_size,            __rodata_start            },
        .debug_str_offsets_bytes = { debug_str_offsets_size, __debug_str_offsets_start },
        .debug_addr_bytes        = { debug_addr_size,        __debug_addr_start        },
        // These two fields will get set while parsing.
        .debug_str_offsets_base  = 0,
        .debug_addr_base         = 0,
    };

    Byte_Reader debug_info(sections.debug_info_bytes);
    auto [subprogram_infos, debug_line_offset, has_debug_line_offset] = parse_subprograms(debug_info, sections);

    // @TODO(blanktiger): Make this optional.
    kstd_assert(has_debug_line_offset);

    // Currently we get around 32k rows, so preallocate a little more than that.
    auto source_rows = parse_line_table(sections, debug_line_offset, 34'000);

    infos = std::move(subprogram_infos);
    rows  = std::move(source_rows);
}

force_inline auto function_name_for_address(psize address) -> string {
    return function_name_for_address(hidden::infos, address);
}

force_inline auto source_for_address(psize address) -> Source_Lookup_Result {
    return source_for_address(hidden::rows, address);
}

}
