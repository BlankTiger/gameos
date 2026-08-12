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

    u64 last_tick = get_ticks();
    const auto* temporary_allocator_mark = mem::temporary_allocator.mark();
    while (true) {
        input::begin_frame();
        if (input::key_pressed(Key::ESCAPE)) break;

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
        gfx::draw_frame();

        mem::temporary_allocator.rewind(temporary_allocator_mark);

        const u64 frame_ticks = get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) sleep_ticks(TARGET_TICKS - frame_ticks);
    }
}
