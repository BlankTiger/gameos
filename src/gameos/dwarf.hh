#pragma once

#include "kstd/basic.hh"
#include "kstd/dwarf.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/string.hh"

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

extern "C" const u8 __debug_line_str_start[];
extern "C" const u8 __debug_line_str_end[];


auto build_debug_info() -> void {
    serial::println("dwarf: Building debug info");
    serial::println("%, %", __debug_info_start, __debug_info_end);
    serial::println("%, %", __debug_abbrev_start, __debug_abbrev_end);
    serial::println("%, %", __debug_str_start, __debug_str_end);
    serial::println("%, %", __debug_line_start, __debug_line_end);
    serial::println("%, %", __debug_line_str_start, __debug_line_str_end);

    auto debug_info_size     = ptr_addr(__debug_info_end)     - ptr_addr(__debug_info_start);
    auto debug_abbrev_size   = ptr_addr(__debug_abbrev_end)   - ptr_addr(__debug_abbrev_start);
    auto debug_line_size     = ptr_addr(__debug_line_end)     - ptr_addr(__debug_line_start);
    auto debug_str_size      = ptr_addr(__debug_str_end)      - ptr_addr(__debug_str_start);
    auto debug_line_str_size = ptr_addr(__debug_line_str_end) - ptr_addr(__debug_line_str_start);

    Array_View<const u8> debug_info_bytes     (debug_info_size,     __debug_info_start);
    Array_View<const u8> debug_abbrev_bytes   (debug_abbrev_size,   __debug_abbrev_start);
    Array_View<const u8> debug_line_bytes     (debug_line_size,     __debug_line_start);
    Array_View<const u8> debug_str_bytes      (debug_str_size,      __debug_str_start);
    Array_View<const u8> debug_line_str_bytes (debug_line_str_size, __debug_line_str_start);

    Byte_Reader debug_info(debug_info_bytes);
    Byte_Reader debug_abbrev(debug_abbrev_bytes);

    auto abbreviations = parse_abbreviations(debug_abbrev);
    auto header = parse_compilation_unit_header(debug_info);
    auto [subprogram_infos, line_offset, have_line_offset] = parse_compilation_unit_debug_information_entries(
        debug_info,
        abbreviations,
        header.address_size,
        debug_str_bytes,
        debug_line_str_bytes
    );

    serial::println("%", subprogram_infos);
}

}
