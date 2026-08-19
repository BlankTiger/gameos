#pragma once

#include "kstd/array.hh"
#include "kstd/assert.hh"
#include "kstd/basic.hh"
#include "kstd/byte_reader.hh"
#include "kstd/hash_table.hh"

namespace dwarf {

enum struct Tag : u64 {
    SUBPROGRAM = 0x2e,
};
@enum_to_string(Tag);

enum struct Attribute_Type : u64 {
    NAME = 0x03,
};
@enum_to_string(Attribute_Type);

enum struct Form : u64 {
    STRING         = 0x08,
    IMPLICIT_CONST = 0x21,
};
@enum_to_string(Form);

struct Attribute_Spec {
    Attribute_Type attribute_type;
    Form           form;
    s64            implicit_const;
};

struct Abbreviation {
    Tag                   tag;
    bool                  has_children;
    Array<Attribute_Spec> attribute_specs;
};

struct Source_Row {
    psize  address;
    string file_name;
    u32    line;
};

struct Subprogram_Info {
    string name;
    psize  low_pc;
    psize  high_pc;
};

auto parse_attribute_specs(Byte_Reader& reader) -> Array<Attribute_Spec> {
    Array<Attribute_Spec> attribute_specs;
    for (;;) {
        auto [attribute_type_value, attribute_type_ok] = reader.read_uleb128();
        kstd_assert(attribute_type_ok);

        auto [form_value, form_ok] = reader.read_uleb128();
        kstd_assert(form_ok);

        if (attribute_type_value == 0 && form_value == 0) break;

        auto attribute_type = static_cast<Attribute_Type>(attribute_type_value);
        auto form           = static_cast<Form>(form_value);
        s64  implicit_const = 0;
        if (form == Form::IMPLICIT_CONST) {
            auto [implicit_const_value, implicit_const_ok] = reader.read_sleb128();
            kstd_assert(implicit_const_ok);
            implicit_const = implicit_const_value;
        }

        attribute_specs.push_back({ attribute_type, form, implicit_const });
    }
    return attribute_specs;
}

using Abbreviations = Hash_Table<u64, Abbreviation>;

//
// `reader` must be initialized with memory of the .debug_abbrev section.
// It's stopped at the (0, 0, 0) terminator that ends the abbreviations table.
//
auto parse_abbreviations(Byte_Reader& reader) -> Abbreviations {
    Abbreviations abbreviations;

    static constexpr u64 STOP_CODE = 0;

    for (;;) {
        auto [abbreviation_code, code_ok] = reader.read_uleb128();
        kstd_assert(code_ok);

        if (abbreviation_code == STOP_CODE) break;

        auto [tag_value, tag_ok] = reader.read_uleb128();
        kstd_assert(tag_ok);
        auto tag = static_cast<Tag>(tag_value);

        auto [has_children_value, has_children_ok] = reader.read_u8();
        kstd_assert(has_children_ok);
        auto has_children = static_cast<bool>(has_children_value);

        auto attribute_specs = parse_attribute_specs(reader);

        Abbreviation abbreviation(tag, has_children, attribute_specs);
        abbreviations.set(abbreviation_code, abbreviation);
    }

    return abbreviations;
}

enum struct Unit_Type : u8 {
    COMPILE = 0x01,
};

struct Compilation_Unit_Header {
    u64       length;
    u16       version;
    Unit_Type type;
    u8        address_size;
    u64       abbreviation_offset;
};

constexpr auto DWARF_VERSION = 5;

auto parse_compilation_unit_header(Byte_Reader& reader) -> Compilation_Unit_Header {
    // @NOTE: This is only for 32-bit DWARF, for 64-bit the handling is different.
    auto [length, length_ok] = reader.read_u32();
    kstd_assert(length_ok);

    static constexpr auto LENGTH_FOR_64_BIT_DWARF = 0xffffffff;
    kstd_assert(length != LENGTH_FOR_64_BIT_DWARF, "dwarf: 64-bit DWARF format unsupported");

    auto [version, version_ok] = reader.read_u16();
    kstd_assert(version_ok);

    //
    // @TODO(blanktiger): Consider if this shouldn't just be where we bail out
    // instead of asserting, we could just notify instead of halting the entire
    // kernel. Then again, this means that it was not compiled correctly or
    // something.
    //
    kstd_assert(version == DWARF_VERSION, "We only support DWARF5.");

    auto [type_value, type_value_ok] = reader.read_u8();
    kstd_assert(type_value_ok);

    auto [address_size, address_size_ok] = reader.read_u8();
    kstd_assert(address_size_ok);

    // @NOTE: This is only for 32-bit DWARF.
    auto [abbreviation_offset, abbreviation_offset_ok] = reader.read_u32();
    kstd_assert(abbreviation_offset_ok);

    return {
        length,
        version,
        static_cast<Unit_Type>(type_value),
        address_size,
        abbreviation_offset
    };
}

}
