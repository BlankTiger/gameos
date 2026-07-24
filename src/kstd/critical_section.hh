#pragma once

#include "basic.hh"

//
// This lives separately from kstd/interrupts.hh so that other modules can
// protect their critical sections without pulling in the whole interrupt
// subsystem.
//
namespace critical_section {

force_inline auto interrupts_enabled() -> bool {
    u32 flags;
    asm volatile("pushfl\n\tpopl %0" : "=r"(flags));
    return (flags & (1u << 9)) != 0;  // EFLAGS.IF
}

force_inline auto disable_interrupts() -> void {
    asm volatile("cli");
}

force_inline auto enable_interrupts() -> void {
    asm volatile("sti");
}

struct Guard {
    bool was_enabled;

    Guard() : was_enabled(interrupts_enabled()) {
        disable_interrupts();
    }

    ~Guard() {
        if (was_enabled) enable_interrupts();
    }

    Guard(const Guard&) = delete;
    auto operator=(const Guard&) -> Guard& = delete;
};

}  // namespace critical_section
