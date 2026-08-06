#pragma once

#include <cstddef>

#include "advanced_configuration_and_power_interface.hh"
#include "basic.hh"
#include "assert.hh"
#include "local_apic.hh"
#include "low_level_io.hh"

//
// Stores information specific to a unique hardware CPU core.
//
// The way this works is that each logical core has its own G segment where
// you can write your own pointer to core specific stuff. We do that with
// `wrmsr` / `rdmsr` instructions.
//
// For now every core still shares the same global allocator.
// @TODO(blanktiger): Make sure it receives a lock as soon as possible.
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

constexpr u32 IA32_GS_BASE = 0xC0000101;

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

namespace hidden {
    alignas(16) inline Core_Info core_infos[acpi::MAX_CPUS];
}

// Must be called on the logical core that is being set up.
auto initialize_application_processor(u32 index, u8* kernel_stack_top) -> void {
    kstd_assert(index > 0 && index < acpi::MAX_CPUS);
    kstd_assert(kernel_stack_top != nullptr);

    using namespace hidden;

    core_infos[index].self             = &core_infos[index];
    core_infos[index].cpu_index        = index;
    core_infos[index].lapic_id         = lapic::local_apic_id();
    core_infos[index].kernel_stack_top = kernel_stack_top;
    core_infos[index].lapic_ticks      = 0;

    set_g_segment_base(&core_infos[index]);

    kstd_assert(get_g_segment_base() == &core_infos[index]);
    kstd_assert(&current()           == &core_infos[index]);
}

extern "C" u8 stack_top[];

auto initialize_bootstrap_processor() -> void {
    using namespace hidden;

    // By convention takes the first spot in `hidden::core_infos`.
    core_infos[0].self             = &core_infos[0];
    core_infos[0].cpu_index        = 0;
    core_infos[0].lapic_id         = lapic::bootstrap_processor_apic_id();
    core_infos[0].kernel_stack_top = stack_top;
    core_infos[0].lapic_ticks      = 0;

    set_g_segment_base(&core_infos[0]);

    kstd_assert(get_g_segment_base() == &core_infos[0]);
    kstd_assert(&current()           == &core_infos[0]);
}

}
