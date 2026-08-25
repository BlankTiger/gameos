#pragma once

#include "kstd/array.hh"
#include "kstd/basic.hh"
#include "kstd/cstring.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/assert.hh"

#include "gameos/cpuid.hh"
#include "gameos/multiboot2.hh"
#include "gameos/serial_format.hh"

namespace acpi {

constexpr s64 RSDP_V1_SIZE            = 20;
constexpr s64 MAX_CPUS                = 64;
constexpr s64 MAX_IOAPICS             = 4;
constexpr s64 MAX_INTERRUPT_OVERRIDES = 32;

inline constexpr char RSDP_SIGNATURE[8] = {
    'R', 'S', 'D', ' ', 'P', 'T', 'R', ' '
};
inline constexpr char MADT_SIGNATURE[4] = { 'A', 'P', 'I', 'C' };
inline constexpr char RSDT_SIGNATURE[4] = { 'R', 'S', 'D', 'T' };
inline constexpr char XSDT_SIGNATURE[4] = { 'X', 'S', 'D', 'T' };

// Every ACPI table: sum of all bytes in the structure is 0 mod 256.
force_inline auto checksum_ok(const void* table, s64 size) -> bool {
    u8 sum = 0;
    const auto* bytes = cast(const u8*)table;
    for (s64 i = 0; i < size; ++i) {
        sum = cast(u8)(sum + bytes[i]);
    }
    return sum == 0;
}

struct RSDP {
    char signature[8];
    u8   checksum;
    char oem_id[6];
    u8   revision;
    u32  rsdt_address;
    // ACPI 2.0+ only:
    u32  length;
    u64  xsdt_address;
    u8   extended_checksum;
    u8   reserved[3];
} __attribute__((packed));

static_assert(size_of(RSDP) == 36);

struct SDT_Header {
    char signature[4];
    u32  length;
    u8   revision;
    u8   checksum;
    char oem_id[6];
    char oem_table_id[8];
    u32  oem_revision;
    u32  creator_id;
    u32  creator_revision;
} __attribute__((packed));

static_assert(size_of(SDT_Header) == 36);

// MADT Multiple APIC Flags.
union MADT_Flags {
    struct {
        u32 pcat_compat : 1;  // 1 = system also has dual 8259 PICs
        u32 reserved    : 31;
    };
    u32 raw;
} __attribute__((packed));

static_assert(size_of(MADT_Flags) == 4);

// Local APIC / Local x2APIC Processor Local APIC Flags.
union Local_APIC_Flags {
    struct {
        u32 enabled        : 1; // 1 = processor ready for use
        u32 online_capable : 1; // 1 = OSPM may online later (if enabled clear)
        u32 reserved       : 30;
    };
    u32 raw;
} __attribute__((packed));

static_assert(size_of(Local_APIC_Flags) == 4);

enum struct Polarity : u16 {
    BUS_DEFAULT = 0b00,
    ACTIVE_HIGH = 0b01,
    RESERVED    = 0b10,
    ACTIVE_LOW  = 0b11,
};

enum struct Trigger : u16 {
    BUS_DEFAULT = 0b00,
    EDGE        = 0b01,
    RESERVED    = 0b10,
    LEVEL       = 0b11,
};

// MPS INTI flags (Interrupt Source Override / NMI).
union MPS_INTI_Flags {
    struct {
        Polarity polarity : 2;
        Trigger  trigger  : 2;
        u16      reserved : 12;
    };
    u16 raw;
} __attribute__((packed));

static_assert(size_of(MPS_INTI_Flags) == 2);

struct MADT {
    SDT_Header header;
    u32        local_apic_address;
    MADT_Flags flags;
} __attribute__((packed));

static_assert(size_of(MADT) == 44);

enum struct MADT_Entry_Type : u8 {
    LOCAL_APIC   = 0,
    IOAPIC       = 1,
    ISO          = 2, // interrupt source override
    LOCAL_X2APIC = 9,
};

struct MADT_Entry_Header {
    u8 type;
    u8 length;
} __attribute__((packed));

struct MADT_Local_APIC {
    MADT_Entry_Header header;
    u8                acpi_processor_uid;
    u8                apic_id;
    Local_APIC_Flags  flags;
} __attribute__((packed));

static_assert(size_of(MADT_Local_APIC) == 8);

struct MADT_IOAPIC {
    MADT_Entry_Header header;
    u8                ioapic_id;
    u8                reserved;
    u32               address;
    u32               gsi_base;
} __attribute__((packed));

static_assert(size_of(MADT_IOAPIC) == 12);

struct MADT_ISO {
    MADT_Entry_Header header;
    u8                bus;
    u8                source;
    u32               gsi;
    MPS_INTI_Flags    flags;
} __attribute__((packed));

static_assert(size_of(MADT_ISO) == 10);

struct MADT_Local_X2APIC {
    MADT_Entry_Header header;
    u16               reserved;
    u32               x2apic_id;
    Local_APIC_Flags  flags;
    u32               acpi_processor_uid;
} __attribute__((packed));

static_assert(size_of(MADT_Local_X2APIC) == 16);

struct CPU_Desc {
    u32  apic_id;
    u32  acpi_processor_id;
    bool enabled;
    bool is_bsp;
};

struct IOAPIC_Desc {
    u8  id;
    u32 gsi_base;
    u32 mmio_phys;
};

struct ISA_Override {
    u8             isa_irq;
    u32            gsi;
    MPS_INTI_Flags flags;
};

struct MADT_Info {
    bool         valid;
    u32          local_apic_address;
    MADT_Flags   flags;
    u32          bsp_apic_id;

    Bounded_Array<CPU_Desc,     MAX_CPUS>                cpus;
    Bounded_Array<IOAPIC_Desc,  MAX_IOAPICS>             ioapics;
    Bounded_Array<ISA_Override, MAX_INTERRUPT_OVERRIDES> overrides;
};

namespace hidden {
    inline MADT_Info madt_info{};
}

force_inline auto madt() -> const MADT_Info& {
    return hidden::madt_info;
}

auto rsdp_valid(const RSDP* rsdp) -> bool {
    if (rsdp == nullptr) return false;
    if (!kstd_memeq(rsdp->signature, RSDP_SIGNATURE)) return false;
    if (!checksum_ok(rsdp, RSDP_V1_SIZE)) return false;
    if (rsdp->revision >= 2) {
        if (rsdp->length < RSDP_V1_SIZE) return false;
        if (!checksum_ok(rsdp, rsdp->length)) return false;
    }
    return true;
}

auto find_rsdp_in_range(u64 start, u64 end) -> const RSDP* {
    // RSDP is 16-byte aligned in the BIOS areas.
    for (u64 addr = start; addr + RSDP_V1_SIZE <= end; addr += 16) {
        const auto* candidate = addr_as<const RSDP*>(addr);
        if (rsdp_valid(candidate)) return candidate;
    }
    return nullptr;
}

auto find_rsdp_bios_scan() -> const RSDP* {
    u16 extended_bios_data_area_segment = 0;
    kstd_memcpy(&extended_bios_data_area_segment, addr_as<const u8*>(0x40E), size_of(extended_bios_data_area_segment));
    // Word at 0x40E is a real-mode segment, not a byte address.
    // One segment unit is 16 bytes, so base = segment * 16.
    const u64 extended_bios_data_area_base = cast(u64)extended_bios_data_area_segment << 4;
    if (extended_bios_data_area_base != 0) {
        const auto* rsdp = find_rsdp_in_range(extended_bios_data_area_base, extended_bios_data_area_base + 1024);
        if (rsdp != nullptr) return rsdp;
    }
    return find_rsdp_in_range(0xE0000, 0x100000);
}

auto find_rsdp(const boot::Multiboot2_Info* mbi) -> const RSDP* {
    const auto* new_tag = boot::find_multiboot2_tag<boot::Multiboot2_ACPI_New_RSDP_Tag>(mbi);
    if (new_tag != nullptr) {
        const auto* rsdp = new_tag->tag.payload_as<RSDP>();
        if (rsdp_valid(rsdp)) {
            serial::println(
                "ACPI RSDP from Multiboot2 new tag rev=% rsdt=% xsdt=%",
                rsdp->revision,
                rsdp->rsdt_address,
                rsdp->xsdt_address
            );
            return rsdp;
        }
    }

    const auto* old_tag = boot::find_multiboot2_tag<boot::Multiboot2_ACPI_Old_RSDP_Tag>(mbi);
    if (old_tag != nullptr) {
        const auto* rsdp = old_tag->tag.payload_as<RSDP>();
        if (rsdp_valid(rsdp)) {
            serial::println(
                "ACPI RSDP from Multiboot2 old tag rev=% rsdt=%",
                rsdp->revision,
                rsdp->rsdt_address
            );
            return rsdp;
        }
    }

    const auto* scanned = find_rsdp_bios_scan();
    if (scanned != nullptr) {
        serial::println(
            "ACPI RSDP from BIOS scan rev=% rsdt=% xsdt=%",
            scanned->revision,
            scanned->rsdt_address,
            scanned->revision >= 2 ? scanned->xsdt_address : 0
        );
    }
    return scanned;
}

auto sdt_valid(const SDT_Header* header) -> bool {
    if (header == nullptr) return false;
    if (header->length < size_of(SDT_Header)) return false;
    return checksum_ok(header, header->length);
}

auto find_madt_in_rsdt(const SDT_Header* rsdt) -> const MADT* {
    if (!sdt_valid(rsdt)) return nullptr;
    if (!kstd_memeq(rsdt->signature, RSDT_SIGNATURE)) return nullptr;

    const s64 entry_bytes = rsdt->length - size_of(SDT_Header);
    const s64 entry_count = entry_bytes / size_of(u32);
    const auto* entries = addr_as<const u32*>(ptr_addr(rsdt) + size_of(SDT_Header));

    for (s64 i = 0; i < entry_count; ++i) {
        const auto* header = addr_as<const SDT_Header*>(entries[i]);
        if (!sdt_valid(header)) continue;
        if (kstd_memeq(header->signature, MADT_SIGNATURE)) {
            return cast(const MADT*)header;
        }
    }
    return nullptr;
}

auto find_madt_in_xsdt(const SDT_Header* xsdt) -> const MADT* {
    if (!sdt_valid(xsdt)) return nullptr;
    if (!kstd_memeq(xsdt->signature, XSDT_SIGNATURE)) return nullptr;

    const s64 entry_bytes = xsdt->length - size_of(SDT_Header);
    const s64 entry_count = entry_bytes / size_of(u64);
    const auto* entries = addr_as<const u64*>(ptr_addr(xsdt) + size_of(SDT_Header));

    for (s64 i = 0; i < entry_count; ++i) {
        const auto* header = addr_as<const SDT_Header*>(entries[i]);
        if (!sdt_valid(header)) continue;
        if (kstd_memeq(header->signature, MADT_SIGNATURE)) {
            return cast(const MADT*)header;
        }
    }
    return nullptr;
}

auto find_madt(const RSDP* rsdp) -> const MADT* {
    if (rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
        const auto* xsdt = addr_as<const SDT_Header*>(rsdp->xsdt_address);
        const auto* table = find_madt_in_xsdt(xsdt);
        if (table != nullptr) return table;
    }

    if (rsdp->rsdt_address != 0) {
        const auto* rsdt = addr_as<const SDT_Header*>(rsdp->rsdt_address);
        return find_madt_in_rsdt(rsdt);
    }

    return nullptr;
}

auto push_cpu(MADT_Info& info, u32 apic_id, u32 acpi_id, bool enabled) -> void {
    if (info.cpus.size >= cast(s64)MAX_CPUS) {
        serial::println("MADT: cpu list full, dropping apic_id=%", apic_id);
        return;
    }

    info.cpus.push_back({
        .apic_id           = apic_id,
        .acpi_processor_id = acpi_id,
        .enabled           = enabled,
        .is_bsp            = (apic_id == info.bsp_apic_id),
    });
}

auto parse_madt_entries(const MADT* table, MADT_Info& info) -> void {
    const auto* bytes     = cast(const u8*)table;
    const auto* entry     = bytes + size_of(MADT);
    const auto* table_end = bytes + table->header.length;

    while (entry + size_of(MADT_Entry_Header) <= table_end) {
        const auto* header = cast(const MADT_Entry_Header*)entry;
        const u8    length = header->length;

        if (length < size_of(MADT_Entry_Header)) {
            serial::println("MADT: corrupt entry length < 2");
            break;
        }
        if (entry + length > table_end) {
            serial::println("MADT: entry overruns table");
            break;
        }

        using enum MADT_Entry_Type;
        switch (cast(MADT_Entry_Type)header->type) {
            case LOCAL_APIC: {
                if (length < size_of(MADT_Local_APIC)) break;
                const auto* e = cast(const MADT_Local_APIC*)entry;
                push_cpu(info, e->apic_id, e->acpi_processor_uid, e->flags.enabled);
            } break;

            case IOAPIC: {
                if (length < size_of(MADT_IOAPIC)) break;

                const auto* e = cast(const MADT_IOAPIC*)entry;
                info.ioapics.push_back({
                    .id        = e->ioapic_id,
                    .gsi_base  = e->gsi_base,
                    .mmio_phys = e->address,
                });
            } break;

            case ISO: {
                if (length < size_of(MADT_ISO)) break;

                const auto* e = cast(const MADT_ISO*)entry;
                info.overrides.push_back({
                    .isa_irq = e->source,
                    .gsi     = e->gsi,
                    .flags   = e->flags,
                });
            } break;

            case LOCAL_X2APIC: {
                if (length < size_of(MADT_Local_X2APIC)) break;
                const auto* e = cast(const MADT_Local_X2APIC*)entry;
                push_cpu(info, e->x2apic_id, e->acpi_processor_uid, e->flags.enabled);
            } break;
        }

        entry += length;
    }
}

auto print_madt(const MADT_Info& info) -> void {
    serial::println(
        "MADT lapic_addr=% pcat_compat=% cpus=% ioapics=% overrides=%",
        info.local_apic_address,
        info.flags.pcat_compat,
        info.cpus.size,
        info.ioapics.size,
        info.overrides.size
    );

    for (s64 i = 0; i < info.cpus.size; ++i) {
        const auto& cpu = info.cpus[i];
        serial::println(
            "MADT cpu[%] -> apic_id=% acpi_id=% enabled=% bsp=%",
            i,
            cpu.apic_id,
            cpu.acpi_processor_id,
            cpu.enabled,
            cpu.is_bsp
        );
    }

    for (s64 i = 0; i < info.ioapics.size; ++i) {
        const auto& io = info.ioapics[i];
        serial::println(
            "MADT ioapic[%] -> id=% mmio=% gsi_base=%",
            i,
            io.id,
            io.mmio_phys,
            io.gsi_base
        );
    }

    for (s64 i = 0; i < info.overrides.size; ++i) {
        const auto& o = info.overrides[i];
        serial::println(
            "MADT override[%] -> isa_irq=% gsi=% polarity=% trigger=%",
            i,
            o.isa_irq,
            o.gsi,
            o.flags.polarity,
            o.flags.trigger
        );
    }
}

// Call after mem::initialize (identity map must cover ACPI tables).
// Uses CPUID initial_apic_id to mark the BSP.
auto parse_madt(const boot::Multiboot2_Info* mbi) -> bool {
    using namespace hidden;

    madt_info = {};
    madt_info.bsp_apic_id = cpu::features().initial_apic_id;

    const auto* rsdp = find_rsdp(mbi);
    kstd_assert(rsdp != nullptr, "ACPI RSDP not found");

    const auto* table = find_madt(rsdp);
    kstd_assert(table != nullptr, "ACPI MADT not found");
    kstd_assert(sdt_valid(&table->header), "ACPI MADT checksum bad");
    kstd_assert(kstd_memeq(table->header.signature, MADT_SIGNATURE), "ACPI MADT signature bad");

    serial::println("ACPI MADT at % length=%", ptr_addr(table), table->header.length);

    madt_info.local_apic_address = table->local_apic_address;
    madt_info.flags              = table->flags;
    parse_madt_entries(table, madt_info);
    madt_info.valid = true;

    print_madt(madt_info);

    kstd_assert(madt_info.cpus.size    >= 1, "MADT listed no CPUs");
    kstd_assert(madt_info.ioapics.size >= 1, "MADT listed no IOAPIC");

    return true;
}

} // namespace acpi
