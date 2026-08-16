#pragma once

#include "kstd/dwarf.hh"


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

}
