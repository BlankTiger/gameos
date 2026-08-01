#include "tetris/main.hh"
#include "global_constructor_handling.hh"

#include "kstd/assert.hh"
#include "kstd/interrupts.hh"
#include "kstd/memory.hh"
#include "kstd/multiboot2.hh"
#include "kstd/programmable_interrupt_controller.hh"
#include "kstd/ps2.hh"
#include "kstd/term.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/serial_format.hh"
#include "kstd/random.hh"

#if !defined(__x86_64__)
#error "This kernel needs an x86_64-elf compiler"
#endif

auto kernel_init(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    serial::initialize();

    kstd_assert(magic == boot::MULTIBOOT2_MAGIC, "bad multiboot2 magic");

    serial::println("Initializing mem");
    mem::initialize(mbi);

    // Global constructors are called here, after the allocator is live, so any
    // constructor that calls operator new has a valid global allocator (must
    // be called after mem::initialize).
    run_global_constructors();

    serial::println("Initializing gfx");
    const auto gfx_initialized = gfx::initialize(mbi);
    kstd_assert(gfx_initialized);

    serial::println("Initializing term");
    const auto term_initialized = term::initialize();
    kstd_assert(term_initialized);

    idt::initialize();
    pic::initialize();
    ktime::initialize();
    ps2::initialize();
    idt::enable_interrupts();

    krand::initialize();
}

auto main() -> void {
    term::println("Hello from GameOS!");

    using namespace ktime;
    using namespace math;

    constexpr auto TARGET_TICKS = ticks_per_frame(60);
    constexpr f32  RAD_PER_SEC  = 1.2f; // full spin ~5s
    constexpr Vector3<f32> SPIN_AXIS{0.6f, 1.f, 0.3f}; // tumble: all 6 faces show

    gfx::Mesh_Instance cube{
        gfx::UNIT_CUBE,
        {0.f, 0.f, -5.f},
        Quaternion<f32>::identity(),
    };
    cube.texture = @embed("obamium2.png");

    u64 last_tick = get_ticks();
    const auto* temporary_allocator_mark = mem::temporary_allocator.mark();
    while (!ps2::is_pressed(ps2::Scancode::ESCAPE)) {
        const u64 frame_start = get_ticks();
        const u64 elapsed     = frame_start - last_tick;
        last_tick = frame_start;

        const f32 angle = RAD_PER_SEC * static_cast<f32>(elapsed) / static_cast<f32>(TICK_RATE);
        cube.rotation = Quaternion<f32>::from_axis_angle(SPIN_AXIS, angle) * cube.rotation;
        cube.recompute_matrix();

        gfx::clear(gfx::BLACK);
        gfx::draw_mesh(cube);
        gfx::draw_frame();

        mem::temporary_allocator.rewind(temporary_allocator_mark);

        const u64 frame_ticks = get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) sleep_ticks(TARGET_TICKS - frame_ticks);
    }
    // tetris_main();
}

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    kernel_init(magic, mbi);
    main();
}
