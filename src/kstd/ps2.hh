#pragma once

#include "enum_array.hh"
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

constexpr u8 KEY_UP_RANGE_START = 128;

enum class Scancode : u8 {
    ESCAPE          = 0x01,
    DIGIT_1         = 0x02,
    DIGIT_2         = 0x03,
    DIGIT_3         = 0x04,
    DIGIT_4         = 0x05,
    DIGIT_5         = 0x06,
    DIGIT_6         = 0x07,
    DIGIT_7         = 0x08,
    DIGIT_8         = 0x09,
    DIGIT_9         = 0x0A,
    DIGIT_0         = 0x0B,
    MINUS           = 0x0C,
    EQUALS          = 0x0D,
    BACKSPACE       = 0x0E,
    TAB             = 0x0F,
    Q               = 0x10,
    W               = 0x11,
    E               = 0x12,
    R               = 0x13,
    T               = 0x14,
    Y               = 0x15,
    U               = 0x16,
    I               = 0x17,
    O               = 0x18,
    P               = 0x19,
    LEFT_BRACKET    = 0x1A,
    RIGHT_BRACKET   = 0x1B,
    ENTER           = 0x1C,
    LEFT_CONTROL    = 0x1D,
    A               = 0x1E,
    S               = 0x1F,
    D               = 0x20,
    F               = 0x21,
    G               = 0x22,
    H               = 0x23,
    J               = 0x24,
    K               = 0x25,
    L               = 0x26,
    SEMICOLON       = 0x27,
    APOSTROPHE      = 0x28,
    GRAVE           = 0x29,
    LEFT_SHIFT      = 0x2A,
    BACKSLASH       = 0x2B,
    Z               = 0x2C,
    X               = 0x2D,
    C               = 0x2E,
    V               = 0x2F,
    B               = 0x30,
    N               = 0x31,
    M               = 0x32,
    COMMA           = 0x33,
    PERIOD          = 0x34,
    SLASH           = 0x35,
    RIGHT_SHIFT     = 0x36,
    KEYPAD_MULTIPLY = 0x37,
    LEFT_ALT        = 0x38,
    SPACE           = 0x39,
    CAPS_LOCK       = 0x3A,
    F1              = 0x3B,
    F2              = 0x3C,
    F3              = 0x3D,
    F4              = 0x3E,
    F5              = 0x3F,
    F6              = 0x40,
    F7              = 0x41,
    F8              = 0x42,
    F9              = 0x43,
    F10             = 0x44,
    NUM_LOCK        = 0x45,
    SCROLL_LOCK     = 0x46,
    KEYPAD_7        = 0x47,
    KEYPAD_8        = 0x48,
    KEYPAD_9        = 0x49,
    KEYPAD_MINUS    = 0x4A,
    KEYPAD_4        = 0x4B,
    KEYPAD_5        = 0x4C,
    KEYPAD_6        = 0x4D,
    KEYPAD_PLUS     = 0x4E,
    KEYPAD_1        = 0x4F,
    KEYPAD_2        = 0x50,
    KEYPAD_3        = 0x51,
    KEYPAD_0        = 0x52,
    KEYPAD_PERIOD   = 0x53,
    F11             = 0x57,
    F12             = 0x58,
    COUNT           = KEY_UP_RANGE_START, // MUST BE THE LAST ELEMENT
};

inline Enum_Array<Scancode, bool> keys;

force_inline auto is_pressed(Scancode code) -> bool {
    return keys[code];
}

auto isr_handle_ps2_keyboard() -> void {
    u8 scancode_value = inb(PS2_DATA_PORT);
    u8 key_value = scancode_value;
    bool key_up = key_value >= KEY_UP_RANGE_START;
    if (key_up) key_value -= KEY_UP_RANGE_START;

    Scancode scancode = static_cast<Scancode>(key_value);
    keys[scancode] = !key_up;
    serial::println("Keyboard interrupt, scancode: %, pressed: %", scancode, is_pressed(scancode));
}

auto isr_handle_ps2_mouse() -> void {
    u8 scancode = inb(PS2_DATA_PORT);
    term::println("Mouse interrupt, scancode: %", scancode);
}

}
