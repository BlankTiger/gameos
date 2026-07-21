#pragma once

#include "../basic.hh"
#include "low_level_io.hh"

namespace time {

constexpr auto PIT_FREQUENCY_HZ  = 1'193'182;
constexpr auto TICK_RATE         = 1000;
constexpr auto TICK_RATE_DIVISOR = PIT_FREQUENCY_HZ / TICK_RATE;

constexpr u16 PIT_CHANNEL0_DATA_PORT = 0x40;
constexpr u16 PIT_CMD_REGISTER       = 0x43;

inline volatile u64 tick_counter = 0;

union Command {
    struct {
        u8 format         : 1;
        u8 operating_mode : 3;
        u8 access_mode    : 2;
        u8 channel_select : 2;
    };
    u8 raw;
} __attribute__((packed));

constexpr Command INIT_CMD = {
    .format          = 0,     // binary format
    .operating_mode  = 0b011, // mode 3: square wave (periodic)
    .access_mode     = 0b11,  // lo/hi byte
    .channel_select  = 0,     // channel 0
};

auto initialize() -> void {
    using namespace low_level_io;

    tick_counter = 0;
    outb(PIT_CMD_REGISTER, INIT_CMD.raw);

    // low bytes
    outb(PIT_CHANNEL0_DATA_PORT, TICK_RATE_DIVISOR & 0xFF);
    // high bytes
    outb(PIT_CHANNEL0_DATA_PORT, (TICK_RATE_DIVISOR >> 8) & 0xFF);
}

force_inline auto on_tick() -> void {
    tick_counter = tick_counter + 1;
}

force_inline auto get_ticks() -> u64 {
    return tick_counter;
}

force_inline auto uptime_ms() -> u64 {
    return get_ticks() * 1000 / TICK_RATE;
}

force_inline auto sleep_ms(u64 amount) -> void {
    auto current_ticks = get_ticks();
    auto waiting_until = current_ticks + amount;
    while (get_ticks() < waiting_until) {
        asm volatile("hlt");
    }
}

}
