#pragma once

#include <initializer_list>

#include "kstd/allocator.hh"
#include "kstd/array.hh"
#include "kstd/numbers.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/input.hh"
#include "kstd/math.hh"
#include "kstd/serial_format.hh"
#include "kstd/string_builder.hh"
#include "kstd/random.hh"
#include "kstd/power.hh"


using namespace ktime;
using namespace math;
using Key = input::Key;

constexpr u64 FPS_MAX                                 = 144;
constexpr u64 ARBITRARY_FALLING_BODY_BLOCK_SIZE_LIMIT = 10;

using Block_Coords = Vector3<u32>;
using Body = std::initializer_list<Block_Coords>;
using Blocks = Bounded_Array<Block_Coords, ARBITRARY_FALLING_BODY_BLOCK_SIZE_LIMIT>;

struct Falling_Body {
    Blocks blocks;

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
    Grid3<Block_Type> grid;
    Falling_Body falling_body;

    Array<u32> layers_to_destroy;

    u64 time_till_next_move_ms       = 1000;
    u64 current_move_started_at_tick = 0;
    u64 destroy_layers_after_ms      = 1000;

    u64 time_ms    = 0;
    f64 dt         = 0.0;
    u64 dt_real    = 0;
    f64 time_scale = 1.0;
    f64 fps        = 0.0;

    Game(u32 rows = 3, u32 cols = 3, u32 layers = 7) : grid(Grid3<Block_Type>(rows, cols, layers)) {}
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
    const auto body_index = krand::generate(0, available_bodies.size());
    const auto body       = available_bodies.begin()[body_index];

    auto falling_body = create_new_falling_body(body);

    for (const auto& [row, col, layer] : falling_body.blocks) {
        if (game.grid.at(row, col, layer) != Block_Type::EMPTY) return false;
    }

    for (const auto& [row, col, layer] : falling_body.blocks) {
        game.grid.set(row, col, layer, Block_Type::FALLING);
    }

    game.falling_body = falling_body;
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

enum struct Rotation_Axis { X, Y, Z };

auto rotate_90(Block_Coords block, Block_Coords center_of_rotation, Rotation_Axis axis) -> Block_Coords {
    Vector3<s32> a(block);
    Vector3<s32> b(center_of_rotation);

    Vector3<s32> diff_position = a - b;
    Vector3<s32> new_position{};

    using enum Rotation_Axis;
    switch (axis) {
        case X: {
            new_position.x = -diff_position.y;
            new_position.y =  diff_position.x;
            new_position.z =  diff_position.z;
        } break;

        case Y: {
            new_position.x =  diff_position.x;
            new_position.y = -diff_position.z;
            new_position.z =  diff_position.y;
        } break;

        case Z: {
            new_position.x =  diff_position.z;
            new_position.y = -diff_position.y;
            new_position.z =  diff_position.y;
        } break;
    }

    return Block_Coords(new_position + b);
}

auto calculate_center_of_body(const Falling_Body& body) -> Block_Coords {
    u32 min_row   = U32_MAX, max_row   = 0;
    u32 min_col   = U32_MAX, max_col   = 0;
    u32 min_layer = U32_MAX, max_layer = 0;

    for (const auto& [row, col, layer] : body.blocks) {
        min_row = std::min(min_row, row);
        max_row = std::max(max_row, row);

        min_col = std::min(min_col, col);
        max_col = std::max(max_col, col);

        min_layer = std::min(min_layer, layer);
        max_layer = std::max(max_layer, layer);
    }

    return Block_Coords(
        (min_row   + max_row)   / 2,
        (min_col   + max_col)   / 2,
        (min_layer + max_layer) / 2
    );
}

auto try_rotating_falling_body(Game& game, Rotation_Axis axis) -> bool {
    auto center_of_rotation = calculate_center_of_body(game.falling_body);

    Blocks rotated;
    for (const auto& block : game.falling_body.blocks) {
        auto new_coords = rotate_90(block, center_of_rotation, axis);

        // u32 so no need to check if negative.
        if (new_coords.x >= game.grid.rows ||
            new_coords.y >= game.grid.cols ||
            new_coords.z >= game.grid.layers) {
            return false;
        }

        if (game.grid.at(new_coords.x, new_coords.y, new_coords.z) != Block_Type::EMPTY) {
            bool is_self = false;
            for (const auto& other : game.falling_body.blocks) {
                if (other == new_coords) {
                    is_self = true;
                    break;
                }
            }

            if (!is_self) return false;
        }

        rotated.push_back(new_coords);
    }

    for (const auto& [x, y, z] : game.falling_body.blocks) {
        game.grid.clear(x, y, z);
    }

    game.falling_body.blocks = rotated;
    game.falling_body.layer_sort();

    for (const auto& [x, y, z] : game.falling_body.blocks) {
        game.grid.set(x, y, z, Block_Type::FALLING);
    }

    return true;
}

auto update(Game& game) -> void {
    // Handle layer destruction.
    {
        if (game.layers_to_destroy.size > 0) {
            sleep_ms(game.destroy_layers_after_ms);

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
    }

    // Handle rotations.
    {
        // @TODO: implement.
        bool rotate_x_key_pressed = false;
        bool rotate_y_key_pressed = false;
        bool rotate_z_key_pressed = false;

        if (rotate_x_key_pressed) {
            try_rotating_falling_body(game, Rotation_Axis::X);
        }
        else if (rotate_y_key_pressed) {
            try_rotating_falling_body(game, Rotation_Axis::Y);
        }
        else if (rotate_z_key_pressed) {
            try_rotating_falling_body(game, Rotation_Axis::Z);
        }
    }

    // Handle movement.
    {
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
}

auto draw(const Game& game) -> void {
    gfx::clear(gfx::BLACK);

    gfx::draw_text(8, 8, tprint("FPS: %", game.fps));
    gfx::draw_text(gfx::width() / 3, gfx::height() / 8, tprint("%", game.grid));

    gfx::draw_frame();
}

auto tetris_main() -> void {
    mem::Debug_Allocator dbg_allocator{};
    mem::set_global_allocator(&dbg_allocator);

    gfx::Camera3D camera; // TODO: implement moving/rotating camera
    Game game;
    for (u32 row = 0; row < game.grid.rows; ++row) {
        for (u32 col = 0; col < game.grid.cols; ++col) {
            game.grid.set(row, col, game.grid.layers - 1, Block_Type::SOLID);
        }
    }
    game.grid.clear(0, 0, game.grid.layers - 1);

    auto ok = produce_new_falling_body(game);
    kstd_assert(ok);

    u64 last_tick = get_ticks();
    game.current_move_started_at_tick = last_tick;

    constexpr auto TARGET_TICKS = ticks_per_frame(FPS_MAX);

    const auto* temporary_allocator_mark = mem::temporary_allocator.mark();
    while (true) {
        input::begin_frame();
        if (input::key_pressed(Key::ESCAPE)) break;

        const u64 frame_start = get_ticks();
        const u64 elapsed     = frame_start - last_tick;
        last_tick = frame_start;

        {
            game.dt_real  = elapsed;
            game.dt       = static_cast<f64>(game.dt_real) / TICK_RATE * game.time_scale;
            game.time_ms += ticks_to_ms(game.dt_real);
            game.fps      = 1 / game.dt;

            update(game);
            draw(game);

            // serial::println("% MB", static_cast<f32>(mem::temporary_allocator.bytes_used()) / 1000000);
            mem::temporary_allocator.rewind(temporary_allocator_mark);
        }

        const u64 frame_ticks = get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) {
            sleep_ticks(TARGET_TICKS - frame_ticks);
        }
    }

    power::off();
}
