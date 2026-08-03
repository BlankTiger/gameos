#include "kstd/ps2.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/random.hh"

auto cubes_main() -> void {
    using namespace ktime;
    using namespace math;

    gfx::Camera3D camera;

    constexpr auto TARGET_TICKS = ticks_per_frame(60);
    constexpr f32  RAD_PER_SEC  = 1.2f; // full spin ~5s
    constexpr Vector3<f32> SPIN_AXIS{0.6f, 1.f, 0.3f}; // tumble: all 6 faces show

    gfx::Mesh_Instance cube{
        gfx::UNIT_CUBE,
        {-2.f, 0.f, -5.f},
        Quaternion<f32>::identity(),
    };

    gfx::Mesh_Instance wireframe_cube{
        gfx::UNIT_CUBE,
        {2.f, 0.f, -10.f},
        Quaternion<f32>::identity(),
    };

    u64 last_tick = get_ticks();
    const auto* temporary_allocator_mark = mem::temporary_allocator.mark();
    while (!ps2::is_pressed(ps2::Scancode::ESCAPE)) {
        const u64 frame_start = get_ticks();
        const u64 elapsed     = frame_start - last_tick;
        last_tick = frame_start;

        const f32 angle = RAD_PER_SEC * static_cast<f32>(elapsed) / static_cast<f32>(TICK_RATE);
        cube.rotation = Quaternion<f32>::from_axis_angle(SPIN_AXIS, angle) * cube.rotation;
        cube.recompute_matrix();

        wireframe_cube.rotation = Quaternion<f32>::from_axis_angle(SPIN_AXIS, angle) * wireframe_cube.rotation;
        wireframe_cube.recompute_matrix();

        gfx::clear(gfx::BLACK);
        gfx::draw_mesh(cube, camera);
        gfx::draw_wireframe(wireframe_cube, camera);
        gfx::draw_frame();

        mem::temporary_allocator.rewind(temporary_allocator_mark);

        const u64 frame_ticks = get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) sleep_ticks(TARGET_TICKS - frame_ticks);
    }
}
