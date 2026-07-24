#pragma once

#include "basic.hh"

// Wrappers for communicating via I/O ports.
namespace low_level_io {

// PIC1
constexpr u16 PIC_CMD_PORT_MASTER  = 0x20;
constexpr u16 PIC_DATA_PORT_MASTER = 0x21;

// PIC2
constexpr u16 PIC_CMD_PORT_SLAVE  = 0xA0;
constexpr u16 PIC_DATA_PORT_SLAVE = 0xA1;

constexpr u16 PS2_DATA_PORT   = 0x60;  // read: data, write: send to keyboard/mouse
constexpr u16 PS2_STATUS_PORT = 0x64;  // read: controller status
constexpr u16 PS2_CMD_PORT    = 0x64;  // write: controller command

// time
constexpr u16 PIT_CHANNEL0_DATA_PORT = 0x40;
constexpr u16 PIT_CMD_REGISTER       = 0x43;

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
