#pragma once


#include "byte_reader.hh"

namespace dwarf {

enum struct Tag : u16 {
    SUBPROGRAM = 0x2e,
};

enum struct Attribute_Type : u16 {
    NAME = 0x03,
};

}
