#pragma once

#include "enum_name.hh"
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
    ADDR           = 0x01,
    BLOCK2         = 0x03,
    BLOCK4         = 0x04,
    DATA2          = 0x05,
    DATA4          = 0x06,
    DATA8          = 0x07,
    STRING         = 0x08,
    BLOCK          = 0x09,
    BLOCK1         = 0x0a,
    DATA1          = 0x0b,
    FLAG           = 0x0c,
    SDATA          = 0x0d,
    STRP           = 0x0e,
    UDATA          = 0x0f,
    REF_ADDR       = 0x10,
    REF1           = 0x11,
    REF2           = 0x12,
    REF4           = 0x13,
    REF8           = 0x14,
    REF_UDATA      = 0x15,
    INDIRECT       = 0x16,
    SEC_OFFSET     = 0x17,
    EXPRLOC        = 0x18,
    FLAG_PRESENT   = 0x19,
    STRX           = 0x1a,
    ADDRX          = 0x1b,
    REF_SUP4       = 0x1c,
    STRP_SUP       = 0x1d,
    DATA16         = 0x1e,
    LINE_STRP      = 0x1f,
    REF_SIG8       = 0x20,
    IMPLICIT_CONST = 0x21,
    LOCLISTX       = 0x22,
    RNGLISTX       = 0x23,
    REF_SUP8       = 0x24,
    STRX1          = 0x25,
    STRX2          = 0x26,
    STRX3          = 0x27,
    STRX4          = 0x28,
    ADDRX1         = 0x29,
    ADDRX2         = 0x2a,
    ADDRX3         = 0x2b,
    ADDRX4         = 0x2c,
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

//
// `reader` must be pointed at bytes containing the compilation unit headers.
//
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

enum struct Attribute_Value_Kind {
    NONE,
    UNSIGNED,
    SIGNED,
    STRING,
    BLOCK,
    FLAG,
};
@enum_to_string(Attribute_Value_Kind);

struct Attribute_Value {
    Attribute_Value_Kind kind;

    // v - value
    union {
        u64 v_unsigned;
        s64 v_signed;
        string v_string;
        Array_View<const u8> v_block;
    };
};

auto read_section_string(Array_View<const u8> section, u32 offset) -> string {
    kstd_assert(static_cast<usize>(offset) < section.size);

    auto* data      = section.data + offset;
    auto  remaining = section.size - offset;
    usize length    = 0;
    while (length < remaining && data[length] != 0) ++length;

    kstd_assert(length < remaining);
    return { reinterpret_cast<const char*>(data), length };
};

//
// Consumes exactly the bytes `form` occupies in the Debug Info Entry (DIE)
// stream and, where the form has a resolvable value (as opposed to an index
// into a table this parser doesn't build, e.g. strx/addrx), returns it.
//
auto read_attribute_value(
    Byte_Reader&         reader,
    Form                 form,
    u8                   address_size,
    s64                  implicit_const,
    Array_View<const u8> debug_str_bytes,
    Array_View<const u8> debug_line_str_bytes
) -> Attribute_Value {
    using enum Attribute_Value_Kind;

    switch (form) {
        case Form::ADDR: {
            if (address_size == 8) {
                auto [address, ok] = reader.read_u64();
                // @TODO(blanktiger): Reconsider making this function return an error instead.
                kstd_assert(ok);
                return { UNSIGNED, address };
            } else {
                auto [address, ok] = reader.read_u32();
                kstd_assert(ok);
                return { UNSIGNED, address };
            }
        } break;

        case Form::DATA1:
        case Form::REF1: {
            auto [value, ok] = reader.read_u8();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA2:
        case Form::REF2: {
            auto [value, ok] = reader.read_u16();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA4:
        case Form::REF_ADDR:
        case Form::REF4:
        case Form::SEC_OFFSET: {
            auto [value, ok] = reader.read_u32();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA8:
        case Form::REF8:
        case Form::REF_SIG8: {
            auto [value, ok] = reader.read_u64();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA16: {
            auto [bytes, ok] = reader.read_bytes(16);
            kstd_assert(ok);
            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK1: {
            auto [length, length_ok] = reader.read_u8();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = reader.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK2: {
            auto [length, length_ok] = reader.read_u16();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = reader.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK4: {
            auto [length, length_ok] = reader.read_u32();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = reader.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK:
        case Form::EXPRLOC: {
            auto [length, length_ok] = reader.read_uleb128();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = reader.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::STRING: {
            auto [value, ok] = reader.read_cstring();
            kstd_assert(ok);
            return { .kind = STRING, .v_string = value };
        } break;

        case Form::STRP:
        case Form::LINE_STRP: {
            auto [offset, ok] = reader.read_u32();
            kstd_assert(ok);

            auto section = form == Form::STRP ? debug_str_bytes : debug_line_str_bytes;
            auto value   = read_section_string(section, offset);
            return { .kind = STRING, .v_string = value };
        } break;

        case Form::SDATA: {
            auto [value, ok] = reader.read_sleb128();
            kstd_assert(ok);
            return { .kind = SIGNED, .v_signed = value };
        } break;

        case Form::UDATA:
        case Form::REF_UDATA: {
            auto [value, ok] = reader.read_uleb128();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::FLAG: {
            auto [value, ok] = reader.read_u8();
            kstd_assert(ok);
            return { .kind = FLAG, .v_unsigned = value };
        } break;

        case Form::FLAG_PRESENT: return { .kind = FLAG, .v_unsigned = 1 };

        case Form::IMPLICIT_CONST: return { .kind = SIGNED, .v_signed = implicit_const };

        case Form::STRX:
        case Form::ADDRX:
        case Form::LOCLISTX:
        case Form::RNGLISTX: {
            auto [index, ok] = reader.read_uleb128();
            kstd_assert(ok);
            (void)index;
            return { NONE, 0 };
        } break;

        case Form::STRX1:
        case Form::ADDRX1: {
            auto [index, ok] = reader.read_u8();
            kstd_assert(ok);
            (void)index;
            return { NONE, 0 };
        } break;

        case Form::STRX2:
        case Form::ADDRX2: {
            auto [index, ok] = reader.read_u16();
            kstd_assert(ok);
            (void)index;
            return { NONE, 0 };
        } break;

        case Form::STRX3:
        case Form::ADDRX3: {
            auto [index, ok] = reader.read_bytes(3);
            kstd_assert(ok);
            (void)index;
            return { NONE, 0 };
        } break;

        case Form::STRX4:
        case Form::ADDRX4: {
            auto [index, ok] = reader.read_u32();
            kstd_assert(ok);
            (void)index;
            return { NONE, 0 };
        } break;

        case Form::REF_SUP4:
        case Form::STRP_SUP: {
            auto [value, ok] = reader.read_u32();
            kstd_assert(ok);
            (void)value;
            return { NONE, 0 };
        } break;

        case Form::REF_SUP8: {
            auto [value, ok] = reader.read_u64();
            kstd_assert(ok);
            (void)value;
            return { NONE, 0 };
        } break;

        case Form::INDIRECT: {
            auto [actual_form, ok] = reader.read_uleb128();
            kstd_assert(ok);
            return read_attribute_value(
                reader,
                static_cast<Form>(actual_form),
                address_size,
                implicit_const,
                debug_str_bytes,
                debug_line_str_bytes
            );
        } break;

        default: unreachable(csprint("Unhandled read_attribute_value for: %", form));
    }

    return { NONE, 0 };
}

}
