#pragma once

#include "low_level_io.hh"

namespace power {

constexpr u16 QEMU_SHUTDOWN_PORT  = 0x604;
constexpr u16 BOCHS_SHUTDOWN_PORT = 0xB004;
constexpr u16 SHUTDOWN_COMMAND    = 0x2000;

[[noreturn]] auto off() -> void {
    asm volatile("cli" ::: "memory");

    // Supported by QEMU and Bochs. Only first write normally takes effect.
    low_level_io::outw(QEMU_SHUTDOWN_PORT, SHUTDOWN_COMMAND);
    low_level_io::outw(BOCHS_SHUTDOWN_PORT, SHUTDOWN_COMMAND);

    for (;;) asm volatile("hlt");
}

} // namespace power
