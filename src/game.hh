#pragma once

#include <initializer_list>

#include "kstd/allocator.hh"
#include "kstd/array.hh"
#include "kstd/numbers.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/ps2.hh"
#include "kstd/math.hh"
#include "kstd/string_builder.hh"
#include "kstd/random.hh"

constexpr u64 FPS_MAX                                 = 144;
constexpr u64 ARBITRARY_FALLING_BODY_BLOCK_SIZE_LIMIT = 10;

using Block_Coords = math::Vector3<u32>;
using Body = std::initializer_list<Block_Coords>;

struct Falling_Body {
    Bounded_Array<Block_Coords, ARBITRARY_FALLING_BODY_BLOCK_SIZE_LIMIT> blocks;

    auto layer_sort() -> void {
        std::sort(blocks.begin(), blocks.end(), [](const Block_Coords& a, const Block_Coords& b) {
            return a.z > b.z;
        });
    }
};

// Tetracubes below use row/col for the 3x3 footprint and
// layer as the falling axis.
constexpr std::initializer_list<Body> available_bodies = {
    Body{{0, 0, 0}, {0, 0, 1}},                        // domino
    Body{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}},             // tromino, bent
    Body{{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {0, 0, 3}},  // I
    Body{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 0}},  // O (square)
    Body{{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 2}},  // L
    Body{{1, 0, 0}, {1, 0, 1}, {1, 0, 2}, {0, 0, 2}},  // J (mirrored L)
    Body{{0, 0, 0}, {0, 0, 1}, {0, 0, 2}, {1, 0, 1}},  // T
    Body{{1, 0, 0}, {2, 0, 0}, {0, 0, 1}, {1, 0, 1}},  // S (skew)
    Body{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {2, 0, 1}},  // Z (mirrored S)
    Body{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}},  // branch (tripod)
    Body{{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},  // left screw
    Body{{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {1, 1, 1}},  // right screw (mirrored)
};

enum struct Block_Type {
    EMPTY,
    SOLID,
    FALLING,
};

struct Game {
    math::Grid3<Block_Type> grid;
    Falling_Body falling_body;

    Array<u32> layers_to_destroy;

    u64 time_till_next_move_ms       = 1000;
    u64 current_move_started_at_tick = 0;

    u64 time_ms    = 0;
    f64 dt         = 0.0;
    u64 dt_real    = 0;
    f64 time_scale = 1.0;
    f64 fps        = 0.0;

    Game(u32 rows = 3, u32 cols = 3, u32 layers = 7) : grid(math::Grid3<Block_Type>(rows, cols, layers)) {}
};

auto create_new_falling_body(Body blocks) -> Falling_Body {
    Falling_Body body;

    for (const auto& block : blocks) {
        body.blocks.push_back(block);
    }
    body.layer_sort();

    return body;
}

// available space -> true
// otherwise       -> false
auto produce_new_falling_body(Game& game) -> bool {
    const auto body_index = rand::generate(0, available_bodies.size());
    const auto body       = available_bodies.begin()[body_index];

    auto falling_body = create_new_falling_body(body);
    falling_body.layer_sort();

    game.falling_body = falling_body;

    for (const auto& [row, col, layer] : falling_body.blocks) {
        if (game.grid.at(row, col, layer) != Block_Type::EMPTY) return false;
    }

    for (const auto& [row, col, layer] : falling_body.blocks) {
        game.grid.set(row, col, layer, Block_Type::FALLING);
    }
    return true;
}

auto falling_body_can_go_lower(const Game& game) -> bool {
    for (const auto& [row, col, layer] : game.falling_body.blocks) {
        if (game.grid.can_move_lower(row, col, layer)) continue;

        const auto next_layer = layer + 1;
        bool block_exists_in_this_body = false;
        // @TODO: optimize maybe? Don't think it's necessary.
        for (const auto& block_coords : game.falling_body.blocks) {
            if (block_coords == Block_Coords{row, col, next_layer}) {
                block_exists_in_this_body = true;
                break;
            }
        }

        if (!block_exists_in_this_body) return false;
    }

    return true;
}

auto update(Game& game) -> void {
    // Handle layer destruction.
    {
        Array<bool> layer_destroyed(game.grid.layers, false);

        for (u32 layer : game.layers_to_destroy) {
            layer_destroyed[layer] = true;
        }

        auto skip_if_is_falling = [&](u32 row, u32 col, u32 layer) {
            return game.grid.at(row, col, layer) == Block_Type::FALLING;
        };

        s64 write_layer = static_cast<s64>(game.grid.layers) - 1;

        for (s64 read_layer = static_cast<s64>(game.grid.layers) - 1; read_layer >= 0; --read_layer) {
            if (!layer_destroyed[read_layer]) {
                if (read_layer != write_layer) {
                    game.grid.copy_layer(static_cast<u32>(read_layer), static_cast<u32>(write_layer), skip_if_is_falling);
                }
                --write_layer;
            }
        }

        for (s64 layer = write_layer; layer >= 0; --layer) {
            game.grid.clear_layer(static_cast<u32>(layer), skip_if_is_falling);
        }

        game.layers_to_destroy.clear();
    }

    // Handle rotations.

    // Handle movement.

    using namespace time;
    bool timer_elapsed = get_ticks() >= game.current_move_started_at_tick + ms_to_ticks(game.time_till_next_move_ms);
    bool fast_forward_key_pressed = false; // @TODO: implement.

    // If timer elapsed or a button has been pushed then move down, or if it's
    // not possible solidify into the stationary layers at the bottom.
    if (timer_elapsed || fast_forward_key_pressed) {
        if (falling_body_can_go_lower(game)) {
            // This has to go in layer sorted order (bottom-most to top-most layer).
            // falling body should have already been sorted, but it doesn't cost us much to make sure:
            game.falling_body.layer_sort();

            for (auto& [row, col, layer] : game.falling_body.blocks) {
                game.grid.clear(row, col, layer);
                layer += 1;
                game.grid.set(row, col, layer, Block_Type::FALLING);
            }
        }
        else {
            // Solidify into bottom-most layers (FALLING -> SOLID).
            // Get and set the next falling_body.
            for (const auto& [row, col, layer] : game.falling_body.blocks) {
                game.grid.set(row, col, layer, Block_Type::SOLID);
            }

            // If there is a new full layer, then schedule it for destruction.
            for (s64 layer_index = game.grid.layers - 1; layer_index >= 0; --layer_index) {
                if (game.grid.layer_filled_with(layer_index, Block_Type::SOLID)) {
                    game.layers_to_destroy.push_back(layer_index);
                }
            }

            auto ok = produce_new_falling_body(game);
            (void)ok; // @TODO: Discarding for now, will use later for determining if the game is over.
        }

        game.current_move_started_at_tick = get_ticks();
    }
}

auto draw(const Game& game) -> void {
    gfx::clear(gfx::BLACK);

    gfx::draw_text(8, 8, tprint("FPS: %", game.fps));
    gfx::draw_text(gfx::width() / 3, gfx::height() / 8, tprint("%", game.grid));

    gfx::draw_frame();
}

auto game_main() -> void {
    Game game;
    for (u32 row = 0; row < game.grid.rows; ++row) {
        for (u32 col = 0; col < game.grid.cols; ++col) {
            game.grid.set(row, col, game.grid.layers - 1, Block_Type::SOLID);
        }
    }
    game.grid.clear(0, 0, game.grid.layers - 1);

    auto ok = produce_new_falling_body(game);
    kstd_assert(ok);

    u64 last_tick = time::get_ticks();
    game.current_move_started_at_tick = last_tick;

    constexpr auto TARGET_TICKS = time::ticks_per_frame(FPS_MAX);

    const auto* temporary_allocator_mark = mem::temporary_allocator.mark();
    while (!ps2::is_pressed(ps2::Scancode::ESCAPE)) {
        const u64 frame_start = time::get_ticks();
        const u64 elapsed     = frame_start - last_tick;
        last_tick = frame_start;

        {
            game.dt_real  = elapsed;
            game.dt       = static_cast<f64>(game.dt_real) / time::TICK_RATE * game.time_scale;
            game.time_ms += time::ticks_to_ms(game.dt_real);
            game.fps      = 1 / game.dt;

            update(game);
            draw(game);

            // serial::println("% MB", static_cast<f32>(mem::temporary_allocator.bytes_used()) / 1000000);
            mem::temporary_allocator.rewind(temporary_allocator_mark);
        }

        const u64 frame_ticks = time::get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) {
            time::sleep_ticks(TARGET_TICKS - frame_ticks);
        }
    }
}
