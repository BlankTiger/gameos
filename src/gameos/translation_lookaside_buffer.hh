#pragma once

#include "kstd/basic.hh"

#include "gameos/advanced_configuration_and_power_interface.hh"
#include "gameos/application_processor_state.hh"
#include "gameos/cpu_local.hh"
#include "gameos/interrupts_constants.hh"
#include "gameos/local_apic.hh"
#include "gameos/translation_lookaside_buffer_state.hh"

namespace tlb {

// @TODO(blanktiger): Maybe hide those, cause they aren't behind a lock.
force_inline auto flush_local(psize address) -> void {
    if (address == FLUSH_ALL) {
        u64 cr3;
        asm volatile(
            "mov %%cr3, %0\n"
            "mov %0, %%cr3"
            : "=&r"(cr3)
            :
            : "memory"
        );
    } else {
        asm volatile("invlpg (%0)" : : "r"(address) : "memory");
    }
}

auto shootdown(psize address) -> void {
    u32 online   = ap::online_count();
    u32 required = ap::online_count_excluding_self();

    serial::println("TLB shootdown online=% required=% self=%", online, required, cpu_local::current().cpu_index);

    if (online <= 1) {
        flush_local(address);
        return;
    }

    auto guard = state.lock.scoped_irq_lock();

    if (required == 0) {
        flush_local(address);
        return;
    }

    state.acknowledgments.store(0, std::memory_order_relaxed);
    state.required_acknowledgments.store(required, std::memory_order_relaxed);
    state.virtual_address.store(address, std::memory_order_release);

    asm volatile("mfence" ::: "memory");

    static constexpr lapic::Interrupt_Command_Register_Low SHOOTDOWN_COMMAND = {
        .vector                = VECTOR_LOCAL_APIC_TLB_SHOOTDOWN,
        .delivery_mode         = lapic::Delivery_Mode::FIXED,
        .destination_mode      = lapic::Destination_Mode::PHYSICAL,
        .delivery_pending      = 0,
        .reserved_bit_13       = 0,
        .level                 = lapic::Level::ASSERT,
        .trigger_mode          = lapic::Trigger_Mode::EDGE,
        .reserved_mid          = 0,
        .destination_shorthand = lapic::Destination_Shorthand::NONE,
        .reserved_high         = 0,
    };

    const auto self = cpu_local::current().cpu_index;
    for (u32 index = 0; index < acpi::MAX_CPUS; ++index) {
        if (index == self || !ap::cpus_online[index].load(std::memory_order_acquire)) continue;

        lapic::send_inter_processor_interrupt(cpu_local::core_infos[index].lapic_id, SHOOTDOWN_COMMAND);
    }

    flush_local(address);

    while (state.acknowledgments.load(std::memory_order_acquire) < state.required_acknowledgments.load(std::memory_order_acquire)) {
        asm volatile("pause");
    }
}

force_inline auto flush_all() -> void {
    shootdown(FLUSH_ALL);
}


}
