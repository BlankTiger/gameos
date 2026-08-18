#pragma once

#include "kstd/assert.hh"

#include "gameos/interrupts.hh"
#include "gameos/local_apic.hh"
#include "gameos/advanced_configuration_and_power_interface.hh"
#include "gameos/programmable_interrupt_controller.hh"
#include "gameos/serial_format.hh"

//
// IOAPIC is what redirects the IO interrupts to the chosen logical core.
// We configure it to redirect them to the LAPIC of the bootstrap processor.
//
// Communication happens via memory mapped IO.
//
namespace ioapic {

constexpr u32 IO_REGISTER_SELECT              = 0x00;
constexpr u32 IO_WINDOW                       = 0x10;  // Where to read / write the selected register.
constexpr u32 IO_REDIRECTION_ENTRY_TABLE_BASE = 0x10;  // Base used for calculating the correct entry address.

enum struct Register_Index : u32 {
    ID      = 0x00,
    VERSION = 0x01,
};

union Identification_Register {
    static constexpr auto INDEX = Register_Index::ID;

    struct {
        u32 reserved_low : 24;
        u32 ioapic_id    : 8;   // bits 31:24
    };
    u32 raw;
} __attribute__((packed));

union Version_Register {
    static constexpr auto INDEX = Register_Index::VERSION;

    struct {
        u32 version               : 8;  // bits 7:0
        u32 reserved_low          : 8;
        u32 max_redirection_entry : 8;  // bits 23:16 = highest pin index (count-1)
        u32 reserved_high         : 8;
    };
    u32 raw;
} __attribute__((packed));

enum struct Delivery_Mode : u32 {
    FIXED           = 0b000,
    LOWEST_PRIORITY = 0b001,
    SMI             = 0b010,
    NMI             = 0b100,
    INIT            = 0b101,
    EXTINT          = 0b111,
};

enum struct Destination_Mode : u32 {
    PHYSICAL = 0,
    LOGICAL  = 1,
};

enum struct Polarity : u32 {
    HIGH = 0,
    LOW  = 1,
};

enum struct Trigger_Mode : u32 {
    EDGE  = 0,
    LEVEL = 1,
};

// RTE low: index 0x10 + 2*pin
union Redirection_Entry_Low {
    struct {
        u32              vector           : 8;
        Delivery_Mode    delivery_mode    : 3;
        Destination_Mode destination_mode : 1;
        u32              delivery_status  : 1; // RO
        Polarity         polarity         : 1;
        u32              remote_irr       : 1; // RO
        Trigger_Mode     trigger_mode     : 1;
        u32              masked           : 1;
        u32              reserved         : 15;
    };
    u32 raw;
} __attribute__((packed));

// RTE high: index 0x11 + 2*pin
union Redirection_Entry_High {
    struct {
        u32 reserved            : 24;
        u32 destination_apic_id : 8; // bits 63:56 of full 64-bit RTE
    };
    u32 raw;
} __attribute__((packed));

struct Redirection_Entry {
    Redirection_Entry_Low  low;
    Redirection_Entry_High high;
};

force_inline auto ioapic_info() -> acpi::IOAPIC_Desc {
    const auto& madt = acpi::madt();
    kstd_assert(madt.ioapics.size >= 1);
    return madt.ioapics[0];
}

namespace hidden {
    volatile u32* mmio;
    // false: legacy 8259 still owns ISA IRQs (PIT calibrate path).
    // true:  IOAPIC -> LAPIC owns them (BSP or AP destination).
    inline bool device_irqs_use_ioapic = false;
}

force_inline auto device_irqs_via_ioapic() -> bool {
    return hidden::device_irqs_use_ioapic;
}

auto write_redirection_entry(u32 pin, Redirection_Entry entry) -> void {
    using namespace hidden;

    // High first.
    *ptr_offset(mmio, IO_REGISTER_SELECT) = IO_REDIRECTION_ENTRY_TABLE_BASE + 2 * pin + 1;
    *ptr_offset(mmio, IO_WINDOW) = entry.high.raw;

    *ptr_offset(mmio, IO_REGISTER_SELECT) = IO_REDIRECTION_ENTRY_TABLE_BASE + 2 * pin;
    *ptr_offset(mmio, IO_WINDOW) = entry.low.raw;
}

auto read_redirection_entry(u32 pin) -> Redirection_Entry {
    using namespace hidden;

    *ptr_offset(mmio, IO_REGISTER_SELECT) = IO_REDIRECTION_ENTRY_TABLE_BASE + 2 * pin + 1;
    Redirection_Entry_High high{ .raw = *ptr_offset(mmio, IO_WINDOW) };

    *ptr_offset(mmio, IO_REGISTER_SELECT) = IO_REDIRECTION_ENTRY_TABLE_BASE + 2 * pin;
    Redirection_Entry_Low low{ .raw = *ptr_offset(mmio, IO_WINDOW) };

    Redirection_Entry e{ .low = low, .high = high };
    return e;
}

template <typename Register>
auto read_register() -> Register {
    static_assert(sizeof(Register) == sizeof(u32));

    using namespace hidden;

    *ptr_offset(mmio, IO_REGISTER_SELECT) = static_cast<u32>(Register::INDEX);
    auto value = *ptr_offset(mmio, IO_WINDOW);
    Register r{ .raw = value };
    return r;
}

auto mask_all_pins(u32 max_redirection_entry) -> void {
    for (u32 pin = 0; pin <= max_redirection_entry; ++pin) {
        auto entry = read_redirection_entry(pin);
        entry.low.masked = 1;
        write_redirection_entry(pin, entry);
    }
}

using pic::ISA_Irq;
using idt::Interrupt_Vector_Type;

auto route_industry_standard_architecture_irq(ISA_Irq isa_irq, Interrupt_Vector_Type vector, u32 destination_apic_id) -> void {
    const auto& madt = acpi::madt();
    const auto info = ioapic_info();

    auto gsi = static_cast<u32>(isa_irq);
    acpi::MPS_INTI_Flags flags{};

    for (const auto& ov : madt.overrides) {
        if (ov.isa_irq == gsi) {
            gsi   = ov.gsi;
            flags = ov.flags;
            break;
        }
    }

    const auto pin = gsi - info.gsi_base;
    auto entry = read_redirection_entry(pin);

    entry.high.destination_apic_id = destination_apic_id;

    entry.low.vector           = static_cast<u32>(vector);
    entry.low.masked           = 0;
    entry.low.delivery_mode    = Delivery_Mode::FIXED;
    entry.low.destination_mode = Destination_Mode::PHYSICAL;

    // ISA bus defaults.
    auto pol = Polarity::HIGH;
    auto trg = Trigger_Mode::EDGE;

    switch (flags.polarity) {
        case acpi::Polarity::ACTIVE_HIGH: pol = Polarity::HIGH; break;
        case acpi::Polarity::ACTIVE_LOW:  pol = Polarity::LOW;  break;
        default: break;
    }

    switch (flags.trigger) {
        case acpi::Trigger::EDGE:  trg = Trigger_Mode::EDGE;  break;
        case acpi::Trigger::LEVEL: trg = Trigger_Mode::LEVEL; break;
        default: break;
    }

    entry.low.polarity     = pol;
    entry.low.trigger_mode = trg;

    write_redirection_entry(pin, entry);
}


auto initialize() -> void {
    const auto& madt = acpi::madt();
    kstd_assert(madt.valid && madt.ioapics.size >= 1);

    const auto info = ioapic_info();
    hidden::mmio = addr_as<volatile u32*>(info.mmio_phys);

    auto id      = read_register<Identification_Register>();
    auto version = read_register<Version_Register>();
    serial::println(
        "IOAPIC ioapic_id=%, version=%, max RTE=%",
        static_cast<u32>(id.ioapic_id),
        static_cast<u32>(version.version),
        static_cast<u32>(version.max_redirection_entry)
    );

    mask_all_pins(version.max_redirection_entry);

    hidden::device_irqs_use_ioapic = true;

    route_industry_standard_architecture_irq(ISA_Irq::PS2_KEYBOARD, Interrupt_Vector_Type::PS2_KEYBOARD, lapic::bootstrap_processor_apic_id());
    route_industry_standard_architecture_irq(ISA_Irq::PS2_MOUSE,    Interrupt_Vector_Type::PS2_MOUSE,    lapic::bootstrap_processor_apic_id());
}

auto route_device_irqs_to_application_processor() -> void {
    for (u32 cpu_index = 1; cpu_index < acpi::MAX_CPUS; ++cpu_index) {
        if (!ap::cpus_online[cpu_index].load(std::memory_order_acquire)) continue;

        const u32 destination_apic_id = cpu_local::core_infos[cpu_index].lapic_id;
        route_industry_standard_architecture_irq(ISA_Irq::PS2_KEYBOARD, Interrupt_Vector_Type::PS2_KEYBOARD, destination_apic_id);
        route_industry_standard_architecture_irq(ISA_Irq::PS2_MOUSE,    Interrupt_Vector_Type::PS2_MOUSE,    destination_apic_id);
        serial::println("Routing device IRQs to AP index=%, apic_id=%", cpu_index, destination_apic_id);
        return;
    }
}

}
