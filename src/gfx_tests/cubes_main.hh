#include "kstd/input.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/random.hh"
#include "kstd/string_builder.hh"

using Key = input::Key;

auto cubes_main() -> void {
    using namespace ktime;
    using namespace math;

    gfx::Camera3D camera;

    constexpr auto FPS_MAX      = 280;
    constexpr auto TARGET_TICKS = ticks_per_frame(FPS_MAX);
    constexpr f32  RAD_PER_SEC  = 1.2f; // full spin ~5s
    constexpr Vector3<f32> SPIN_AXIS{0.6f, 1.f, 0.3f}; // tumble: all 6 faces show

    gfx::Mesh_Instance cube{
        gfx::UNIT_CUBE,
        {-2.f, 0.f, -5.f},
        Quaternion<f32>::identity(),
    };

    gfx::Mesh_Instance cool_cube{
        gfx::UNIT_CUBE,
        @embed("cool.png"),
        {0.f, 2.f, -5.f},
        Quaternion<f32>::identity(),
    };

    gfx::Mesh_Instance wireframe_cube{
        gfx::UNIT_CUBE,
        {2.f, 0.f, -10.f},
        Quaternion<f32>::identity(),
    };

    const Resource_View cursor = @embed("cursor.png");
    constexpr u32 CURSOR_SCALE = 5;
    const u32 cursor_width = cursor.width / CURSOR_SCALE;
    const u32 cursor_height = cursor.height / CURSOR_SCALE;
    s32 cursor_x = 0;
    s32 cursor_y = 0;
    gfx::Mesh_Instance* dragged_cube = nullptr;

    u64 last_tick = get_ticks();
    const auto* temporary_allocator_mark = mem::temporary_allocator.mark();
    while (true) {
        input::begin_frame();
        if (input::key_pressed(Key::ESCAPE)) break;

        const auto mouse_delta = input::mouse_motion();
        cursor_x += mouse_delta.x;
        cursor_y += mouse_delta.y;
        cursor_x = std::clamp(cursor_x, 0, static_cast<s32>(gfx::width() - cursor_width));
        cursor_y = std::clamp(cursor_y, 0, static_cast<s32>(gfx::height() - cursor_height));

        if (input::mouse_button_pressed(input::Mouse_Button::LEFT)) {
            dragged_cube = nullptr;
            f32 dragged_depth = 2.f;
            for (auto* candidate: {&cube, &cool_cube, &wireframe_cube}) {
                auto projected = gfx::project(*candidate, camera);
                f32 min_x = static_cast<f32>(gfx::width());
                f32 min_y = static_cast<f32>(gfx::height());
                f32 max_x = 0.f;
                f32 max_y = 0.f;
                f32 depth = 2.f;
                for (const auto& vertex: projected) {
                    if (vertex.w <= 0.f) continue;
                    min_x = std::min(min_x, vertex.x);
                    min_y = std::min(min_y, vertex.y);
                    max_x = std::max(max_x, vertex.x);
                    max_y = std::max(max_y, vertex.y);
                    depth = std::min(depth, vertex.z);
                }

                const bool pointer_inside = static_cast<f32>(cursor_x) >= min_x &&
                    static_cast<f32>(cursor_x) <= max_x &&
                    static_cast<f32>(cursor_y) >= min_y &&
                    static_cast<f32>(cursor_y) <= max_y;
                if (pointer_inside && depth < dragged_depth) {
                    dragged_cube = candidate;
                    dragged_depth = depth;
                }
            }
        }

        if (!input::mouse_button_held(input::Mouse_Button::LEFT)) {
            dragged_cube = nullptr;
        } else if (dragged_cube != nullptr) {
            const f32 depth = -dragged_cube->translation.z;
            const f32 aspect_ratio = static_cast<f32>(gfx::width()) / static_cast<f32>(gfx::height());
            dragged_cube->translation.x += 2.f * depth * aspect_ratio * static_cast<f32>(mouse_delta.x) / static_cast<f32>(gfx::width());
            dragged_cube->translation.y -= 2.f * depth * static_cast<f32>(mouse_delta.y) / static_cast<f32>(gfx::height());
            dragged_cube->recompute_matrix();
        }

        const u64 frame_start = get_ticks();
        const u64 elapsed     = frame_start - last_tick;
        last_tick = frame_start;
        const f64 dt  = static_cast<f64>(elapsed) / TICK_RATE;
        const f64 fps = dt > 0.0 ? 1.0 / dt : 0.0;

        const f32 angle = RAD_PER_SEC * static_cast<f32>(elapsed) / static_cast<f32>(TICK_RATE);

        cube.rotate(SPIN_AXIS, angle);
        cool_cube.rotate(SPIN_AXIS, angle);
        wireframe_cube.rotate(SPIN_AXIS, angle);

        gfx::clear(gfx::BLACK);
        gfx::draw_text(8, 8, tprint("FPS: %", fps));
        gfx::draw_mesh(cube, camera);
        gfx::draw_mesh(cool_cube, camera);
        gfx::draw_wireframe(wireframe_cube, camera);
        gfx::draw_sprite_scaled(cursor, static_cast<u32>(cursor_x), static_cast<u32>(cursor_y), cursor_width, cursor_height);
        gfx::draw_frame();

        mem::temporary_allocator.rewind(temporary_allocator_mark);

        const u64 frame_ticks = get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) sleep_ticks(TARGET_TICKS - frame_ticks);
    }
}
