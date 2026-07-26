#pragma once

#include "basic.hh"
#include "low_level_io.hh"

namespace time {

// Unit model: the PIT fires an interrupt TICK_RATE times per second (see
// TICK_RATE_DIVISOR), and each interrupt increments tick_counter by one. So a
// "tick" is always exactly 1 / TICK_RATE of a second, i.e. 100us at the
// current TICK_RATE. Every function below that returns/accepts a plain u64
// duration is in ticks, unless its name says otherwise (e.g. _ms suffix).
// Use ms_to_ticks / ticks_to_ms / ticks_per_frame to convert to/from ticks
// instead of multiplying by TICK_RATE by hand.
constexpr auto PIT_FREQUENCY_HZ  = 1'193'182;
constexpr auto TICK_RATE         = 10'000; // ticks per second (100us per tick)
constexpr auto TICK_RATE_DIVISOR = PIT_FREQUENCY_HZ / TICK_RATE;

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

force_inline constexpr auto ms_to_ticks(u64 ms) -> u64 {
    return ms * TICK_RATE / 1000;
}

force_inline constexpr auto ticks_to_ms(u64 ticks) -> u64 {
    return ticks * 1000 / TICK_RATE;
}

// Number of ticks that make up a single period at the given frequency, e.g.
// ticks_per_frame(60) is how many ticks one frame lasts at 60fps.
force_inline constexpr auto ticks_per_frame(u64 hz) -> u64 {
    return TICK_RATE / hz;
}

force_inline auto uptime_ms() -> u64 {
    return ticks_to_ms(get_ticks());
}

force_inline auto sleep_ticks(u64 amount) -> void {
    auto waiting_until = get_ticks() + amount;
    while (get_ticks() < waiting_until) {
        asm volatile("hlt");
    }
}

force_inline auto sleep_ms(u64 amount) -> void {
    sleep_ticks(ms_to_ticks(amount));
}

}
