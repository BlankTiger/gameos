#pragma once

#include "enum_flags.hh"
#include "input.hh"
#include "low_level_io.hh"

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

// Extended (0xE0-prefix) scancodes add this offset to distinguish from base
// scancodes.  E.g. scancode set 1 Right Arrow is E0 4D -> 0x4D + 0x80 = 0xCD.
constexpr u8 KEY_EXTENDED_OFFSET = 0x80;

enum struct Scancode : u16 {
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

    // Extended (0xE0 prefix) scancodes. Stored at base + KEY_EXTENDED_OFFSET.
    RIGHT_CONTROL   = 0x1D + KEY_EXTENDED_OFFSET,
    RIGHT_ALT       = 0x38 + KEY_EXTENDED_OFFSET,
    RIGHT_ARROW     = 0x4D + KEY_EXTENDED_OFFSET,
    LEFT_ARROW      = 0x4B + KEY_EXTENDED_OFFSET,
    UP_ARROW        = 0x48 + KEY_EXTENDED_OFFSET,
    DOWN_ARROW      = 0x50 + KEY_EXTENDED_OFFSET,

    COUNT           = KEY_UP_RANGE_START + KEY_EXTENDED_OFFSET, // MUST BE THE LAST ELEMENT
};
@enum_to_string(Scancode);

inline bool extended_pending = false;

auto to_input_key(Scancode scancode) -> input::Key {
    switch (scancode) {
        case Scancode::ESCAPE:          return input::Key::ESCAPE;
        case Scancode::DIGIT_1:         return input::Key::DIGIT_1;
        case Scancode::DIGIT_2:         return input::Key::DIGIT_2;
        case Scancode::DIGIT_3:         return input::Key::DIGIT_3;
        case Scancode::DIGIT_4:         return input::Key::DIGIT_4;
        case Scancode::DIGIT_5:         return input::Key::DIGIT_5;
        case Scancode::DIGIT_6:         return input::Key::DIGIT_6;
        case Scancode::DIGIT_7:         return input::Key::DIGIT_7;
        case Scancode::DIGIT_8:         return input::Key::DIGIT_8;
        case Scancode::DIGIT_9:         return input::Key::DIGIT_9;
        case Scancode::DIGIT_0:         return input::Key::DIGIT_0;
        case Scancode::MINUS:           return input::Key::MINUS;
        case Scancode::EQUALS:          return input::Key::EQUALS;
        case Scancode::BACKSPACE:       return input::Key::BACKSPACE;
        case Scancode::TAB:             return input::Key::TAB;
        case Scancode::Q:               return input::Key::Q;
        case Scancode::W:               return input::Key::W;
        case Scancode::E:               return input::Key::E;
        case Scancode::R:               return input::Key::R;
        case Scancode::T:               return input::Key::T;
        case Scancode::Y:               return input::Key::Y;
        case Scancode::U:               return input::Key::U;
        case Scancode::I:               return input::Key::I;
        case Scancode::O:               return input::Key::O;
        case Scancode::P:               return input::Key::P;
        case Scancode::LEFT_BRACKET:    return input::Key::LEFT_BRACKET;
        case Scancode::RIGHT_BRACKET:   return input::Key::RIGHT_BRACKET;
        case Scancode::ENTER:           return input::Key::ENTER;
        case Scancode::LEFT_CONTROL:    return input::Key::LEFT_CONTROL;
        case Scancode::A:               return input::Key::A;
        case Scancode::S:               return input::Key::S;
        case Scancode::D:               return input::Key::D;
        case Scancode::F:               return input::Key::F;
        case Scancode::G:               return input::Key::G;
        case Scancode::H:               return input::Key::H;
        case Scancode::J:               return input::Key::J;
        case Scancode::K:               return input::Key::K;
        case Scancode::L:               return input::Key::L;
        case Scancode::SEMICOLON:       return input::Key::SEMICOLON;
        case Scancode::APOSTROPHE:      return input::Key::APOSTROPHE;
        case Scancode::GRAVE:           return input::Key::GRAVE;
        case Scancode::LEFT_SHIFT:      return input::Key::LEFT_SHIFT;
        case Scancode::BACKSLASH:       return input::Key::BACKSLASH;
        case Scancode::Z:               return input::Key::Z;
        case Scancode::X:               return input::Key::X;
        case Scancode::C:               return input::Key::C;
        case Scancode::V:               return input::Key::V;
        case Scancode::B:               return input::Key::B;
        case Scancode::N:               return input::Key::N;
        case Scancode::M:               return input::Key::M;
        case Scancode::COMMA:           return input::Key::COMMA;
        case Scancode::PERIOD:          return input::Key::PERIOD;
        case Scancode::SLASH:           return input::Key::SLASH;
        case Scancode::RIGHT_SHIFT:     return input::Key::RIGHT_SHIFT;
        case Scancode::KEYPAD_MULTIPLY: return input::Key::KEYPAD_MULTIPLY;
        case Scancode::LEFT_ALT:        return input::Key::LEFT_ALT;
        case Scancode::SPACE:           return input::Key::SPACE;
        case Scancode::CAPS_LOCK:       return input::Key::CAPS_LOCK;
        case Scancode::F1:              return input::Key::F1;
        case Scancode::F2:              return input::Key::F2;
        case Scancode::F3:              return input::Key::F3;
        case Scancode::F4:              return input::Key::F4;
        case Scancode::F5:              return input::Key::F5;
        case Scancode::F6:              return input::Key::F6;
        case Scancode::F7:              return input::Key::F7;
        case Scancode::F8:              return input::Key::F8;
        case Scancode::F9:              return input::Key::F9;
        case Scancode::F10:             return input::Key::F10;
        case Scancode::NUM_LOCK:        return input::Key::NUM_LOCK;
        case Scancode::SCROLL_LOCK:     return input::Key::SCROLL_LOCK;
        case Scancode::KEYPAD_7:        return input::Key::KEYPAD_7;
        case Scancode::KEYPAD_8:        return input::Key::KEYPAD_8;
        case Scancode::KEYPAD_9:        return input::Key::KEYPAD_9;
        case Scancode::KEYPAD_MINUS:    return input::Key::KEYPAD_MINUS;
        case Scancode::KEYPAD_4:        return input::Key::KEYPAD_4;
        case Scancode::KEYPAD_5:        return input::Key::KEYPAD_5;
        case Scancode::KEYPAD_6:        return input::Key::KEYPAD_6;
        case Scancode::KEYPAD_PLUS:     return input::Key::KEYPAD_PLUS;
        case Scancode::KEYPAD_1:        return input::Key::KEYPAD_1;
        case Scancode::KEYPAD_2:        return input::Key::KEYPAD_2;
        case Scancode::KEYPAD_3:        return input::Key::KEYPAD_3;
        case Scancode::KEYPAD_0:        return input::Key::KEYPAD_0;
        case Scancode::KEYPAD_PERIOD:   return input::Key::KEYPAD_PERIOD;
        case Scancode::F11:             return input::Key::F11;
        case Scancode::F12:             return input::Key::F12;
        case Scancode::RIGHT_CONTROL:   return input::Key::RIGHT_CONTROL;
        case Scancode::RIGHT_ALT:       return input::Key::RIGHT_ALT;
        case Scancode::RIGHT_ARROW:     return input::Key::RIGHT_ARROW;
        case Scancode::LEFT_ARROW:      return input::Key::LEFT_ARROW;
        case Scancode::UP_ARROW:        return input::Key::UP_ARROW;
        case Scancode::DOWN_ARROW:      return input::Key::DOWN_ARROW;
        case Scancode::COUNT:           return input::Key::UNKNOWN;
    }

    return input::Key::UNKNOWN;
}

auto isr_handle_ps2_keyboard() -> void {
    u8 scancode_value = inb(PS2_DATA_PORT);

    // Handle 0xE0 extended prefix.
    if (scancode_value == 0xE0) {
        extended_pending = true;
        return;
    }

    u8 key_value = scancode_value;
    bool key_up  = key_value >= KEY_UP_RANGE_START;
    if (key_up) key_value -= KEY_UP_RANGE_START;

    if (extended_pending) {
        key_value        += KEY_EXTENDED_OFFSET;
        extended_pending  = false;
    }

    const auto key = to_input_key(static_cast<Scancode>(key_value));
    if (key == input::Key::UNKNOWN) return;
    const bool repeat = !key_up && input::key_held(key);
    if (key_up) {
        input::submit_event({
            .type   = input::Event_Type::KEY_UP,
            .key_up = { .key = key, .repeat = false },
        });
    } else {
        input::submit_event({
            .type     = input::Event_Type::KEY_DOWN,
            .key_down = { .key = key, .repeat = repeat },
        });
    }
}

constexpr usize MOUSE_PACKET_SIZE = 3;

enum struct Mouse_Flags : u8 {
    LEFT_BUTTON   = 1 << 0,
    RIGHT_BUTTON  = 1 << 1,
    MIDDLE_BUTTON = 1 << 2,
    ALWAYS_ONE    = 1 << 3,
    X_SIGN        = 1 << 4,
    Y_SIGN        = 1 << 5,
    X_OVERFLOW    = 1 << 6,
    Y_OVERFLOW    = 1 << 7,
};

struct Mouse_Packet_State {
    Static_Array<u8, MOUSE_PACKET_SIZE> bytes;
    u8                                  index = 0;
};

inline Mouse_Packet_State mouse_packet;

auto submit_mouse_button(Mouse_Flags flags, Mouse_Flags button_flag, input::Mouse_Button button) -> void {
    const bool is_down = has_flag(flags, button_flag);
    if (input::mouse_button_held(button) == is_down) return;

    if (is_down) {
        input::submit_event({
            .type = input::Event_Type::MOUSE_BUTTON_DOWN,
            .mouse_button_down = { .button = button },
        });
    } else {
        input::submit_event({
            .type = input::Event_Type::MOUSE_BUTTON_UP,
            .mouse_button_up = { .button = button },
        });
    }
}

auto process_mouse_byte(u8 value) -> void {
    if (mouse_packet.index == 0 && !has_flag(static_cast<Mouse_Flags>(value), Mouse_Flags::ALWAYS_ONE)) return;

    mouse_packet.bytes[mouse_packet.index] = value;
    ++mouse_packet.index;
    if (mouse_packet.index < MOUSE_PACKET_SIZE) return;
    mouse_packet.index = 0;

    const auto flags = static_cast<Mouse_Flags>(mouse_packet.bytes[0]);
    submit_mouse_button(flags, Mouse_Flags::LEFT_BUTTON, input::Mouse_Button::LEFT);
    submit_mouse_button(flags, Mouse_Flags::RIGHT_BUTTON, input::Mouse_Button::RIGHT);
    submit_mouse_button(flags, Mouse_Flags::MIDDLE_BUTTON, input::Mouse_Button::MIDDLE);

    if (has_flag(flags, Mouse_Flags::X_OVERFLOW) || has_flag(flags, Mouse_Flags::Y_OVERFLOW)) return;

    const auto xrel = static_cast<s32>(static_cast<s8>(mouse_packet.bytes[1]));
    const auto yrel = -static_cast<s32>(static_cast<s8>(mouse_packet.bytes[2]));
    if (xrel == 0 && yrel == 0) return;

    input::submit_event({
        .type = input::Event_Type::MOUSE_MOTION,
        .mouse_motion = { .xrel = xrel, .yrel = yrel },
    });
}

auto isr_handle_ps2_mouse() -> void {
    constexpr u8 OUTPUT_BUFFER_FULL = 1 << 0;
    constexpr u8 AUX_DATA          = 1 << 5;

    while (has_flag(static_cast<Mouse_Flags>(inb(PS2_STATUS_PORT)), static_cast<Mouse_Flags>(OUTPUT_BUFFER_FULL | AUX_DATA))) {
        process_mouse_byte(inb(PS2_DATA_PORT));
    }
}

}
