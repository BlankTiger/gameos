#pragma once

#include <algorithm>
#include <initializer_list>
#include <tuple>

#include "kstd/array.hh"
#include "kstd/numbers.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/ps2.hh"
#include "kstd/math.hh"
#include "kstd/string.hh"

constexpr u64 FPS_MAX                                 = 144;
constexpr u64 ARBITRARY_FALLING_BODY_BLOCK_SIZE_LIMIT = 10;
using Block_Coords = std::tuple<u32, u32, u32>;
using Body         = std::initializer_list<Block_Coords>;
using Falling_Body = Bounded_Array<Block_Coords, ARBITRARY_FALLING_BODY_BLOCK_SIZE_LIMIT>;

constexpr std::initializer_list<Body> available_bodies = {
    Body{{0, 0, 0}, {0, 0, 1}},
    Body{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}},
};

struct Game {
    math::Grid3<bool> grid;
    Falling_Body falling_body;

    u64 time_till_next_move_ms       = 1000;
    u64 current_move_started_at_tick = 0;

    u64 time_ms    = 0;
    f64 dt         = 0.0;
    u64 dt_real    = 0;
    f64 time_scale = 1.0;
    f64 fps        = 0.0;

    Game(u32 rows = 3, u32 cols = 3, u32 layers = 5) : grid(math::Grid3(rows, cols, layers)) {}
};

// there was available space -> true
// otherwise                 -> false
auto set_new_falling_body(Game& game, Falling_Body body) -> bool {
    game.falling_body = body;

    for (const auto& [row, col, layer] : body) {
        if (game.grid.at(row, col, layer)) return false;
    }

    for (const auto& [row, col, layer] : body) {
        game.grid.set(row, col, layer, true);
    }
    return true;
}

auto create_new_falling_body(std::initializer_list<std::tuple<u32, u32, u32>> blocks) -> Falling_Body {
    Falling_Body body;

    for (const auto& block : blocks) {
        body.push_back(block);
    }

    return body;
}

auto update(Game& game) -> void {
    // Handle rotations.

    // Handle movement.

    using namespace time;
    bool timer_elapsed = get_ticks() >= game.current_move_started_at_tick + ms_to_ticks(game.time_till_next_move_ms);
    bool fast_forward_key_pressed = false; // @TODO: implement.

    // If timer elapsed or a button has been pushed then move down, or if it's
    // not possible solidify into the stationary layers at the bottom.
    if (timer_elapsed || fast_forward_key_pressed) {
        bool falling_body_can_move_lower = true;
        for (const auto& [row, col, layer] : game.falling_body) {
            if (!game.grid.can_move_lower(row, col, layer)) {
                falling_body_can_move_lower = false;
                break;
            }
        }

        if (falling_body_can_move_lower) {
            for (auto& [row, col, layer] : game.falling_body) {
                game.grid.clear(row, col, layer);
                layer += 1;
                game.grid.set(row, col, layer, true);
            }
        }
        else {
            // Solidify into bottom-most layers.
            // Get and set the next falling_body.
        }

        game.current_move_started_at_tick = get_ticks();
    }

    // If there is a new full layer, then clear it and move everything above it
    // lower by a layer.
}

auto draw(const Game& game) -> void {
    gfx::clear(gfx::BLACK);

    const auto fps_text = sprint("FPS: %", game.fps);
    gfx::draw_text(8, 8, fps_text);

    const auto grid_as_text = sprint("%", game.grid);
    gfx::draw_text(gfx::width() / 2, gfx::height() / 8, grid_as_text);

    gfx::draw_frame();
}

auto game_main() -> void {
    Game game;

    Falling_Body body = create_new_falling_body({{0, 0, 0}});
    set_new_falling_body(game, body);

    u64 last_tick = time::get_ticks();
    game.current_move_started_at_tick = last_tick;

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
