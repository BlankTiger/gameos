#pragma once

#include "kstd/basic.hh"
#include "kstd/context.hh"
#include "kstd/dwarf.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/string.hh"
#include "kstd/allocator.hh"

#include "gameos/serial_format.hh"
#include "gameos/power.hh"

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
    mem::Arena_Allocator debug_info_allocator(0);

    
}

auto build_debug_info() -> void {
    serial::println("dwarf: Building debug info");

    defer(power::off());

    mem::Arena_Allocator debug_info_building_allocator(5 * 1024 * 1024);

    using namespace hidden;
    PUSH_ALLOCATOR(&debug_info_building_allocator);
    defer(serial::println("DWARF parsing uses: % MB", static_cast<f32>(debug_info_building_allocator.bytes_used()) / 1024 / 1024));

    auto debug_info_size        = ptr_addr(__debug_info_end)        - ptr_addr(__debug_info_start);
    auto debug_abbrev_size      = ptr_addr(__debug_abbrev_end)      - ptr_addr(__debug_abbrev_start);
    auto debug_line_size        = ptr_addr(__debug_line_end)        - ptr_addr(__debug_line_start);
    auto debug_str_size         = ptr_addr(__debug_str_end)         - ptr_addr(__debug_str_start);
    // Linker merges .debug_line_str strings into .rodata. Use full loaded range.
    auto rodata_size            = ptr_addr(__rodata_end)             - ptr_addr(__rodata_start);
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
    Byte_Reader debug_abbrev(sections.debug_abbrev_bytes);

    // Currently we get around 260 abbreviations, so preallocate a little bit more than that.
    auto abbreviations = parse_abbreviations(debug_abbrev, 400);
    auto compilation_unit_start = debug_info.current_offset;
    auto header                 = parse_compilation_unit_header(debug_info);
    auto compilation_unit_end   = compilation_unit_start + sizeof(u32) + header.length;
    auto [subprogram_infos, debug_line_offset, has_debug_line_offset] = parse_compilation_unit_debug_information_entries(
        debug_info,
        compilation_unit_end,
        abbreviations,
        header.address_size,
        sections
    );

    PUSH_CONTEXT();
    context.formatting_config.newline_after_each_array_element = true;
    serial::println("%", subprogram_infos);

    // @TODO(blanktiger): Make this optional.
    kstd_assert(has_debug_line_offset);

    // Currently we get around 32k rows, so preallocate a little more than that.
    auto source_rows = parse_line_table(sections, debug_line_offset, 34'000);

    auto free_string_address = reinterpret_cast<psize>(&free_string);
    auto name                = function_name_for_address(subprogram_infos, free_string_address);
    auto row                 = source_for_address(source_rows, free_string_address);
    serial::println("%, %", name, row);
}

}
