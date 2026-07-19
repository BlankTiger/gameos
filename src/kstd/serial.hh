#pragma once

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

force_inline auto outb(u16 port, u8 value) -> void {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

force_inline auto transmit_empty() -> bool {
    u8 status;
    asm volatile("inb %1, %0" : "=a"(status) : "Nd"((u16)(COM1 + REG_LSR)));
    return (status & LSR_THR_EMPTY) != 0;
}

auto initialize() -> void {
    outb(COM1 + REG_IER, 0x00);
    outb(COM1 + REG_LCR, LCR_DLAB);
    outb(COM1 + REG_DIVISOR_LOW,  BAUD_DIVISOR_38400 & 0xFF);
    outb(COM1 + REG_DIVISOR_HIGH, BAUD_DIVISOR_38400 >> 8);
    outb(COM1 + REG_LCR, LCR_DATA_BITS_8);
    outb(COM1 + REG_FCR, FCR_ENABLE | FCR_CLEAR_RX | FCR_CLEAR_TX | FCR_TRIGGER_14);
    outb(COM1 + REG_MCR, MCR_DTR | MCR_RTS | MCR_OUT2);
}

auto put_char(char c) -> void {
    while (!transmit_empty()) {}
    outb(COM1, (u8)c);
}

force_inline auto print(const char* message) -> void {
    while (*message) put_char(*message++);
}

force_inline auto println(const char* message) -> void {
    print(message);
    put_char('\n');
}

}
