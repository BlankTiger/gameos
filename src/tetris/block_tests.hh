#pragma once

#include "kstd/string_builder.hh"

#include "gameos/input.hh"
#include "gameos/gfx.hh"
#include "gameos/time.hh"

#include "tetris/main.hh"

constexpr usize BLOCK_TEST_BODY_COUNT = available_bodies.size();
constexpr usize BLOCK_TEST_MAX_BLOCKS = ARBITRARY_FALLING_BODY_BLOCK_SIZE_LIMIT;
constexpr usize BLOCK_TEST_MAX_VERTICES = BLOCK_TEST_MAX_BLOCKS * gfx::unit_cube_vertices.size;
constexpr usize BLOCK_TEST_MAX_INDICES = BLOCK_TEST_MAX_BLOCKS * gfx::unit_cube_indices.size;
constexpr f32 BLOCK_TEST_CUBE_SCALE = 0.65f;
constexpr f32 BLOCK_TEST_CUBE_GAP = 0.1f;
constexpr f32 BLOCK_TEST_BLOCK_SPACING = 2.f + BLOCK_TEST_CUBE_GAP / BLOCK_TEST_CUBE_SCALE;

struct Block_Test_Mesh_Storage {
    Static_Array<gfx::Vertex, BLOCK_TEST_MAX_VERTICES> vertices;
    Static_Array<gfx::Index, BLOCK_TEST_MAX_INDICES> indices;
};

inline Static_Array<Block_Test_Mesh_Storage, BLOCK_TEST_BODY_COUNT> block_test_mesh_storage;
inline Static_Array<gfx::Mesh, BLOCK_TEST_BODY_COUNT> block_test_meshes;
inline bool block_test_meshes_initialized = false;

auto make_block_test_meshes() -> Static_Array<gfx::Mesh, BLOCK_TEST_BODY_COUNT>& {
    if (block_test_meshes_initialized) return block_test_meshes;
    block_test_meshes_initialized = true;

    usize body_index = 0;
    for (const auto& body : available_bodies) {
        auto& body_storage = block_test_mesh_storage[body_index];
        usize vertex_count = 0;
        usize index_count = 0;

        const auto center = calculate_center_of_body(create_new_falling_body(body));
        for (const auto& [row, col, layer] : body) {
            const Vector3<f32> offset{
                static_cast<f32>(row) - static_cast<f32>(center.x),
                static_cast<f32>(col) - static_cast<f32>(center.y),
                static_cast<f32>(layer) - static_cast<f32>(center.z),
            };

            const usize vertex_offset = vertex_count;
            for (const auto& source_vertex : gfx::DEBUG_CUBE.vertices) {
                auto vertex = source_vertex;
                vertex.position += offset * BLOCK_TEST_BLOCK_SPACING;
                body_storage.vertices[vertex_count++] = vertex;
            }

            for (const auto& [i1, i2, i3] : gfx::DEBUG_CUBE.indices) {
                body_storage.indices[index_count++] = {
                    static_cast<u32>(vertex_offset + i1),
                    static_cast<u32>(vertex_offset + i2),
                    static_cast<u32>(vertex_offset + i3),
                };
            }
        }

        block_test_meshes[body_index] = gfx::Mesh{
            Array_View<gfx::Vertex>{vertex_count, body_storage.vertices.data},
            Array_View<gfx::Index>{index_count, body_storage.indices.data},
        };
        ++body_index;
    }

    return block_test_meshes;
}

auto block_test_initial_rotation() -> Quaternion<f32> {
    return Quaternion<f32>::from_axis_angle({0.f, 1.f, 0.f}, 0.6f) *
           Quaternion<f32>::from_axis_angle({1.f, 0.f, 0.f}, 0.4f);
}

auto blocks_tests_main() -> void {
    using namespace ktime;
    using namespace math;

    gfx::Camera3D camera;
    auto& meshes = make_block_test_meshes();

    constexpr u64 BODY_DISPLAY_MS = 5'000;
    constexpr f32 MOUSE_ROTATION_SENSITIVITY = 0.01f;
    constexpr auto TARGET_TICKS = ticks_per_frame(144);

    usize body_index = 0;
    u64 last_tick = get_ticks();
    u64 body_started_at = last_tick;
    gfx::Mesh_Instance body_instance{
        meshes[body_index],
        {0.f, 0.f, -6.f},
        block_test_initial_rotation(),
        {BLOCK_TEST_CUBE_SCALE, BLOCK_TEST_CUBE_SCALE, BLOCK_TEST_CUBE_SCALE},
    };

    const auto* temporary_allocator_mark = mem::temporary_allocator.mark();
    while (true) {
        input::begin_frame();
        if (input::key_pressed(input::Key::ESCAPE)) break;

        const u64 frame_start = get_ticks();
        const u64 elapsed = frame_start - last_tick;
        last_tick = frame_start;
        const auto mouse_delta = input::mouse_motion();

        if (frame_start >= body_started_at + ms_to_ticks(BODY_DISPLAY_MS)) {
            body_index = (body_index + 1) % BLOCK_TEST_BODY_COUNT;
            body_started_at = frame_start;
            body_instance = gfx::Mesh_Instance{
                meshes[body_index],
                {0.f, 0.f, -6.f},
                block_test_initial_rotation(),
                {BLOCK_TEST_CUBE_SCALE, BLOCK_TEST_CUBE_SCALE, BLOCK_TEST_CUBE_SCALE},
            };
        }

        if (input::mouse_button_held(input::Mouse_Button::LEFT)) {
            body_instance.rotate({0.f, 1.f, 0.f}, MOUSE_ROTATION_SENSITIVITY * static_cast<f32>(mouse_delta.x));
            body_instance.rotate({1.f, 0.f, 0.f}, MOUSE_ROTATION_SENSITIVITY * static_cast<f32>(mouse_delta.y));
        }

        const f64 dt = static_cast<f64>(elapsed) / TICK_RATE;
        const f64 fps = dt > 0.0 ? 1.0 / dt : 0.0;

        gfx::clear(gfx::BLACK);
        gfx::draw_text(8, 8, tprint("FPS: %", fps));
        gfx::draw_text(8, 28, tprint("Body: % / %", body_index + 1, BLOCK_TEST_BODY_COUNT));
        gfx::draw_mesh(body_instance, camera);
        gfx::draw_frame();

        mem::temporary_allocator.rewind(temporary_allocator_mark);

        const u64 frame_ticks = get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) sleep_ticks(TARGET_TICKS - frame_ticks);
    }
}
