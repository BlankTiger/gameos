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
    gfx::Mesh_Instance cube{gfx::UNIT_CUBE, {10.f, 10.f, 10.f}};
    gfx::draw_mesh(cube);
    gfx::draw_frame();
    // tetris_main();
}

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    kernel_init(magic, mbi);
    main();
}
