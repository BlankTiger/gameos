#pragma once

#include <cstddef>

#include "basic.hh"
#include "local_apic.hh"
#include "low_level_io.hh"

namespace cpu_local {

// Stores information specific to a unique hardware CPU core.
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
Core_Info a;

force_inline auto current() -> Core_Info& {
    // Probably have to get it with rdmsr %gs:0
    return a;
}

namespace hidden {
    inline Core_Info bootstrap_core_info;
}

extern "C" u8* stack_top;

auto initialize_bootstrap_processor() -> void {
    using namespace hidden;

    bootstrap_core_info.self             = &bootstrap_core_info;
    bootstrap_core_info.cpu_index        = 0;
    bootstrap_core_info.lapic_id         = lapic::bootstrap_processor_apic_id();
    bootstrap_core_info.kernel_stack_top = stack_top;
    bootstrap_core_info.lapic_ticks      = 0;

    low_level_io::write_model_specific_register(IA32_GS_BASE, ptr_addr(&bootstrap_core_info));
    auto hopefully_bootstrap_core_info_addr = low_level_io::read_model_specific_register(IA32_GS_BASE);
    auto* hopefully_bootstrap_core_info = addr_as<Core_Info>(hopefully_bootstrap_core_info_addr);
    kstd_assert(hopefully_bootstrap_core_info == &bootstrap_core_info);
}

}
