#pragma once

#include <atomic>

#include "kstd/array.hh"
#include "kstd/allocator.hh"
#include "kstd/assert.hh"
#include "kstd/basic.hh"
#include "kstd/cstring.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/string_builder.hh"

#include "gameos/advanced_configuration_and_power_interface.hh"
#include "gameos/application_processor_state.hh"
#include "gameos/cpu_local.hh"
#include "gameos/global_descriptors.hh"
#include "gameos/interrupts_constants.hh"
#include "gameos/interrupts.hh"
#include "gameos/local_apic.hh"
#include "gameos/serial_format.hh"
#include "gameos/smp_constants.hh"
#include "gameos/threads.hh"
#include "gameos/time.hh"
#include "gameos/thread_local_storage.hh"

namespace ap {

// Stops all future interrupts and goes to sleep.
force_inline auto freeze_cpu() -> void {
    auto idx = cpu_local::current().cpu_index;
    cpus_frozen[idx].store(true, std::memory_order_release);
    asm volatile("" ::: "memory");
    asm volatile("cli" ::: "memory");
    for (;;) asm volatile("hlt");
}

namespace hidden {
    //
    // @SAFETY: No need for an atomic as eventually all the APs will get to read the correct
    //          value and go to sleep.
    //
    inline std::atomic<bool> stop_requested{false};
}

force_inline auto is_stop_requested() -> bool {
    return hidden::stop_requested.load(std::memory_order_acquire);
}

force_inline auto request_stop_of_all_other_aps() -> void {
    hidden::stop_requested.store(true, std::memory_order_release);
    asm volatile("" ::: "memory");  // Compiler barrier (release semantics).

    using namespace lapic;
    static constexpr Interrupt_Command_Register_Low STOP_IPI = {
        .vector                = VECTOR_LOCAL_APIC_STOP,
        .delivery_mode         = Delivery_Mode::FIXED,
        .destination_mode      = Destination_Mode::PHYSICAL,
        .delivery_pending      = 0,
        .reserved_bit_13       = 0,
        .level                 = Level::ASSERT,
        .trigger_mode          = Trigger_Mode::EDGE,
        .reserved_mid          = 0,
        .destination_shorthand = Destination_Shorthand::ALL_EXCLUDING_SELF,
        .reserved_high         = 0,
    };

    static constexpr Interrupt_Command_Register_Low NMI_BACKUP = {
        .vector                = 0,
        .delivery_mode         = Delivery_Mode::NON_MASKABLE_INTERRUPT,
        .destination_mode      = Destination_Mode::PHYSICAL,
        .delivery_pending      = 0,
        .reserved_bit_13       = 0,
        .level                 = Level::ASSERT,
        .trigger_mode          = Trigger_Mode::EDGE,
        .reserved_mid          = 0,
        .destination_shorthand = Destination_Shorthand::ALL_EXCLUDING_SELF,
        .reserved_high         = 0,
    };

    send_inter_processor_interrupt(0, STOP_IPI);
    // To be extra sure it won't get ignored.
    send_inter_processor_interrupt(0, NMI_BACKUP);
}

extern "C" u8 smp_trampoline_start[];
extern "C" u8 smp_trampoline_end[];
extern "C" u8 smp_trampoline_pm32[];
extern "C" u8 smp_trampoline_lm64[];
extern "C" const psize smp_trampoline_size;
extern "C" const psize boot_pml4_physical_address;

enum struct Breadcrumb_Stage : u16 {
    SMP_ENTRY   = SMP_BREADCRUMB_ENTRY,
    SMP_GDTR    = SMP_BREADCRUMB_GDTR,
    SMP_PE      = SMP_BREADCRUMB_PE,
    SMP_PM32    = SMP_BREADCRUMB_PM32,
    SMP_CR3     = SMP_BREADCRUMB_CR3,
    SMP_LME     = SMP_BREADCRUMB_LME,
    SMP_PG      = SMP_BREADCRUMB_PG,
    SMP_LM64    = SMP_BREADCRUMB_LM64,
    SMP_STACK   = SMP_BREADCRUMB_STACK,
    SMP_GS      = SMP_BREADCRUMB_GS,
    SMP_ENTRY64 = SMP_BREADCRUMB_ENTRY64,
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
    kstd_assert(value == SMP_BREADCRUMB_OKAY_VALUE, ctprint("Stage %: expected %, got %", stage, SMP_BREADCRUMB_OKAY_VALUE, value));
}

auto adjust_asm_label_offsets() -> void {
    {
        auto far32_addr = TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_FAR32;
        u32 pm32_phys = TRAMPOLINE_PHYSICAL_ADDRESS + (ptr_addr(smp_trampoline_pm32) - ptr_addr(smp_trampoline_start));

        *addr_as<volatile u32*>(far32_addr)     = pm32_phys;
        *addr_as<volatile u16*>(far32_addr + 4) = SMP_SEGMENT_OFFSET_IN_GDT_PM32_CODE;
        serial::println("Adjusted the 32 bit protected-mode section offset");
    }

    {
        auto far64_addr = TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_FAR64;
        u32 lm64_phys = TRAMPOLINE_PHYSICAL_ADDRESS + (ptr_addr(smp_trampoline_lm64) - ptr_addr(smp_trampoline_start));

        *addr_as<volatile u32*>(far64_addr)     = lm64_phys;
        *addr_as<volatile u16*>(far64_addr + 4) = SMP_SEGMENT_OFFSET_IN_GDT_LM64_CODE;
        serial::println("Adjusted the 64 bit protected-mode section offset");
    }
}

auto copy_trampoline_code_to_the_expected_place() -> void {
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
static_assert(offsetof(Temp_Descriptor_Table, code32) == SMP_SEGMENT_OFFSET_IN_GDT_PM32_CODE);
static_assert(offsetof(Temp_Descriptor_Table, data)   == SMP_SEGMENT_OFFSET_IN_GDT_PM32_DATA);
static_assert(offsetof(Temp_Descriptor_Table, code64) == SMP_SEGMENT_OFFSET_IN_GDT_LM64_CODE);

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

auto copy_boot_pml4_to_the_expected_place() -> void {
    *addr_as<volatile psize*>(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_CR3) = boot_pml4_physical_address;
}

constexpr auto STACK_POINTER_ADDR = TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_STACK;

auto ap_main(u32 cpu_index) -> void {
    ap::cpus_online[cpu_index].store(true, std::memory_order_release);

    // Switch away from the temporary GDT to the kernel GDT.
    gdt::load_shared();

    // Load kernel IDT (table already built by the BSP).
    idt::load();

    tls::initialize_application_processor(cpu_index, {
        .allocator = mem::buddy.get_allocator(),
        // This is set intentionally. If we hit an assert because we actually
        // want to use a temporary allocator in the APs startup then we can
        // reconsider.
        .temporary_state     = nullptr,
        .temporary_allocator = {},
        .formatting_config   = {},
    });
    serial::println("AP index=% started", cpu_index);

    auto* kernel_top = addr_as<u8*>(ap::STACK_POINTER_ADDR);
    cpu_local::initialize_application_processor(cpu_index, kernel_top);

    kstd_assert(cpu_local::current().cpu_index == cpu_index);

    lapic::initialize_application_processor();
    lapic::start_timer_periodic(ktime::TICK_RATE);

    idt::enable_interrupts();

    serial::println("AP online index=% apic_id=%", cpu_index, lapic::local_apic_id());

    for (;;) {
        if (ap::is_stop_requested()) {
            ap::freeze_cpu();
        }

        if (!threads::idle_poll()) {
            asm volatile("hlt"); // Wait for work.
        }
    }
}

auto set_ap_main_as_offset_entry() -> void {
    auto ap_main_address = ptr_addr(ap_main);
    *addr_as<volatile psize*>(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_ENTRY) = ap_main_address;
}

auto get_stack_pointer() -> u8* {
    return addr_as<u8*>(STACK_POINTER_ADDR);
}

// @TODO(blanktiger): Free this if something goes wrong later (if we decide that
// we want to continue with an AP that failed to initialize).
auto set_up_a_new_stack() -> void {
    auto stack_allocation = mem::alloc(AP_STACK_SIZE, AP_STACK_ALIGNMENT);
    kstd_assert(stack_allocation.error == mem::Allocator_Error::NONE);

    auto stack_mem = stack_allocation.memory;
    auto stack_top = ptr_addr(stack_mem) + AP_STACK_SIZE;
    *addr_as<volatile psize*>(STACK_POINTER_ADDR) = stack_top;
    serial::println("Created a % KiB stack", AP_STACK_SIZE / 1024);
}

auto set_cpu_index(u32 cpu_index) -> void {
    *addr_as<volatile u32*>(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_CPU_INDEX) = cpu_index;
}

auto copy_pointer_to_core_info(u32 apic_id) -> void {
    *addr_as<volatile psize*>(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_CORE_INFO) = ptr_addr(&cpu_local::core_infos[apic_id]);
}

auto initialize_aps() -> void {
    for (u32 index = 0; index < acpi::MAX_CPUS; ++index) {
        cpus_online[index].store(false, std::memory_order_relaxed);
        cpus_frozen[index].store(false, std::memory_order_relaxed);
    }
    // Mark BSP as online.
    cpus_online[0].store(true, std::memory_order_release);

    kstd_assert(smp_trampoline_size <= 0xF00, "This must fit for real mode to work.");
    kstd_assert(smp_trampoline_size > 0);

    halt::set_pre_halt_hook(ap::request_stop_of_all_other_aps);

    copy_trampoline_code_to_the_expected_place();
    adjust_asm_label_offsets();
    copy_gdtr_to_the_expected_place();
    copy_boot_pml4_to_the_expected_place();
    set_ap_main_as_offset_entry();

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

    const auto& madt = acpi::madt();
    u32 next_cpu_index = 1;
    for (u32 cpu_index = 0; cpu_index < madt.cpus.size; ++cpu_index) {
        const auto& cpu = madt.cpus[cpu_index];
        if (!cpu.enabled || cpu.is_bsp) continue;

        defer(++next_cpu_index);

        set_all_breadcrumbs_to_zero();
        asm volatile("mfence");

        auto apic_id = cpu.apic_id;
        serial::println("Initializing AP index=%, apic_id=%", next_cpu_index, apic_id);

        set_up_a_new_stack();
        set_cpu_index(next_cpu_index);
        copy_pointer_to_core_info(next_cpu_index);

        // ICR INIT
        serial::println("Sending INIT_ASSERT");
        lapic::send_inter_processor_interrupt(apic_id, INIT_ASSERT);
        ktime::sleep_ms(10);

        // ICR SIPI
        serial::println("Sending SIPI to AP");
        lapic::send_inter_processor_interrupt(apic_id, SIPI);
        ktime::sleep_ms(1);

        // ICR SIPI
        serial::println("Sending SIPI to AP");
        lapic::send_inter_processor_interrupt(apic_id, SIPI);
        ktime::sleep_ms(1);

        // Wait for AP to initialize.
        bool reached_timeout = false;
        static constexpr auto RETRY_LIMIT = 50'000'000;
        for (u64 retry_counter = 0; retry_counter < RETRY_LIMIT; ++retry_counter) {
            if (retry_counter == RETRY_LIMIT - 1) reached_timeout = true;
            if (cpus_online[next_cpu_index].load(std::memory_order_acquire)) break;

            asm volatile("pause");
        }

        if (reached_timeout)
            serial::println("Timeout reached waiting for AP index=% to go online", next_cpu_index);

        for (auto stage : @enum_values(Breadcrumb_Stage))
            assert_breadcrumb_okay(stage);
    }
}

}
