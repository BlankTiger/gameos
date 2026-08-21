#pragma once

#include "kstd/basic.hh"
#include "kstd/context.hh"
#include "kstd/dwarf.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/string.hh"

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

extern "C" const u8 __debug_line_str_start[];
extern "C" const u8 __debug_line_str_end[];

extern "C" const u8 __debug_str_offsets_start[];
extern "C" const u8 __debug_str_offsets_end[];

extern "C" const u8 __debug_addr_start[];
extern "C" const u8 __debug_addr_end[];


auto build_debug_info() -> void {
    serial::println("dwarf: Building debug info");

    auto debug_info_size        = ptr_addr(__debug_info_end)        - ptr_addr(__debug_info_start);
    auto debug_abbrev_size      = ptr_addr(__debug_abbrev_end)      - ptr_addr(__debug_abbrev_start);
    auto debug_line_size        = ptr_addr(__debug_line_end)        - ptr_addr(__debug_line_start);
    auto debug_str_size         = ptr_addr(__debug_str_end)         - ptr_addr(__debug_str_start);
    auto debug_line_str_size    = ptr_addr(__debug_line_str_end)    - ptr_addr(__debug_line_str_start);
    auto debug_str_offsets_size = ptr_addr(__debug_str_offsets_end) - ptr_addr(__debug_str_offsets_start);
    auto debug_addr_size        = ptr_addr(__debug_addr_end)        - ptr_addr(__debug_addr_start);

    Sections sections{
        .debug_info_bytes        = { debug_info_size,        __debug_info_start        },
        .debug_abbrev_bytes      = { debug_abbrev_size,      __debug_abbrev_start      },
        .debug_line_bytes        = { debug_line_size,        __debug_line_start        },
        .debug_str_bytes         = { debug_str_size,         __debug_str_start         },
        .debug_line_str_bytes    = { debug_line_str_size,    __debug_line_str_start    },
        .debug_str_offsets_bytes = { debug_str_offsets_size, __debug_str_offsets_start },
        .debug_addr_bytes        = { debug_addr_size,        __debug_addr_start        }
    };

    Byte_Reader debug_info(sections.debug_info_bytes);
    Byte_Reader debug_abbrev(sections.debug_abbrev_bytes);

    auto abbreviations = parse_abbreviations(debug_abbrev);
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
    auto source_rows = parse_line_table(sections, debug_line_offset);

    power::off();
}

}
