#pragma once

#include "low_level_io.hh"
#include "term.hh"

// PS/2 controller (Intel 8042) initialization.
// Must be called after pic::initialize() and before idt::enable_interrupts().
namespace ps2 {

using namespace low_level_io;

// Controller commands written to PS2_CMD_PORT.
enum class Cmd : u8 {
    read_config        = 0x20,
    write_config       = 0x60,
    enable_second_port = 0xA8,
    write_to_mouse     = 0xD4,
};

enum class Mouse_Cmd : u8 {
    // Start sending movement/button packets.
    enable_reporting = 0xF4,
};

// ACK byte returned by devices after a command.
constexpr u8 ACK = 0xFA;

// Read from PS2_STATUS_PORT.
union Controller_Status_Register {
    struct {
        u8 output_full   : 1;  // 1 = output buffer has data to read
        u8 input_full    : 1;  // 1 = input buffer busy, don't write yet
        u8 system_flag   : 1;  // POST pass/fail result
        u8 cmd_data      : 1;  // 0 = data written to port 0x60, 1 = command to 0x64
        u8 _reserved     : 2;
        u8 timeout_error : 1;
        u8 parity_error  : 1;
    };
    u8 raw;
} __attribute__((packed));

// Read/written via read_config/write_config commands.
union Controller_Config {
    struct {
        u8 port1_irq_enable  : 1;  // 1 = enable IRQ1 for port 1 (keyboard)
        u8 port2_irq_enable  : 1;  // 1 = enable IRQ12 for port 2 (mouse)
        u8 system_flag       : 1;  // must be set (POST passed)
        u8 port1_clock       : 1;  // 0 = port 1 clock enabled
        u8 port2_clock       : 1;  // 0 = port 2 clock enabled
        u8 port1_translation : 1;  // 1 = translate port 1 scancodes to set 1
        u8 _reserved         : 2;
    };
    u8 raw;
} __attribute__((packed));

// Spin-wait limit for PS/2 I/O operations.  Each iteration reads one I/O port
// which takes ~1 µs on real hardware, so 100 000 iterations ≈ 100 ms - enough
// for any real device while avoiding an infinite hang on systems that have no
// PS/2 controller.
constexpr u32 PS2_TIMEOUT_ITERS = 100'000;

// Returns true if the input buffer became ready, false on timeout.
auto wait_input_ready() -> bool {
    Controller_Status_Register s;
    for (u32 i = 0; i < PS2_TIMEOUT_ITERS; ++i) {
        s.raw = inb(PS2_STATUS_PORT);
        if (!s.input_full) return true;
    }
    return false;
}

// Returns true if the output buffer has data ready to read, false on timeout.
auto wait_output_ready() -> bool {
    Controller_Status_Register s;
    for (u32 i = 0; i < PS2_TIMEOUT_ITERS; ++i) {
        s.raw = inb(PS2_STATUS_PORT);
        if (s.output_full) return true;
    }
    return false;
}

// Returns false if the controller did not become ready in time.
auto send_cmd(Cmd cmd) -> bool {
    if (!wait_input_ready()) return false;
    outb(PS2_CMD_PORT, static_cast<u8>(cmd));
    return true;
}

// Returns false if the controller did not become ready in time.
auto send_data(u8 data) -> bool {
    if (!wait_input_ready()) return false;
    outb(PS2_DATA_PORT, data);
    return true;
}

// Returns the byte read, or 0xFF on timeout (0xFF is an invalid ACK value).
auto read_data() -> u8 {
    if (!wait_output_ready()) return 0xFF;
    return inb(PS2_DATA_PORT);
}

auto isr_handle_ps2_keyboard() -> void {
    u8 scancode = inb(PS2_DATA_PORT);
    term::println("Keyboard interrupt, scancode: %", scancode);
}

auto isr_handle_ps2_mouse() -> void {
    u8 scancode = inb(PS2_DATA_PORT);
    term::println("Mouse interrupt, scancode: %", scancode);
}

auto initialize() -> void {
    // 1. Enable the second PS/2 port (mouse) - disabled by default on reset.
    //    Any step that times out means there is no functional PS/2 controller;
    //    bail out early so we don't hang.
    if (!send_cmd(Cmd::enable_second_port)) return;

    // 2. Read the current controller configuration byte.
    if (!send_cmd(Cmd::read_config)) return;
    Controller_Config config;
    config.raw = read_data();
    if (config.raw == 0xFF) return;  // timeout reading config

    // 3. Enable IRQ1 (keyboard) and IRQ12 (mouse).
    config.port1_irq_enable = 1;
    config.port2_irq_enable = 1;
    if (!send_cmd(Cmd::write_config)) return;
    if (!send_data(config.raw)) return;

    // 4. Tell the mouse to start sending movement/click packets.
    if (!send_cmd(Cmd::write_to_mouse)) return;
    if (!send_data(static_cast<u8>(Mouse_Cmd::enable_reporting))) return;
    read_data();  // consume ACK (ignore timeout - mouse may not be present)
}

}
