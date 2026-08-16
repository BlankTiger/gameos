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
}

}
