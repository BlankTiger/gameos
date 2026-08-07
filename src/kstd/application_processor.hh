#pragma once

#include "advanced_configuration_and_power_interface.hh"
#include "assert.hh"
#include "basic.hh"
#include "cstring.hh"
#include "global_descriptors.hh"
#include "local_apic.hh"
#include "serial_format.hh"
#include "config.hh"
#include "time.hh"
#include "pointer_utils.hh"
#include "string_builder.hh"

extern "C" auto ap_main() -> void {

}

extern "C" {
    extern u8 smp_trampoline_start[];
    extern u8 smp_trampoline_end[];
    extern const u64 smp_trampoline_size;
}

namespace ap {

enum struct Breadcrumb_Stage : u16 {
    SMP_ENTRY = SMP_BREADCRUMB_ENTRY,
    SMP_GDTR  = SMP_BREADCRUMB_GDTR,
    SMP_PE    = SMP_BREADCRUMB_PE,
};
@enum_to_string(Breadcrumb_Stage);

auto set_all_breadcrumbs_to_zero() -> void {
    for (auto stage : @enum_values(Breadcrumb_Stage)) {
        *reinterpret_cast<volatile u8*>(TRAMPOLINE_PHYSICAL_ADDRESS + static_cast<u16>(stage)) = 0;
    }
}

force_inline auto assert_breadcrumb_okay(Breadcrumb_Stage stage) -> void {
    auto value = *reinterpret_cast<volatile u8*>(TRAMPOLINE_PHYSICAL_ADDRESS + static_cast<u16>(stage));
    serial::println("Checking stage: %", stage);
    kstd_assert(value == 67, ctprint("Stage %: expected 67, got %", stage, value));
}

auto copy_trampoline_code_to_the_expected_place() -> void {
    kstd_assert(smp_trampoline_size > 0);

    auto* destination = reinterpret_cast<u8*>(TRAMPOLINE_PHYSICAL_ADDRESS);
    auto* source      = reinterpret_cast<u8*>(smp_trampoline_start);
    kstd_memcpy(destination, source, smp_trampoline_size);
    serial::println("Copied smp_trampoline code (% bytes) from % to %", smp_trampoline_size, source, destination);
}

struct Temp_Descriptor_Table {
    gdt::Segment_Descriptor null{};

    gdt::Segment_Descriptor code32{
        .limit_low    = 0xFFFF,
        .base_low     = 0,
        .base_mid     = 0,
        .accessed     = 0,
        .rw           = 1,
        .dc           = 0,
        .executable   = gdt::Executable::CODE,
        .code_or_data = gdt::Code_Or_Data::CODE_OR_DATA,
        .dpl          = gdt::Descriptor_Privilege_Level::RING0,
        .present      = 1,
        .limit_high   = 0xF,
        .available    = 0,
        .long_mode    = 0,
        .op_size      = 1,
        .granularity  = gdt::Granularity::_4KiB,
        .base_high    = 0,
    };
    gdt::Segment_Descriptor data{
        .limit_low    = 0xFFFF,
        .base_low     = 0,
        .base_mid     = 0,
        .accessed     = 0,
        .rw           = 1,
        .dc           = 0,
        .executable   = gdt::Executable::DATA,
        .code_or_data = gdt::Code_Or_Data::CODE_OR_DATA,
        .dpl          = gdt::Descriptor_Privilege_Level::RING0,
        .present      = 1,
        .limit_high   = 0xF,
        .available    = 0,
        .long_mode    = 0,
        .op_size      = 1,
        .granularity  = gdt::Granularity::_4KiB,
        .base_high    = 0,
    };

    gdt::Segment_Descriptor null2{};

    gdt::Segment_Descriptor code64{
        .limit_low    = 0xFFFF,
        .base_low     = 0,
        .base_mid     = 0,
        .accessed     = 0,
        .rw           = 1,
        .dc           = 0,
        .executable   = gdt::Executable::CODE,
        .code_or_data = gdt::Code_Or_Data::CODE_OR_DATA,
        .dpl          = gdt::Descriptor_Privilege_Level::RING0,
        .present      = 1,
        .limit_high   = 0xF,
        .available    = 0,
        .long_mode    = 1,
        .op_size      = 0,
        .granularity  = gdt::Granularity::_4KiB,
        .base_high    = 0,
    };
};

static_assert(sizeof(Temp_Descriptor_Table) == 40);

auto copy_gdtr_to_the_expected_place() -> void {
    using namespace gdt;

    constexpr Temp_Descriptor_Table temp_gdt{};
    constexpr Global_Descriptor_Table_Register temp_gdtr{
        .limit = static_cast<u16>(sizeof(Temp_Descriptor_Table)) - 1,
        .base  = TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_GDT,
    };

    *addr_as<Global_Descriptor_Table_Register*>(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_GDTR) = temp_gdtr;
    *addr_as<Temp_Descriptor_Table*>(TRAMPOLINE_PHYSICAL_ADDRESS            + SMP_OFFSET_GDT)  = temp_gdt;
}

auto initialize_aps() -> void {
    copy_trampoline_code_to_the_expected_place();
    copy_gdtr_to_the_expected_place();

    using namespace lapic;

    constexpr u8 TRAMPOLINE_SIPI_VECTOR = TRAMPOLINE_PHYSICAL_ADDRESS >> 12;  // 0x07

    constexpr Interrupt_Command_Register_Low INIT_ASSERT = {
        .vector                = 0,
        .delivery_mode         = Delivery_Mode::INIT,
        .destination_mode      = Destination_Mode::PHYSICAL,
        .delivery_pending      = 0,
        .reserved_bit_13       = 0,
        .level                 = Level::ASSERT,
        .trigger_mode          = Trigger_Mode::LEVEL,
        .reserved_mid          = 0,
        .destination_shorthand = Destination_Shorthand::NONE,
        .reserved_high         = 0,
    };

    constexpr Interrupt_Command_Register_Low SIPI = {
        .vector                = TRAMPOLINE_SIPI_VECTOR,
        .delivery_mode         = Delivery_Mode::START_UP,
        .destination_mode      = Destination_Mode::PHYSICAL,
        .delivery_pending      = 0,
        .reserved_bit_13       = 0,
        .level                 = Level::ASSERT,
        .trigger_mode          = Trigger_Mode::EDGE,
        .reserved_mid          = 0,
        .destination_shorthand = Destination_Shorthand::NONE,
        .reserved_high         = 0,
    };

    for (auto cpu : acpi::madt().cpus) {
        if (cpu.is_bsp) continue;

        set_all_breadcrumbs_to_zero();
        asm volatile("mfence");

        auto apic_id = cpu.apic_id;
        serial::println("Initializing AP%", apic_id);

        // ICR INIT
        serial::println("Sending INIT_ASSERT to AP%", apic_id);
        lapic::send_inter_processor_interrupt(apic_id, INIT_ASSERT);
        ktime::sleep_ms(10);

        // ICR SIPI
        serial::println("Sending SIPI to AP%", apic_id);
        lapic::send_inter_processor_interrupt(apic_id, SIPI);
        ktime::sleep_ms(1);

        // ICR SIPI
        serial::println("Sending SIPI to AP%", apic_id);
        lapic::send_inter_processor_interrupt(apic_id, SIPI);
        ktime::sleep_ms(1);

        // Wait for AP to initialize.
        ktime::sleep_ms(10);

        for (auto stage : @enum_values(Breadcrumb_Stage))
            assert_breadcrumb_okay(stage);
    }
}

}
