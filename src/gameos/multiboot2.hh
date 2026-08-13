#pragma once

#include "kstd/basic.hh"
#include "kstd/pointer_utils.hh"

namespace boot {

enum class Multiboot_Info_Flag : u32 {
    HAS_MMAP = 1u << 6,
    HAS_VBE = 1u << 11,
    HAS_FRAMEBUFFER = 1u << 12,
};

constexpr u32 MULTIBOOT2_MAGIC = 0x36D76289;

enum class Multiboot2_Tag_Type : u16 {
    END              = 0,
    CMDLINE          = 1,
    BOOT_LOADER_NAME = 2,
    MODULE           = 3,
    BASIC_MEMINFO    = 4,
    BOOT_DEVICE      = 5,
    MEMORY_MAP       = 6,
    VBE              = 7,
    FRAMEBUFFER      = 8,
    ACPI_OLD_RSDP    = 14,
    ACPI_NEW_RSDP    = 15,
};

struct Multiboot2_Memory_Map_Entry {
    u64 addr;
    u64 len;
    u32 type;
    u32 zero;
} __attribute__((packed));

struct Multiboot2_Tag {
    Multiboot2_Tag_Type type;
    u16 flags;
    u32 size;

    template <typename T>
    auto as() const -> const T* {
        return reinterpret_cast<const T*>(this);
    }

    auto payload() const -> const void* {
        return reinterpret_cast<const void*>(ptr_addr(this) + sizeof(*this));
    }

    template <typename T>
    auto payload_as() const -> const T* {
        return reinterpret_cast<const T*>(payload());
    }

    auto next() const -> const Multiboot2_Tag* {
        return reinterpret_cast<const Multiboot2_Tag*>(ptr_addr(this) + ((size + 7u) & ~7u));
    }
} __attribute__((packed));

struct Multiboot2_Info {
    u32 total_size;
    u32 reserved;

    auto first_tag() const -> const Multiboot2_Tag* {
        return reinterpret_cast<const Multiboot2_Tag*>(ptr_addr(this) + sizeof(*this));
    }

    auto end_tag() const -> const Multiboot2_Tag* {
        return reinterpret_cast<const Multiboot2_Tag*>(ptr_addr(this) + total_size);
    }
} __attribute__((packed));

struct Multiboot2_Memory_Map_Tag {
    static constexpr auto TAG_TYPE = Multiboot2_Tag_Type::MEMORY_MAP;

    Multiboot2_Tag tag;
    u32 entry_size;
    u32 entry_version;

    auto first_entry() const -> const Multiboot2_Memory_Map_Entry* {
        return reinterpret_cast<const Multiboot2_Memory_Map_Entry*>(ptr_addr(this) + sizeof(*this));
    }

    auto end_entry() const -> const Multiboot2_Memory_Map_Entry* {
        return reinterpret_cast<const Multiboot2_Memory_Map_Entry*>(ptr_addr(this) + tag.size);
    }
} __attribute__((packed));

struct Multiboot2_Module_Tag {
    static constexpr auto TAG_TYPE = Multiboot2_Tag_Type::MODULE;

    Multiboot2_Tag tag;
    u32 mod_start;
    u32 mod_end;
    u32 string;
    u32 reserved;

    auto string_ptr() const -> const char* {
        return reinterpret_cast<const char*>(static_cast<uintptr_t>(string));
    }
} __attribute__((packed));

struct Multiboot2_Framebuffer_Tag {
    static constexpr auto TAG_TYPE = Multiboot2_Tag_Type::FRAMEBUFFER;

    Multiboot2_Tag tag;
    u64 framebuffer_addr;
    u32 framebuffer_pitch;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u8 framebuffer_bpp;
    u8 framebuffer_type;
    u16 reserved;
    union {
        struct {
            u32 framebuffer_palette_addr;
            u16 framebuffer_palette_num_colors;
        } palette;
        struct {
            u8 framebuffer_red_field_position;
            u8 framebuffer_red_mask_size;
            u8 framebuffer_green_field_position;
            u8 framebuffer_green_mask_size;
            u8 framebuffer_blue_field_position;
            u8 framebuffer_blue_mask_size;
        } direct_color;
    } framebuffer_info;
} __attribute__((packed));

// Multiboot2 tags 14/15: RSDP copy sits in tag payload (after Multiboot2_Tag).
// Parse payload as acpi::RSDP.
struct Multiboot2_ACPI_Old_RSDP_Tag {
    static constexpr auto TAG_TYPE = Multiboot2_Tag_Type::ACPI_OLD_RSDP;

    Multiboot2_Tag tag;
} __attribute__((packed));

struct Multiboot2_ACPI_New_RSDP_Tag {
    static constexpr auto TAG_TYPE = Multiboot2_Tag_Type::ACPI_NEW_RSDP;

    Multiboot2_Tag tag;
} __attribute__((packed));

template <typename Tag>
auto find_multiboot2_tag(const Multiboot2_Info* mbi) -> const Tag* {
    auto*       tag = mbi->first_tag();
    const auto* end = mbi->end_tag();

    // Don't change to tag != end because end is one past the real end + if a
    // tag is corrupted then it could get bad.
    while (ptr_addr(tag) < ptr_addr(end)) {
        if (tag->type == Tag::TAG_TYPE) {
            return tag->as<Tag>();
        }

        tag = tag->next();
    }

    return nullptr;
}

}  // namespace boot
