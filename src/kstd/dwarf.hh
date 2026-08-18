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

enum struct Form_Type : u64 {
    STRING         = 0x08,
    IMPLICIT_CONST = 0x21,
};
@enum_to_string(Form_Type);

// struct Form {
//     Form_Type type;
//
//     union {
//
//     };
// };

struct Attribute_Spec {
    Attribute_Type attribute_type;
    Form_Type      form_type;
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

using Abbreviations = Hash_Table<u64, Abbreviation>;

auto parse_abbreviations(Array_View<const u8> bytes) -> Abbreviations {
    Abbreviations abbreviations;
    Byte_Reader reader(bytes);

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

        Array<Attribute_Spec> attribute_specs;
        for (;;) {
            auto [attribute_type_value, attribute_type_ok] = reader.read_uleb128();
            kstd_assert(attribute_type_ok);

            auto [form_type_value, form_type_ok] = reader.read_uleb128();
            kstd_assert(form_type_ok);

            if (attribute_type_value == 0 && form_type_value == 0) break;

            auto attribute_type = static_cast<Attribute_Type>(attribute_type_value);
            auto form_type      = static_cast<Form_Type>(form_type_value);
            s64  implicit_const = 0;
            if (form_type == Form_Type::IMPLICIT_CONST) {
                auto [implicit_const_value, implicit_const_ok] = reader.read_sleb128();
                kstd_assert(implicit_const_ok);
                implicit_const = implicit_const_value;
            }

            attribute_specs.push_back({ attribute_type, form_type, implicit_const });
        }

        Abbreviation abbreviation(tag, has_children, attribute_specs);
        abbreviations.set(abbreviation_code, abbreviation);
    }

    return abbreviations;
}

}
