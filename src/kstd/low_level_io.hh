#pragma once

// Wrappers for communicating via I/O ports.
namespace low_level_io {

force_inline auto outb(u16 port, u8 value) -> void {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

force_inline auto inb(u16 port) -> u8 {
    u8 value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

force_inline auto wait() -> void {
    constexpr auto UNUSED_PORT = 0x80;
    outb(UNUSED_PORT, 0); // Write to an unused port (takes time which gives the chip time to react).
}

force_inline auto outb_with_delay(u16 port, u8 value) -> void {
    outb(port, value);
    wait();
}

}
