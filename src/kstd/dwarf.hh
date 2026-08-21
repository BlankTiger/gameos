#pragma once

#include "kstd/enum_name.hh"
#include "kstd/array.hh"
#include "kstd/assert.hh"
#include "kstd/basic.hh"
#include "kstd/byte_reader.hh"
#include "kstd/hash_table.hh"

namespace dwarf {

struct Sections {
    Array_View<const u8> debug_info_bytes;
    Array_View<const u8> debug_abbrev_bytes;
    Array_View<const u8> debug_line_bytes;
    Array_View<const u8> debug_str_bytes;
    Array_View<const u8> debug_line_str_bytes;
    Array_View<const u8> debug_str_offsets_bytes;
    Array_View<const u8> debug_addr_bytes;
    u64                  debug_str_offsets_base;
    u64                  debug_addr_base;
};

enum struct Tag : u64 {
    COMPILE_UNIT = 0x11,
    SUBPROGRAM   = 0x2e,
};
@enum_to_string(Tag);

enum struct Attribute_Type : u64 {
    NAME             = 0x03,
    STMT_LIST        = 0x10,
    LOW_PC           = 0x11,
    HIGH_PC          = 0x12,
    STR_OFFSETS_BASE = 0x72,
    ADDR_BASE        = 0x73,
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

auto parse_attribute_specs(Byte_Reader& debug_abbrev) -> Array<Attribute_Spec> {
    Array<Attribute_Spec> attribute_specs;
    for (;;) {
        auto [attribute_type_value, attribute_type_ok] = debug_abbrev.read_uleb128();
        kstd_assert(attribute_type_ok);

        auto [form_value, form_ok] = debug_abbrev.read_uleb128();
        kstd_assert(form_ok);

        if (attribute_type_value == 0 && form_value == 0) break;

        auto attribute_type = static_cast<Attribute_Type>(attribute_type_value);
        auto form           = static_cast<Form>(form_value);
        s64  implicit_const = 0;
        if (form == Form::IMPLICIT_CONST) {
            auto [implicit_const_value, implicit_const_ok] = debug_abbrev.read_sleb128();
            kstd_assert(implicit_const_ok);
            implicit_const = implicit_const_value;
        }

        attribute_specs.push_back({ attribute_type, form, implicit_const });
    }
    return attribute_specs;
}

static constexpr u64 STOP_ABBREVIATON_CODE = 0;
using Abbreviations = Hash_Table<u64, Abbreviation>;

//
// `debug_abbrev` must be initialized with memory of the .debug_abbrev section.
// It's stopped at the (0, 0, 0) terminator that ends the abbreviations table.
//
auto parse_abbreviations(Byte_Reader& debug_abbrev) -> Abbreviations {
    Abbreviations abbreviations;

    for (;;) {
        auto [abbreviation_code, code_ok] = debug_abbrev.read_uleb128();
        kstd_assert(code_ok);

        if (abbreviation_code == STOP_ABBREVIATON_CODE) break;

        auto [tag_value, tag_ok] = debug_abbrev.read_uleb128();
        kstd_assert(tag_ok);
        auto tag = static_cast<Tag>(tag_value);

        auto [has_children, has_children_ok] = debug_abbrev.read_bool();
        kstd_assert(has_children_ok);

        auto attribute_specs = parse_attribute_specs(debug_abbrev);

        Abbreviation abbreviation(tag, has_children, attribute_specs);
        abbreviations.set(abbreviation_code, abbreviation);
    }

    return abbreviations;
}

enum struct Unit_Type : u8 {
    COMPILE = 0x01,
};
@enum_to_string(Unit_Type);

struct Compilation_Unit_Header {
    u64       length;
    u16       version;
    Unit_Type type;
    u8        address_size;
    u64       abbreviation_offset;
};

constexpr auto DWARF_VERSION = 5;

//
// `debug_info` must be pointed at bytes containing the compilation unit headers.
//
auto parse_compilation_unit_header(Byte_Reader& debug_info) -> Compilation_Unit_Header {
    // @NOTE: This is only for 32-bit DWARF, for 64-bit the handling is different.
    auto [length, length_ok] = debug_info.read_u32();
    kstd_assert(length_ok);

    static constexpr auto LENGTH_FOR_64_BIT_DWARF = 0xffffffff;
    kstd_assert(length != LENGTH_FOR_64_BIT_DWARF, "dwarf: 64-bit DWARF format unsupported");

    auto [version, version_ok] = debug_info.read_u16();
    kstd_assert(version_ok);

    //
    // @TODO(blanktiger): Consider if this shouldn't just be where we bail out
    // instead of asserting, we could just notify instead of halting the entire
    // kernel. Then again, this means that it was not compiled correctly or
    // something.
    //
    kstd_assert(version == DWARF_VERSION, "We only support DWARF5.");

    auto [type_value, type_value_ok] = debug_info.read_u8();
    kstd_assert(type_value_ok);

    auto [address_size, address_size_ok] = debug_info.read_u8();
    kstd_assert(address_size_ok);

    // @NOTE: This is only for 32-bit DWARF.
    auto [abbreviation_offset, abbreviation_offset_ok] = debug_info.read_u32();
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

inline auto normalize_section_offset(Array_View<const u8> section, u32 raw_offset) -> usize {
    if (raw_offset <= section.size) return raw_offset;

    // Linker resolves this DWARF relocation to section address when debug
    // sections are embedded in the loaded kernel image.
    auto section_address = reinterpret_cast<psize>(section.data);
    kstd_assert(static_cast<psize>(raw_offset) >= section_address, "dwarf: section offset out of range");

    auto offset = static_cast<psize>(raw_offset) - section_address;
    kstd_assert(offset <= section.size, "dwarf: section offset out of range");
    return static_cast<usize>(offset);
}

auto read_section_string(Array_View<const u8> section, u32 offset) -> string {
    auto section_offset = normalize_section_offset(section, offset);
    Byte_Reader reader(const_cast<u8*>(section.data + section_offset), section.size - section_offset);
    auto [str, ok] = reader.read_cstring();
    kstd_assert(ok, "dwarf: unterminated string in .debug_str/.debug_line_str");
    return str;
};

auto resolve_strx(
    Array_View<const u8> debug_str_offsets_bytes,
    Array_View<const u8> debug_str_bytes,
    u64 str_offsets_base,
    u64 index
) -> string {
    auto section_offset = normalize_section_offset(debug_str_offsets_bytes, str_offsets_base);
    Byte_Reader reader(debug_str_offsets_bytes);
    auto skip_ok = reader.skip(section_offset + index * sizeof(u32));
    kstd_assert(skip_ok, "dwarf: strx index out of range");

    auto [str_offset, str_offset_ok] = reader.read_u32();
    kstd_assert(str_offset_ok);

    return read_section_string(debug_str_bytes, str_offset);
}

auto resolve_addrx(
    Array_View<const u8> debug_addr_bytes,
    u8  address_size,
    u64 addr_base,
    u64 index
) -> u64 {
    auto section_offset = normalize_section_offset(debug_addr_bytes, addr_base);
    Byte_Reader reader(debug_addr_bytes);
    auto skip_ok = reader.skip(section_offset + index * address_size);
    kstd_assert(skip_ok, "dwarf: addrx index out of range");

    if (address_size == 8) {
        auto [address, ok] = reader.read_u64();
        kstd_assert(ok);
        return address;
    } else {
        auto [address, ok] = reader.read_u32();
        kstd_assert(ok);
        return address;
    }
}

//
// Consumes exactly the bytes `form` occupies in the Debug Info Entry (DIE)
// stream and, where the form has a resolvable value (as opposed to an index
// into a table this parser doesn't build, e.g. strx/addrx), returns it.
//
auto read_attribute_value(
    Byte_Reader&    debug_info,
    Form            form,
    u8              address_size,
    s64             implicit_const,
    const Sections& sections
) -> Attribute_Value {
    using enum Attribute_Value_Kind;

    switch (form) {
        case Form::ADDR: {
            if (address_size == 8) {
                auto [address, ok] = debug_info.read_u64();
                // @TODO(blanktiger): Reconsider making this function return an error instead.
                kstd_assert(ok);
                return { UNSIGNED, address };
            } else {
                auto [address, ok] = debug_info.read_u32();
                kstd_assert(ok);
                return { UNSIGNED, address };
            }
        } break;

        case Form::DATA1:
        case Form::REF1: {
            auto [value, ok] = debug_info.read_u8();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA2:
        case Form::REF2: {
            auto [value, ok] = debug_info.read_u16();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA4:
        case Form::REF_ADDR:
        case Form::REF4:
        case Form::SEC_OFFSET: {
            auto [value, ok] = debug_info.read_u32();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA8:
        case Form::REF8:
        case Form::REF_SIG8: {
            auto [value, ok] = debug_info.read_u64();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::DATA16: {
            auto [bytes, ok] = debug_info.read_bytes(16);
            kstd_assert(ok);
            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK1: {
            auto [length, length_ok] = debug_info.read_u8();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = debug_info.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK2: {
            auto [length, length_ok] = debug_info.read_u16();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = debug_info.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK4: {
            auto [length, length_ok] = debug_info.read_u32();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = debug_info.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::BLOCK:
        case Form::EXPRLOC: {
            auto [length, length_ok] = debug_info.read_uleb128();
            kstd_assert(length_ok);

            auto [bytes, bytes_ok] = debug_info.read_bytes(length);
            kstd_assert(bytes_ok);

            return { .kind = BLOCK, .v_block = bytes };
        } break;

        case Form::STRING: {
            auto [value, ok] = debug_info.read_cstring();
            kstd_assert(ok);
            return { .kind = STRING, .v_string = value };
        } break;

        case Form::STRP:
        case Form::LINE_STRP: {
            auto [offset, ok] = debug_info.read_u32();
            kstd_assert(ok);

            auto section = form == Form::STRP ? sections.debug_str_bytes : sections.debug_line_str_bytes;
            auto normalized_offset = normalize_section_offset(section, offset);
            auto value = read_section_string(section, normalized_offset);
            return { .kind = STRING, .v_string = value };
        } break;

        case Form::SDATA: {
            auto [value, ok] = debug_info.read_sleb128();
            kstd_assert(ok);
            return { .kind = SIGNED, .v_signed = value };
        } break;

        case Form::UDATA:
        case Form::REF_UDATA: {
            auto [value, ok] = debug_info.read_uleb128();
            kstd_assert(ok);
            return { UNSIGNED, value };
        } break;

        case Form::FLAG: {
            auto [value, ok] = debug_info.read_u8();
            kstd_assert(ok);
            return { .kind = FLAG, .v_unsigned = value };
        } break;

        case Form::FLAG_PRESENT: return { .kind = FLAG, .v_unsigned = 1 };

        case Form::IMPLICIT_CONST: return { .kind = SIGNED, .v_signed = implicit_const };

        case Form::STRX: {
            auto [index, ok] = debug_info.read_uleb128();
            kstd_assert(ok);
            return { .kind = STRING, .v_string = resolve_strx(sections.debug_str_offsets_bytes, sections.debug_str_bytes, sections.debug_str_offsets_base, index) };
        } break;

        case Form::ADDRX: {
            auto [index, ok] = debug_info.read_uleb128();
            kstd_assert(ok);
            return { UNSIGNED, resolve_addrx(sections.debug_addr_bytes, address_size, sections.debug_addr_base, index) };
        } break;

        case Form::LOCLISTX:
        case Form::RNGLISTX: {
            auto [index, ok] = debug_info.read_uleb128();
            kstd_assert(ok);
            (void)index;
            return { NONE, 0 };
        } break;

        case Form::STRX1: {
            auto [index, ok] = debug_info.read_u8();
            kstd_assert(ok);
            return { .kind = STRING, .v_string = resolve_strx(sections.debug_str_offsets_bytes, sections.debug_str_bytes, sections.debug_str_offsets_base, index) };
        } break;

        case Form::STRX2: {
            auto [index, ok] = debug_info.read_u16();
            kstd_assert(ok);
            return { .kind = STRING, .v_string = resolve_strx(sections.debug_str_offsets_bytes, sections.debug_str_bytes, sections.debug_str_offsets_base, index) };
        } break;

        case Form::STRX3: {
            auto [bytes, ok] = debug_info.read_bytes(3);
            kstd_assert(ok);
            u32 index = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
            return { .kind = STRING, .v_string = resolve_strx(sections.debug_str_offsets_bytes, sections.debug_str_bytes, sections.debug_str_offsets_base, index) };
        } break;

        case Form::STRX4: {
            auto [index, ok] = debug_info.read_u32();
            kstd_assert(ok);
            return { .kind = STRING, .v_string = resolve_strx(sections.debug_str_offsets_bytes, sections.debug_str_bytes, sections.debug_str_offsets_base, index) };
        } break;

        case Form::ADDRX1: {
            auto [index, ok] = debug_info.read_u8();
            kstd_assert(ok);
            return { UNSIGNED, resolve_addrx(sections.debug_addr_bytes, address_size, sections.debug_addr_base, index) };
        } break;

        case Form::ADDRX2: {
            auto [index, ok] = debug_info.read_u16();
            kstd_assert(ok);
            return { UNSIGNED, resolve_addrx(sections.debug_addr_bytes, address_size, sections.debug_addr_base, index) };
        } break;

        case Form::ADDRX3: {
            auto [bytes, ok] = debug_info.read_bytes(3);
            kstd_assert(ok);
            u32 index = bytes[0] | (bytes[1] << 8) | (bytes[2] << 16);
            return { UNSIGNED, resolve_addrx(sections.debug_addr_bytes, address_size, sections.debug_addr_base, index) };
        } break;

        case Form::ADDRX4: {
            auto [index, ok] = debug_info.read_u32();
            kstd_assert(ok);
            return { UNSIGNED, resolve_addrx(sections.debug_addr_bytes, address_size, sections.debug_addr_base, index) };
        } break;

        case Form::REF_SUP4:
        case Form::STRP_SUP: {
            auto [value, ok] = debug_info.read_u32();
            kstd_assert(ok);
            (void)value;
            return { NONE, 0 };
        } break;

        case Form::REF_SUP8: {
            auto [value, ok] = debug_info.read_u64();
            kstd_assert(ok);
            (void)value;
            return { NONE, 0 };
        } break;

        case Form::INDIRECT: {
            auto [actual_form, ok] = debug_info.read_uleb128();
            kstd_assert(ok);
            return read_attribute_value(
                debug_info,
                static_cast<Form>(actual_form),
                address_size,
                implicit_const,
                sections
            );
        } break;

        default: unreachable(csprint("Unhandled read_attribute_value for: %", form));
    }

    return { NONE, 0 };
}


struct Subprogram_Info {
    string name;
    psize  low_pc;
    psize  high_pc; // Exclusive, already normalized from offset-form Attribute_Type::HIGH_PC.

    auto format() const -> string {
        return sprint("Subprogram_Info{ %, 0x%, 0x% }", name, low_pc, high_pc);
    }
};


struct Parse_Compilation_Unit_Result {
    Array<Subprogram_Info> infos;

    u32  debug_line_offset;
    bool has_debug_line_offset;
};

//
// Walks every Debug Info Entry (DIE) in one Compilation Unit's .debug_info
// slice, collecting subprograms that have both a name and an address range.
//
// `debug_info` must be positioned right after the Compilation Unit's header.
//
auto parse_compilation_unit_debug_information_entries(
    Byte_Reader& debug_info,
    usize compilation_unit_end,
    const Abbreviations& abbreviations,
    u8 address_size,
    Sections& sections
) -> Parse_Compilation_Unit_Result {
    Array<Subprogram_Info> infos;
    u32  debug_line_offset     = 0;
    bool has_debug_line_offset = false;

    bool has_debug_str_offsets_base = false;
    bool has_debug_addr_base        = false;

    // Initial size chosen arbitrarily.
    Array<bool> scope_stack(8);
    for (;;) {
        if (debug_info.current_offset >= compilation_unit_end) break;

        auto [abbreviation_code, code_ok] = debug_info.read_uleb128();
        kstd_assert(code_ok);

        if (abbreviation_code == STOP_ABBREVIATON_CODE) {
            // If we break here this means we finished parsing this compilation unit.
            if (scope_stack.size == 0) break;
            scope_stack.pop_back();
            continue;
        }

        const Abbreviation* declaration = abbreviations.find(abbreviation_code);
        kstd_assert(declaration != nullptr, "Shouldn't be possible for it to not exist if it's referenced. Compiler error?");

        bool is_subprogram = declaration->tag == Tag::SUBPROGRAM;
        bool has_name      = false;
        bool has_low_pc    = false;
        bool has_high_pc   = false;

        string name{};
        psize  low_pc{};
        psize  high_pc_raw{};
        bool   high_pc_is_offset = false;

        for (const auto& spec : declaration->attribute_specs) {
            auto value = read_attribute_value(
                debug_info,
                spec.form,
                address_size,
                spec.implicit_const,
                sections
            );

            if (is_subprogram) {
                switch (spec.attribute_type) {
                    case Attribute_Type::NAME: {
                        kstd_assert(value.kind == Attribute_Value_Kind::STRING);
                        name     = value.v_string;
                        has_name = true;
                    } break;

                    case Attribute_Type::LOW_PC: {
                        kstd_assert(value.kind == Attribute_Value_Kind::UNSIGNED);
                        low_pc     = value.v_unsigned;
                        has_low_pc = true;
                    } break;

                    case Attribute_Type::HIGH_PC: {
                        kstd_assert(value.kind == Attribute_Value_Kind::UNSIGNED);
                        high_pc_raw       = value.v_unsigned;
                        high_pc_is_offset = spec.form != Form::ADDR;
                        has_high_pc       = true;
                    } break;

                    // Nothing else interests us here for now.
                    default: break;
                }
            }

            if (spec.attribute_type == Attribute_Type::STMT_LIST) {
                kstd_assert(declaration->tag == Tag::COMPILE_UNIT);
                kstd_assert(!has_debug_line_offset, "Should only have one per compilation unit.");
                kstd_assert(value.kind == Attribute_Value_Kind::UNSIGNED);
                debug_line_offset     = value.v_unsigned;
                has_debug_line_offset = true;
            }

            if (spec.attribute_type == Attribute_Type::STR_OFFSETS_BASE) {
                kstd_assert(declaration->tag == Tag::COMPILE_UNIT);
                kstd_assert(!has_debug_str_offsets_base, "Should only have one per compilation unit.");
                kstd_assert(value.kind == Attribute_Value_Kind::UNSIGNED);
                sections.debug_str_offsets_base = value.v_unsigned;
                has_debug_str_offsets_base      = true;
            }

            if (spec.attribute_type == Attribute_Type::ADDR_BASE) {
                kstd_assert(declaration->tag == Tag::COMPILE_UNIT);
                kstd_assert(!has_debug_addr_base, "Should only have one per compilation unit.");
                kstd_assert(value.kind == Attribute_Value_Kind::UNSIGNED);
                sections.debug_addr_base = value.v_unsigned;
                has_debug_addr_base      = true;
            }
        }

        if (is_subprogram && has_name && has_low_pc && has_high_pc) {
            psize high_pc = high_pc_is_offset ? low_pc + high_pc_raw : high_pc_raw;
            infos.push_back({ name, low_pc, high_pc });
        }

        if (declaration->has_children)
            scope_stack.push_back(true);
    }

    return { infos, debug_line_offset, has_debug_line_offset };
}

enum struct Line_Content_Type : u64 {
    PATH            = 0x01,
    DIRECTORY_INDEX = 0x02,
};
@enum_to_string(Line_Content_Type);

struct Entry_Format {
    Line_Content_Type type;
    Form              form;

    auto format() const -> string {
        return sprint("Entry_Format{ %, % }", type, form);
    }
};
using Entry_Formats = Array<Entry_Format>;

auto parse_entry_formats(Byte_Reader& debug_line) -> Entry_Formats {
    auto [entry_format_count, entry_format_count_ok] = debug_line.read_u8();
    kstd_assert(entry_format_count_ok);

    Entry_Formats entry_formats(entry_format_count);
    for (u8 index = 0; index < entry_format_count; ++index) {
        auto [line_content_type_value, line_content_type_ok] = debug_line.read_uleb128();
        auto [form_value,              form_ok]              = debug_line.read_uleb128();

        kstd_assert(line_content_type_ok);
        kstd_assert(form_ok);

        entry_formats.push_back({
            static_cast<Line_Content_Type>(line_content_type_value),
            static_cast<Form>(form_value)
        });
    }

    return entry_formats;
}

auto parse_directories_or_file_names(
    Byte_Reader& debug_line,
    usize address_size,
    const Entry_Formats& entry_formats,
    const Sections& sections
) -> Array<string> {
    auto [count, count_ok] = debug_line.read_uleb128();
    kstd_assert(count_ok);

    Array<string> directories_or_file_names(count);
    for (u64 index = 0; index < count; ++index) {
        string path{};
        bool   has_path = false;

        defer({
            kstd_assert(has_path);
            directories_or_file_names.push_back(path);
        });

        for (const auto& [line_content_type, form] : entry_formats) {
            auto value = read_attribute_value(debug_line, form, address_size, 0, sections);

            if (line_content_type == Line_Content_Type::PATH) {
                kstd_assert(!has_path);
                kstd_assert(value.kind == Attribute_Value_Kind::STRING);

                has_path = true;
                path     = value.v_string;
            }
        }
    }

    return directories_or_file_names;
}

struct Debug_Line_Header {
    u64  unit_length;
    u16  version;
    u8   address_size;
    u8   segment_selector_size;
    u64  header_length;
    u8   minimum_instruction_length;
    u8   maximum_operations_per_instruction;
    bool default_value_of_is_stmt_register;
    s8   line_base;
    u8   line_range;

    u8                   opcode_base;
    Array_View<const u8> standard_opcode_lengths;

    // Before each of those arrays .debug_line encodes their count.
    Entry_Formats directory_entry_formats;
    Array<string> directories;
    Entry_Formats file_name_entry_formats;
    Array<string> file_names;
};

auto parse_debug_line_header(Byte_Reader& debug_line, const Sections& sections) -> Debug_Line_Header {
    auto [unit_length,                        unit_length_ok]                        = debug_line.read_u32();
    auto [version,                            version_ok]                            = debug_line.read_u16();
    auto [address_size,                       address_size_ok]                       = debug_line.read_u8();
    auto [segment_selector_size,              segment_selector_size_ok]              = debug_line.read_u8();
    auto [header_length,                      header_length_ok]                      = debug_line.read_u32(); // 32-bit DWARF
    auto [minimum_instruction_length,         minimum_instruction_length_ok]         = debug_line.read_u8();
    auto [maximum_operations_per_instruction, maximum_operations_per_instruction_ok] = debug_line.read_u8();
    auto [default_value_of_is_stmt_register,  default_value_of_is_stmt_register_ok]  = debug_line.read_bool();
    auto [line_base,                          line_base_ok]                          = debug_line.read_s8();
    auto [line_range,                         line_range_ok]                         = debug_line.read_u8();
    auto [opcode_base,                        opcode_base_ok]                        = debug_line.read_u8();

    kstd_assert(unit_length_ok);
    kstd_assert(version_ok);
    kstd_assert(version == DWARF_VERSION);
    kstd_assert(address_size_ok);
    kstd_assert(segment_selector_size_ok);
    kstd_assert(header_length_ok);
    kstd_assert(minimum_instruction_length_ok);
    kstd_assert(maximum_operations_per_instruction_ok);
    kstd_assert(default_value_of_is_stmt_register_ok);
    kstd_assert(line_base_ok);
    kstd_assert(line_range_ok);
    kstd_assert(opcode_base_ok);

    auto [standard_opcode_lengths, standard_opcode_lengths_ok] = debug_line.read_bytes(opcode_base - 1);
    kstd_assert(standard_opcode_lengths_ok);

    auto directory_entry_formats = parse_entry_formats(debug_line);
    auto directories = parse_directories_or_file_names(
        debug_line,
        address_size,
        directory_entry_formats,
        sections
    );

    auto file_name_entry_formats = parse_entry_formats(debug_line);
    // @TODO(blanktiger): Join directory paths and file paths if file_name_entry_formats contain DIRECTORY.
    auto file_names = parse_directories_or_file_names(
        debug_line,
        address_size,
        file_name_entry_formats,
        sections
    );

    Debug_Line_Header header{
        .unit_length                        = unit_length,
        .version                            = version,
        .address_size                       = address_size,
        .segment_selector_size              = segment_selector_size,
        .header_length                      = header_length,
        .minimum_instruction_length         = minimum_instruction_length,
        .maximum_operations_per_instruction = maximum_operations_per_instruction,
        .default_value_of_is_stmt_register  = default_value_of_is_stmt_register,
        .line_base                          = line_base,
        .line_range                         = line_range,
        .opcode_base                        = opcode_base,
        .standard_opcode_lengths            = standard_opcode_lengths,
        .directory_entry_formats            = directory_entry_formats,
        .directories                        = directories,
        .file_name_entry_formats            = file_name_entry_formats,
        .file_names                         = file_names
    };

    return header;
}

enum struct Line_Number_Standard_Opcode : u64 {
    COPY               = 0x01,
    ADVANCE_PC         = 0x02,
    ADVANCE_LINE       = 0x03,
    SET_FILE           = 0x04,
    SET_COLUMN         = 0x05,
    NEGATE_STMT        = 0x06,
    SET_BASIC_BLOCK    = 0x07,
    CONST_ADD_PC       = 0x08,
    FIXED_ADVANCE_PC   = 0x09,
    SET_PROLOGUE_END   = 0x0a,
    SET_PROLOGUE_BEGIN = 0x0b,
    SET_ISA            = 0x0c,
};
@enum_to_string(Line_Number_Standard_Opcode);


constexpr auto EXTENDED_OPCODE_MARKER = 0;

enum struct Line_Number_Extended_Opcode : u64 {
    END_SEQUENCE = 0x01,
    SET_ADDRESS  = 0x02,
};
@enum_to_string(Line_Number_Extended_Opcode);

struct Source_Row {
    psize  address;
    string file_name;
    s32    line;
};

struct Debug_Line_State {
    psize address        = 0;
    u64   op_index       = 0;
    u32   file_index     = 0;
    s32   line           = 1;
    u32   column         = 0;
    bool  is_stmt;
    bool  basic_block    = false;
    bool  end_sequence   = false;
    bool  prologue_end   = false;
    bool  epilogue_begin = false;
    u32   isa            = 0;
    u32   discriminator  = 0;

    Debug_Line_State() = delete;

    explicit Debug_Line_State(bool default_is_stmt) : is_stmt(default_is_stmt) {}
};

auto parse_line_table(const Sections& sections, u32 debug_line_offset) -> Array<Source_Row> {
    Byte_Reader debug_line(sections.debug_line_bytes);
    auto normalized_offset = normalize_section_offset(sections.debug_line_bytes, debug_line_offset);
    auto skip_ok = debug_line.skip(normalized_offset);
    kstd_assert(skip_ok);

    const auto unit_start = debug_line.current_offset;
    const auto header     = parse_debug_line_header(debug_line, sections);

    auto payload_start = unit_start + sizeof(u32);
    kstd_assert(payload_start <= debug_line.size);
    kstd_assert(header.unit_length <= static_cast<u64>(debug_line.size - payload_start));

    auto unit_end = payload_start + static_cast<usize>(header.unit_length);

    Debug_Line_State state(header.default_value_of_is_stmt_register);

    Array<Source_Row> rows;
    while (debug_line.current_offset < unit_end) {
        auto [opcode, ok] = debug_line.read_u8();
        kstd_assert(ok);

        if (opcode == EXTENDED_OPCODE_MARKER) {
            // length includes extended_opcode value, so payload is length - 1
            auto [length,          length_ok]          = debug_line.read_uleb128();
            auto [extended_opcode, extended_opcode_ok] = debug_line.read_u8();

            kstd_assert(length_ok);
            kstd_assert(extended_opcode_ok);

            using enum Line_Number_Extended_Opcode;
            switch (static_cast<Line_Number_Extended_Opcode>(extended_opcode)) {
                case END_SEQUENCE: {
                    kstd_assert(length == 1);
                    //
                    // This might appear like it has no effect whatsoever, but
                    // that's only because our Source_Row doesn't retain that
                    // information. Documentation on DWARF5 says that this is
                    // important so.. don't remove it.
                    //
                    state.end_sequence = true;

                    rows.push_back({ state.address, header.file_names[state.file_index], state.line });
                    state = Debug_Line_State{header.default_value_of_is_stmt_register};
                } break;

                case SET_ADDRESS: {
                    psize new_address;
                    if (header.address_size == 4) {
                        auto [address, address_ok] = debug_line.read_u32();
                        kstd_assert(address_ok);

                        new_address = address;
                    }
                    else if (header.address_size == 8) {
                        auto [address, address_ok] = debug_line.read_u64();
                        kstd_assert(address_ok);

                        new_address = address;
                    }
                    else {
                        [[unlikely]]
                        unreachable();
                    }

                    auto skip_ok = debug_line.skip(length - header.address_size - 1);
                    kstd_assert(skip_ok);

                    state.address  = new_address;
                    state.op_index = 0;
                } break;

                default: {
                    // For now we just consume those bytes without doing anything with them.
                    auto skip_ok = debug_line.skip(length - 1);
                    kstd_assert(skip_ok);
                } break;
            }
        }
        else if (opcode < header.opcode_base) {
            using enum Line_Number_Standard_Opcode;
            switch (static_cast<Line_Number_Standard_Opcode>(opcode)) {
                case COPY: {
                    state.basic_block    = false;
                    state.prologue_end   = false;
                    state.epilogue_begin = false;
                    state.discriminator  = 0;

                    rows.push_back({ state.address, header.file_names[state.file_index], state.line });
                } break;

                case ADVANCE_PC: {
                    auto [op_advance, op_advance_ok] = debug_line.read_uleb128();
                    kstd_assert(op_advance_ok);

                    state.address += header.minimum_instruction_length * ((state.op_index + op_advance) / header.maximum_operations_per_instruction);
                    state.op_index = (state.op_index + op_advance) % header.maximum_operations_per_instruction;
                } break;

                case ADVANCE_LINE: {
                    auto [line_advance, line_advance_ok] = debug_line.read_sleb128();
                    kstd_assert(line_advance_ok);

                    state.line += line_advance;
                } break;

                case SET_FILE: {
                    auto [new_file_index, new_file_index_ok] = debug_line.read_uleb128();
                    kstd_assert(new_file_index_ok);

                    state.file_index = new_file_index;
                } break;

                case SET_COLUMN: {
                    auto [new_column, new_column_ok] = debug_line.read_uleb128();
                    kstd_assert(new_column_ok);

                    state.column = new_column;
                } break;

                case NEGATE_STMT: {
                    state.is_stmt = !state.is_stmt;
                } break;

                case SET_BASIC_BLOCK: {
                    state.basic_block = true;
                } break;

                case CONST_ADD_PC: {
                    // Do what a special opcode 255 would do to the address.
                    auto adjusted_opcode = 255 - header.opcode_base;
                    auto op_advance      = adjusted_opcode / header.line_range;

                    state.address += op_advance * header.minimum_instruction_length;
                } break;

                case FIXED_ADVANCE_PC: {
                    auto [op_advance, op_advance_ok] = debug_line.read_u16();
                    kstd_assert(op_advance_ok);

                    state.address += op_advance;
                    state.op_index = 0;
                } break;

                case SET_PROLOGUE_END: {
                    state.prologue_end = true;
                } break;

                case SET_PROLOGUE_BEGIN: {
                    state.epilogue_begin = true;
                } break;

                case SET_ISA: {
                    auto [new_isa, new_isa_ok] = debug_line.read_uleb128();
                    kstd_assert(new_isa_ok);

                    state.isa = new_isa;
                } break;
            }
        }
        else {
            // Special opcode handling.
            auto adjusted_opcode = opcode - header.opcode_base;
            auto op_advance      = adjusted_opcode / header.line_range;
            auto line_advance    = header.line_base + (adjusted_opcode % header.line_range);

            state.address += op_advance * header.minimum_instruction_length;
            state.line    += line_advance;
            state.op_index = (state.op_index + op_advance) % header.maximum_operations_per_instruction;

            rows.push_back({ state.address, header.file_names[state.file_index], state.line });
        }
    }
    kstd_assert(debug_line.current_offset == unit_end);

    return rows;
}

}
