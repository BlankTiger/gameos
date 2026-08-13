#pragma once

#include <initializer_list>

#include "kstd/allocator.hh"
#include "kstd/array.hh"
#include "kstd/math.hh"
#include "kstd/numbers.hh"
#include "kstd/string_builder.hh"

#include "gameos/time.hh"
#include "gameos/gfx.hh"
#include "gameos/input.hh"
#include "gameos/serial_format.hh"
#include "gameos/random.hh"
#include "gameos/power.hh"


using namespace ktime;
using namespace math;
using Key = input::Key;

constexpr u64 FPS_MAX                                 = 280;
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

auto falling_body_contains(const Falling_Body& body, Block_Coords coords) -> bool {
    for (const auto& block : body.blocks) {
        if (block == coords) return true;
    }
    return false;
}

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

constexpr u32 TETRIS_BOARD_CELL_ROWS = 10;
constexpr u32 TETRIS_BOARD_CELL_COLS = 10;
constexpr u32 TETRIS_BOARD_CELL_COUNT = TETRIS_BOARD_CELL_ROWS * TETRIS_BOARD_CELL_COLS;
constexpr u32 TETRIS_BOARD_VERTEX_COUNT = TETRIS_BOARD_CELL_COUNT * 4;
constexpr u32 TETRIS_BOARD_INDEX_COUNT = TETRIS_BOARD_CELL_COUNT * 2;
constexpr u64 TETRIS_FAST_FALL_TIME_MS = 50;

struct Game {
    Grid3<Block_Type> grid;
    Grid3<u8> body_indices;
    Falling_Body falling_body;
    usize falling_body_index = 0;
    bool game_over = false;

    Array<u32> layers_to_destroy;

    u64 time_till_next_move_ms       = 1000;
    u64 current_move_started_at_tick = 0;
    u64 destroy_layers_after_ms      = 1000;

    u64 time_ms    = 0;
    f64 dt         = 0.0;
    u64 dt_real    = 0;
    f64 time_scale = 1.0;
    f64 fps        = 0.0;

    Game(u32 rows = TETRIS_BOARD_CELL_ROWS, u32 cols = TETRIS_BOARD_CELL_COLS, u32 layers = 20)
        : grid(Grid3<Block_Type>(rows, cols, layers)),
          body_indices(Grid3<u8>(rows, cols, layers)) {}
};

constexpr f32 TETRIS_BLOCK_SCALE = 0.5f;
constexpr f32 TETRIS_BLOCK_SPACING = 1.f;

constexpr f32 TETRIS_ORIGIN_Z = 0.0f;
constexpr f32 TETRIS_CAMERA_OFFSET_Z = -8.0f;
constexpr f32 TETRIS_CAMERA_RADIUS = 20.0f;
constexpr f32 TETRIS_CAMERA_MOUSE_SENSITIVITY = 0.01f;

constexpr f32 TETRIS_PI = 3.14159265358979323846f;

constexpr f32 TETRIS_BOARD_WALL_GAP = 0.05f;
constexpr f32 TETRIS_BOARD_WALL_HEIGHT = 20.f;
constexpr gfx::Color TETRIS_WALL_COLOR{34, 44, 64, 160};

constexpr gfx::Color TETRIS_BACKGROUND_COLOR{12, 16, 28, 255};

constexpr f32 TETRIS_BOARD_GROUND_SIZE = 14.f;
constexpr gfx::Color TETRIS_GROUND_COLOR{24, 30, 44, 255};
constexpr gfx::Color TETRIS_CHECKER_COLOR_A{42, 52, 72, 255};
constexpr gfx::Color TETRIS_CHECKER_COLOR_B{30, 38, 56, 255};
constexpr gfx::Color TETRIS_SHADOW_COLOR{128, 128, 128, 96};

constexpr Static_Array<gfx::Color, available_bodies.size()> TETRIS_BODY_COLORS{{
    {80, 200, 255, 255},  {255, 180, 60, 255}, {80, 255, 140, 255},  {255, 90, 120, 255},
    {180, 100, 255, 255}, {255, 230, 70, 255}, {80, 160, 255, 255},  {255, 110, 220, 255},
    {100, 240, 240, 255}, {255, 140, 80, 255}, {150, 255, 100, 255}, {220, 120, 255, 255},
}};
constexpr auto make_tetris_dimmed_body_colors() {
    constexpr f32 TETRIS_SOLID_COLOR_DIM = 0.5f;
    Static_Array<gfx::Color, available_bodies.size()> colors{{}};
    for (usize i = 0; i < colors.size; ++i) {
        colors[i] = TETRIS_BODY_COLORS[i].dim(TETRIS_SOLID_COLOR_DIM);
    }
    return colors;
}
constexpr auto TETRIS_DIMMED_BODY_COLORS = make_tetris_dimmed_body_colors();

inline Static_Array<Static_Array<gfx::Vertex, 24>, available_bodies.size()> tetris_solid_vertices;
inline Static_Array<gfx::Mesh, available_bodies.size()> tetris_solid_meshes;
inline Static_Array<gfx::Vertex, TETRIS_BOARD_VERTEX_COUNT> tetris_checkerboard_vertices;
inline Static_Array<gfx::Index, TETRIS_BOARD_INDEX_COUNT> tetris_checkerboard_indices;
inline gfx::Mesh tetris_checkerboard_mesh;

auto initialize_tetris_meshes() -> void {
    for (usize body_index = 0; body_index < tetris_solid_meshes.size; ++body_index) {
        for (usize vertex_index = 0; vertex_index < gfx::UNIT_CUBE.vertices.size; ++vertex_index) {
            tetris_solid_vertices[body_index][vertex_index] = gfx::UNIT_CUBE.vertices[vertex_index];
            tetris_solid_vertices[body_index][vertex_index].color = TETRIS_DIMMED_BODY_COLORS[body_index];
        }
        tetris_solid_meshes[body_index] = gfx::Mesh{
            tetris_solid_vertices[body_index],
            gfx::UNIT_CUBE.indices
        };
    }
}

auto initialize_tetris_board_mesh() -> void {
    for (u32 row = 0; row < TETRIS_BOARD_CELL_ROWS; ++row) {
        for (u32 col = 0; col < TETRIS_BOARD_CELL_COLS; ++col) {
            const u32 cell_index = row * TETRIS_BOARD_CELL_COLS + col;
            const u32 vertex_index = cell_index * 4;
            const u32 index_index = cell_index * 2;
            const f32 x1 = static_cast<f32>(row) - TETRIS_BOARD_CELL_ROWS / 2.f;
            const f32 x2 = x1 + 1.f;
            const f32 y1 = static_cast<f32>(col) - TETRIS_BOARD_CELL_COLS / 2.f;
            const f32 y2 = y1 + 1.f;
            const gfx::Color color = ((row + col) & 1) == 0 ? TETRIS_CHECKER_COLOR_A : TETRIS_CHECKER_COLOR_B;

            tetris_checkerboard_vertices[vertex_index + 0] = {{x1, y1, 0.f}, color, {0.f, 0.f}};
            tetris_checkerboard_vertices[vertex_index + 1] = {{x2, y1, 0.f}, color, {1.f, 0.f}};
            tetris_checkerboard_vertices[vertex_index + 2] = {{x2, y2, 0.f}, color, {1.f, 1.f}};
            tetris_checkerboard_vertices[vertex_index + 3] = {{x1, y2, 0.f}, color, {0.f, 1.f}};

            tetris_checkerboard_indices[index_index + 0] = {vertex_index + 0, vertex_index + 1, vertex_index + 2};
            tetris_checkerboard_indices[index_index + 1] = {vertex_index + 0, vertex_index + 2, vertex_index + 3};
        }
    }

    tetris_checkerboard_mesh = gfx::Mesh{tetris_checkerboard_vertices, tetris_checkerboard_indices};
}

auto update_tetris_camera(gfx::Camera3D& camera, f32 orbit_angle) -> void {
    const f32 distance = sqrt(camera.position.x * camera.position.x + camera.position.y * camera.position.y);
    camera.position.x = sin(orbit_angle) * distance;
    camera.position.y = -cos(orbit_angle) * distance;
    camera.position.z = TETRIS_ORIGIN_Z + TETRIS_CAMERA_OFFSET_Z;
    camera.rotation = Quaternion<f32>::from_axis_angle({0.f, 0.f, 1.f}, orbit_angle) *
                      Quaternion<f32>::from_axis_angle({1.f, 0.f, 0.f}, TETRIS_PI / 2.f);
    camera.recompute_matrix();
}

auto camera_relative_move_delta(f32 orbit_angle, s32 screen_x, s32 screen_y) -> Vector2<s32> {
    f32 quarter_turns = orbit_angle / (TETRIS_PI * 0.5f);
    s32 sector = static_cast<s32>(quarter_turns >= 0.f ? quarter_turns + 0.5f : quarter_turns - 0.5f);
    sector = ((sector % 4) + 4) % 4;

    constexpr Static_Array<Vector2<s32>, 4> screen_right_deltas{{
        {1, 0}, {0, 1}, {-1, 0}, {0, -1},
    }};
    constexpr Static_Array<Vector2<s32>, 4> screen_up_deltas{{
        {0, -1}, {1, 0}, {0, 1}, {-1, 0},
    }};

    const auto right = screen_right_deltas[sector];
    const auto up = screen_up_deltas[sector];
    return {
        screen_x * right.x + screen_y * up.x,
        screen_x * right.y + screen_y * up.y,
    };
}

auto tetris_position(const Game& game, u32 row, u32 col, u32 layer) -> Vector3<f32> {
    return {
        (static_cast<f32>(row) - static_cast<f32>(game.grid.rows - 1) / 2.f) * TETRIS_BLOCK_SPACING,
        (static_cast<f32>(col) - static_cast<f32>(game.grid.cols - 1) / 2.f) * TETRIS_BLOCK_SPACING,
        TETRIS_ORIGIN_Z - static_cast<f32>(layer) * TETRIS_BLOCK_SPACING,
    };
}

auto tetris_board_z(const Game& game) -> f32 {
    return tetris_position(game, 0, 0, game.grid.layers - 1).z - TETRIS_BLOCK_SCALE;
}

auto tetris_shadow_layer_offset(const Game& game) -> u32 {
    u32 shadow_offset = game.grid.layers;

    for (const auto& [row, col, layer] : game.falling_body.blocks) {
        u32 block_offset = game.grid.layers - 1 - layer;
        for (u32 offset = 1; offset <= block_offset; ++offset) {
            const u32 next_layer = layer + offset;
            if (game.grid.at(row, col, next_layer) == Block_Type::EMPTY) continue;
            if (falling_body_contains(game.falling_body, {row, col, next_layer})) continue;
            block_offset = offset - 1;
            break;
        }
        shadow_offset = std::min(shadow_offset, block_offset);
    }

    return shadow_offset;
}

auto draw_tetris_back_walls(const Game& game, gfx::Camera3D& camera) -> void {
    const bool negative_x_wall = camera.position.x >= 0.f;
    const bool positive_y_wall = camera.position.y <= 0.f;
    const f32 x_edge = static_cast<f32>(game.grid.rows) / 2.f + TETRIS_BOARD_WALL_GAP;
    const f32 y_edge = static_cast<f32>(game.grid.cols) / 2.f + TETRIS_BOARD_WALL_GAP;
    const f32 board_z = tetris_board_z(game);
    const f32 wall_z = board_z + TETRIS_BOARD_WALL_HEIGHT / 2.f;

    gfx::draw_plane(
        {negative_x_wall ? -x_edge : x_edge, 0.f, wall_z},
        {TETRIS_BOARD_WALL_HEIGHT, static_cast<f32>(game.grid.cols) + 2.f * TETRIS_BOARD_WALL_GAP},
        TETRIS_WALL_COLOR,
        camera,
        Quaternion<f32>::from_axis_angle({0.f, 1.f, 0.f}, TETRIS_PI / 2.f)
    );
    gfx::draw_plane(
        {0.f, positive_y_wall ? y_edge : -y_edge, wall_z},
        {static_cast<f32>(game.grid.rows) + 2.f * TETRIS_BOARD_WALL_GAP, TETRIS_BOARD_WALL_HEIGHT},
        TETRIS_WALL_COLOR,
        camera,
        Quaternion<f32>::from_axis_angle({1.f, 0.f, 0.f}, TETRIS_PI / 2.f)
    );
}

auto draw_tetris_board(const Game& game, gfx::Camera3D& camera) -> void {
    const f32 board_z = tetris_board_z(game);
    gfx::draw_plane(
        {0.f, 0.f, board_z - 0.02f},
        {TETRIS_BOARD_GROUND_SIZE, TETRIS_BOARD_GROUND_SIZE},
        TETRIS_GROUND_COLOR,
        camera
    );
    gfx::draw_mesh(
        gfx::Mesh_Instance{
            tetris_checkerboard_mesh,
            {},
            {0.f, 0.f, board_z},
            {},
            {1.f, 1.f, 1.f},
            gfx::WHITE,
        },
        camera
    );
    draw_tetris_back_walls(game, camera);
}

auto draw(const Game& game, gfx::Camera3D& camera) -> void {
    gfx::clear(TETRIS_BACKGROUND_COLOR);

    gfx::draw_text(8, 8, tprint("FPS: %", game.fps));
    gfx::draw_text(
        8,
        32,
        "WASD - rotate block\nArrow keys - move block\nSpace - drop\nShift - speed up falling\nESC - quit"
    );
    if (game.game_over) {
        constexpr u32 GAME_OVER_TEXT_WIDTH = 12 * font::GLYPH_WIDTH;
        const u32 game_over_x = (gfx::width() - GAME_OVER_TEXT_WIDTH) / 2;
        const u32 game_over_y = (gfx::height() - font::GLYPH_HEIGHT) / 2;
        gfx::draw_text(game_over_x, game_over_y, "GAME OVER :(", gfx::WHITE, gfx::TRANSPARENT, 2);
    }

    draw_tetris_board(game, camera);

    for (u32 row = 0; row < game.grid.rows; ++row) {
        for (u32 col = 0; col < game.grid.cols; ++col) {
            for (u32 layer = 0; layer < game.grid.layers; ++layer) {
                const auto type = game.grid.at(row, col, layer);
                if (type == Block_Type::EMPTY) continue;
                const auto body_index = game.body_indices.at(row, col, layer);
                const auto mesh     = type == Block_Type::FALLING ? gfx::UNIT_CUBE : tetris_solid_meshes[body_index];
                const auto modulate = type == Block_Type::FALLING ? TETRIS_BODY_COLORS[body_index] : gfx::WHITE;
                gfx::draw_mesh(
                gfx::Mesh_Instance{
                    mesh,
                    @embed("tetris_block.png"),
                    tetris_position(game, row, col, layer),
                    {},
                    {TETRIS_BLOCK_SCALE, TETRIS_BLOCK_SCALE, TETRIS_BLOCK_SCALE},
                    modulate
                }, camera);
            }
        }
    }

    const u32 shadow_offset = tetris_shadow_layer_offset(game);
    for (const auto& [row, col, layer] : game.falling_body.blocks) {
        gfx::draw_mesh(
            gfx::Mesh_Instance{
                gfx::UNIT_CUBE,
                {},
                tetris_position(game, row, col, layer + shadow_offset),
                {},
                {TETRIS_BLOCK_SCALE, TETRIS_BLOCK_SCALE, TETRIS_BLOCK_SCALE},
                TETRIS_SHADOW_COLOR,
            },
            camera
        );
    }

    gfx::draw_frame();
}

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
        game.body_indices.set(row, col, layer, static_cast<u8>(body_index));
    }

    game.falling_body = falling_body;
    game.falling_body_index = body_index;
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
            new_position.x =  diff_position.x;
            new_position.y = -diff_position.z;
            new_position.z =  diff_position.y;
        } break;

        case Y: {
            new_position.x =  diff_position.z;
            new_position.y =  diff_position.y;
            new_position.z = -diff_position.x;
        } break;

        case Z: {
            new_position.x = -diff_position.y;
            new_position.y =  diff_position.x;
            new_position.z =  diff_position.z;
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

auto rotate_90_doubled(Block_Coords block, Vector3<s32> center_twice, Rotation_Axis axis) -> Vector3<s32> {
    const Vector3<s32> position_twice = Vector3<s32>(block) * 2;
    const Vector3<s32> difference = position_twice - center_twice;
    Vector3<s32> rotated{};

    using enum Rotation_Axis;
    switch (axis) {
        case X: rotated = {difference.x, -difference.z, difference.y}; break;
        case Y: rotated = {difference.z, difference.y, -difference.x}; break;
        case Z: rotated = {-difference.y, difference.x, difference.z}; break;
    }

    return center_twice + rotated;
}

auto try_rotating_falling_body(Game& game, Rotation_Axis axis) -> bool {
    u32 min_row = U32_MAX, max_row = 0;
    u32 min_col = U32_MAX, max_col = 0;
    u32 min_layer = U32_MAX, max_layer = 0;
    for (const auto& [row, col, layer] : game.falling_body.blocks) {
        min_row = std::min(min_row, row);
        max_row = std::max(max_row, row);
        min_col = std::min(min_col, col);
        max_col = std::max(max_col, col);
        min_layer = std::min(min_layer, layer);
        max_layer = std::max(max_layer, layer);
    }
    const Vector3<s32> center_twice{
        static_cast<s32>(min_row + max_row),
        static_cast<s32>(min_col + max_col),
        static_cast<s32>(min_layer + max_layer),
    };

    Blocks rotated;
    for (const auto& block : game.falling_body.blocks) {
        const auto rotated_twice = rotate_90_doubled(block, center_twice, axis);
        // If any of the rotated_twice values are not even - something went wrong
        if ((rotated_twice.x & 1) != 0 || (rotated_twice.y & 1) != 0 || (rotated_twice.z & 1) != 0) return false;

        const Vector3<s32> rotated_position = rotated_twice / 2;
        if (rotated_position.x < 0 || rotated_position.y < 0 || rotated_position.z < 0) return false;

        const Block_Coords new_coords(rotated_position);
        if (new_coords.x >= game.grid.rows || new_coords.y >= game.grid.cols || new_coords.z >= game.grid.layers)
            return false;

        if (game.grid.at(new_coords.x, new_coords.y, new_coords.z) != Block_Type::EMPTY) {
            if (!falling_body_contains(game.falling_body, new_coords)) return false;
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
        game.body_indices.set(x, y, z, static_cast<u8>(game.falling_body_index));
    }

    return true;
}

auto move_falling_body_lower(Game& game) -> void {
    game.falling_body.layer_sort();
    for (auto& [row, col, layer] : game.falling_body.blocks) {
        game.grid.clear(row, col, layer);
        layer += 1;
        game.grid.set(row, col, layer, Block_Type::FALLING);
        game.body_indices.set(row, col, layer, static_cast<u8>(game.falling_body_index));
    }
}

auto try_moving_falling_body(Game& game, s32 row_delta, s32 col_delta) -> bool {
    Blocks moved;
    for (const auto& [row, col, layer] : game.falling_body.blocks) {
        const s32 new_row = static_cast<s32>(row) + row_delta;
        const s32 new_col = static_cast<s32>(col) + col_delta;
        if (new_row < 0 || new_col < 0 || new_row >= static_cast<s32>(game.grid.rows) || new_col >= static_cast<s32>(game.grid.cols))
            return false;

        const Block_Coords new_coords{
            static_cast<u32>(new_row),
            static_cast<u32>(new_col),
            layer,
        };
        if (game.grid.at(new_coords.x, new_coords.y, new_coords.z) != Block_Type::EMPTY) {
            if (!falling_body_contains(game.falling_body, new_coords)) return false;
        }
        moved.push_back(new_coords);
    }

    for (const auto& [row, col, layer] : game.falling_body.blocks) game.grid.clear(row, col, layer);
    game.falling_body.blocks = moved;
    for (const auto& [row, col, layer] : game.falling_body.blocks) {
        game.grid.set(row, col, layer, Block_Type::FALLING);
        game.body_indices.set(row, col, layer, static_cast<u8>(game.falling_body_index));
    }
    return true;
}

auto update(Game& game, f32 camera_orbit_angle) -> void {
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
                        game.body_indices.copy_layer(static_cast<u32>(read_layer), static_cast<u32>(write_layer), skip_if_is_falling);
                    }
                    --write_layer;
                }
            }

            for (s64 layer = write_layer; layer >= 0; --layer) {
                game.grid.clear_layer(static_cast<u32>(layer), skip_if_is_falling);
                game.body_indices.clear_layer(static_cast<u32>(layer), skip_if_is_falling);
            }

            game.layers_to_destroy.clear();
        }
    }

    // Handle rotations.
    {
        const bool rotate_x_key_pressed = input::key_pressed(Key::W) || input::key_pressed(Key::S);
        const bool rotate_y_key_pressed = input::key_pressed(Key::A) || input::key_pressed(Key::D);

        if (rotate_x_key_pressed) {
            try_rotating_falling_body(game, Rotation_Axis::X);
        }
        else if (rotate_y_key_pressed) {
            try_rotating_falling_body(game, Rotation_Axis::Y);
        }
    }

    // Handle movement.
    {
        if (input::key_pressed(Key::LEFT_ARROW)) {
            const auto delta = camera_relative_move_delta(camera_orbit_angle, -1, 0);
            try_moving_falling_body(game, delta.x, delta.y);
        }
        if (input::key_pressed(Key::RIGHT_ARROW)) {
            const auto delta = camera_relative_move_delta(camera_orbit_angle, 1, 0);
            try_moving_falling_body(game, delta.x, delta.y);
        }
        if (input::key_pressed(Key::UP_ARROW)) {
            const auto delta = camera_relative_move_delta(camera_orbit_angle, 0, -1);
            try_moving_falling_body(game, delta.x, delta.y);
        }
        if (input::key_pressed(Key::DOWN_ARROW)) {
            const auto delta = camera_relative_move_delta(camera_orbit_angle, 0, 1);
            try_moving_falling_body(game, delta.x, delta.y);
        }

        const bool fast_fall_key_held = input::key_held(Key::LEFT_SHIFT) || input::key_held(Key::RIGHT_SHIFT);
        const bool instant_drop_key_pressed = input::key_pressed(Key::SPACE);
        const u64 move_interval_ms = fast_fall_key_held ? TETRIS_FAST_FALL_TIME_MS : game.time_till_next_move_ms;
        const bool timer_elapsed = get_ticks() >= game.current_move_started_at_tick + ms_to_ticks(move_interval_ms);

        // If timer elapsed or a button has been pushed then move down, or if it's
        // not possible solidify into the stationary layers at the bottom.
        if (timer_elapsed || instant_drop_key_pressed) {
            bool solidify = false;
            if (instant_drop_key_pressed) {
                while (falling_body_can_go_lower(game)) move_falling_body_lower(game);
                solidify = true;
            }
            else if (falling_body_can_go_lower(game)) {
                move_falling_body_lower(game);
            }
            else {
                solidify = true;
            }

            if (solidify) {
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

                game.game_over = !produce_new_falling_body(game);
            }

            game.current_move_started_at_tick = get_ticks();
        }
    }
}

auto tetris_main() -> void {
    mem::Debug_Allocator dbg_allocator{};
    mem::set_global_allocator(&dbg_allocator);

    gfx::Camera3D camera;
    f32 camera_orbit_angle = 0.f;
    camera.position = {0.f, -TETRIS_CAMERA_RADIUS, TETRIS_ORIGIN_Z + TETRIS_CAMERA_OFFSET_Z};
    update_tetris_camera(camera, camera_orbit_angle);
    initialize_tetris_meshes();
    initialize_tetris_board_mesh();
    Game game;
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

        const auto mouse_delta = input::mouse_motion();
        if (input::mouse_button_held(input::Mouse_Button::LEFT)) {
            camera_orbit_angle += TETRIS_CAMERA_MOUSE_SENSITIVITY * static_cast<f32>(mouse_delta.x);
        }
        update_tetris_camera(camera, camera_orbit_angle);

        {
            game.dt_real  = elapsed;
            game.dt       = static_cast<f64>(game.dt_real) / TICK_RATE * game.time_scale;
            game.time_ms += ticks_to_ms(game.dt_real);
            game.fps      = 1 / game.dt;

            update(game, camera_orbit_angle);
            draw(game, camera);

            if (game.game_over) {
                while (true) {
                    input::begin_frame();
                    if (input::key_pressed(Key::ESCAPE)) break;

                    const auto mouse_delta = input::mouse_motion();
                    if (input::mouse_button_held(input::Mouse_Button::LEFT)) {
                        camera_orbit_angle += TETRIS_CAMERA_MOUSE_SENSITIVITY * static_cast<f32>(mouse_delta.x);
                    }
                    update_tetris_camera(camera, camera_orbit_angle);
                    draw(game, camera);
                    sleep_ticks(TARGET_TICKS);
                }
                break;
            }

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
