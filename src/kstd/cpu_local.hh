#pragma once

#include <cstddef>

#include "advanced_configuration_and_power_interface.hh"
#include "basic.hh"
#include "assert.hh"
#include "array.hh"
#include "local_apic.hh"
#include "low_level_io.hh"
#include "serial_format.hh"
#include "smp_constants.hh"

//
// Stores information specific to a unique hardware CPU core.
//
// The way this works is that each logical core has its own G segment where
// you can write your own pointer to core specific stuff. We do that with
// `wrmsr` / `rdmsr` instructions.
//
// For now every core still shares the same global allocator.
//
namespace cpu_local {

struct Core_Info {
    Core_Info* self;
    u32        cpu_index;
    u32        lapic_id;
    u8*        kernel_stack_top;
    u64        lapic_ticks;
    alignas(16) u8 fpu_irq_save[512];
};

// Numeric #define so ISR asm strings can stringify it. Keep in sync with layout.
#define CPU_LOCAL_FPU_IRQ_SAVE_OFFSET 32
#define CPU_LOCAL_ASM_STR_HELPER(x) #x
#define CPU_LOCAL_ASM_STR(x) CPU_LOCAL_ASM_STR_HELPER(x)
static_assert(offsetof(Core_Info, fpu_irq_save) == CPU_LOCAL_FPU_IRQ_SAVE_OFFSET);

force_inline auto current() -> Core_Info& {
    Core_Info* self;
    asm volatile("movq %%gs:0, %0" : "=r"(self));
    return *self;
}

force_inline auto set_g_segment_base(const Core_Info* core_info) -> void {
    low_level_io::write_model_specific_register(IA32_GS_BASE, ptr_addr(core_info));
}

force_inline auto get_g_segment_base() -> Core_Info* {
    auto core_info_addr = low_level_io::read_model_specific_register(IA32_GS_BASE);
    auto* core_info = addr_as<Core_Info*>(core_info_addr);
    return core_info;
}

alignas(16) inline Static_Array<Core_Info, acpi::MAX_CPUS> core_infos;

namespace hidden {
    inline bool bsp_core_info_initialized = false;
}

// Must be called on the logical core that is being set up.
auto initialize_application_processor(u32 index, u8* kernel_stack_top) -> void {
    if (hidden::bsp_core_info_initialized)
        kstd_assert(index > 0);
    kstd_assert(kernel_stack_top != nullptr);

    serial::println("initialize_application_processor for AP index=%", index);

    core_infos[index].self             = &core_infos[index];
    core_infos[index].cpu_index        = index;
    core_infos[index].lapic_id         = index == 0 ? lapic::bootstrap_processor_apic_id() : lapic::local_apic_id();
    core_infos[index].kernel_stack_top = kernel_stack_top;
    core_infos[index].lapic_ticks      = 0;

    set_g_segment_base(&core_infos[index]);

    kstd_assert(get_g_segment_base() == &core_infos[index]);
    kstd_assert(&current()           == &core_infos[index]);
}

extern "C" u8 stack_top[];

force_inline auto initialize_bootstrap_processor() -> void {
    // By convention takes the first spot in `core_infos`.
    initialize_application_processor(0, stack_top);
    hidden::bsp_core_info_initialized = true;
}

}
