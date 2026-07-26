#pragma once

#include <algorithm>

#include "kstd/int.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/ps2.hh"
#include "kstd/string.hh"

constexpr u64 FPS_MAX = 144;

struct Game {
    u64 time_ms    = 0;
    f64 dt         = 0.0;
    u64 dt_real    = 0;
    f64 time_scale = 1.0;
    f64 fps        = 0.0;
};

auto update(Game& game) -> void {}

auto draw(const Game& game) -> void {
    gfx::clear(gfx::BLACK);

    const auto fps_text = sprint("FPS: %", game.fps);
    gfx::draw_text(8, 8, fps_text);

    gfx::draw_frame();
}

auto game_main() -> void {
    Game game;

    u64 last_tick = time::get_ticks();

    constexpr auto TARGET_TICKS = time::ticks_per_frame(FPS_MAX);

    while (!ps2::is_pressed(ps2::Scancode::ESCAPE)) {
        const u64 frame_start = time::get_ticks();
        const u64 elapsed     = frame_start - last_tick;
        last_tick = frame_start;

        game.dt_real  = elapsed;
        game.dt       = static_cast<f64>(game.dt_real) / time::TICK_RATE * game.time_scale;
        game.time_ms += time::ticks_to_ms(game.dt_real);
        game.fps      = 1 / game.dt;

        update(game);
        draw(game);

        const u64 frame_ticks = time::get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) {
            time::sleep_ticks(TARGET_TICKS - frame_ticks);
        }
    }
}
