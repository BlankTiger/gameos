#pragma once

#include "low_level_io.hh"
#include "format.hh"

namespace serial {

constexpr u16 COM1 = 0x3F8;

constexpr u16 REG_DATA         = 0; // +0, DLAB=0
constexpr u16 REG_DIVISOR_LOW  = 0; // +0, DLAB=1
constexpr u16 REG_IER          = 1; // +1, DLAB=0
constexpr u16 REG_DIVISOR_HIGH = 1; // +1, DLAB=1
constexpr u16 REG_FCR          = 2;
constexpr u16 REG_LCR          = 3;
constexpr u16 REG_MCR          = 4;
constexpr u16 REG_LSR          = 5;

constexpr u8 LCR_DATA_BITS_8 = 0b0000'0011;
constexpr u8 LCR_DLAB        = 0b1000'0000;

constexpr u8 FCR_ENABLE     = 0b0000'0001;
constexpr u8 FCR_CLEAR_RX   = 0b0000'0010;
constexpr u8 FCR_CLEAR_TX   = 0b0000'0100;
constexpr u8 FCR_TRIGGER_14 = 0b1100'0000;

constexpr u8 MCR_DTR  = 0b0000'0001;
constexpr u8 MCR_RTS  = 0b0000'0010;
constexpr u8 MCR_OUT2 = 0b0000'1000;

constexpr u8 LSR_THR_EMPTY = 0b0010'0000;

constexpr u16 BAUD_DIVISOR_38400 = 3;  // 115200 / divisor = baud rate

force_inline auto transmit_empty() -> bool {
    auto status = low_level_io::inb(COM1 + REG_LSR);
    return (status & LSR_THR_EMPTY) != 0;
}

auto initialize() -> void {
    low_level_io::outb(COM1 + REG_IER, 0x00);
    low_level_io::outb(COM1 + REG_LCR, LCR_DLAB);
    low_level_io::outb(COM1 + REG_DIVISOR_LOW,  BAUD_DIVISOR_38400 & 0xFF);
    low_level_io::outb(COM1 + REG_DIVISOR_HIGH, BAUD_DIVISOR_38400 >> 8);
    low_level_io::outb(COM1 + REG_LCR, LCR_DATA_BITS_8);
    low_level_io::outb(COM1 + REG_FCR, FCR_ENABLE | FCR_CLEAR_RX | FCR_CLEAR_TX | FCR_TRIGGER_14);
    low_level_io::outb(COM1 + REG_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);
}

auto put_char(char c) -> void {
    // If serial is somehow not available it's better to drop a byte than lock everything.
    for (int spins = 0; spins < 100000 && !transmit_empty(); ++spins) {}
    low_level_io::outb(COM1, (u8)c);
}

struct Backend {
    static auto put_char(char c) -> void {
        serial::put_char(c);
    }

    static auto new_line() -> void {
        serial::put_char('\n');
    }
};

auto print(const char* format) -> int {
    return fmt::print<Backend>(format);
}

template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::print<Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

auto println() -> int {
    return fmt::println<Backend>();
}

auto println(const char* format) -> int {
    return fmt::println<Backend>(format);
}

template <typename T>
auto print(T&& value) -> int {
    return fmt::print<Backend>(std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    return fmt::println<Backend>(std::forward<T>(value));
}

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::println<Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

}
