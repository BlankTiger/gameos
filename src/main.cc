#include "global_constructor_handling.hh"
#include "kstd.hh"
#include "kstd/interrupts.hh"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    serial::initialize();

    assert(magic == boot::MULTIBOOT2_MAGIC, "bad multiboot2 magic");

    serial::println("Initializing gfx");
    const auto gfx_initialized  = gfx::initialize(mbi);
    assert(gfx_initialized);

    serial::println("Initializing term");
    const auto term_initialized = term::initialize();
    assert(term_initialized);

    serial::println("Initializing mem");
    mem::initialize(mbi);

    idt::initialize();
    pic::initialize();
    time::initialize();
    idt::enable_interrupts();

    gfx::clear(gfx::BLACK);

    term::println("Hello from GameOS!");

    gfx::draw_rect(250, 250, 100, 100, gfx::BLUE);
    gfx::draw_rect(300, 300, 80, 80, gfx::Color{200, 0, 0, 128});

    gfx::draw_rect(400, 250, 100, 100, gfx::GREEN);
    gfx::draw_rect(450, 300, 80, 80, gfx::Color{0, 200, 0, 128});

    gfx::draw_rect(550, 250, 100, 100, gfx::RED);
    gfx::draw_rect(600, 300, 80, 80, gfx::Color{0, 0, 200, 128});

    gfx::draw_rect(700, 250, 100, 100, gfx::WHITE);
    gfx::draw_rect(750, 300, 80, 80, gfx::Color{230, 230, 230, 128});

    gfx::draw_rect(250, 400, 550, 300, gfx::WHITE);
    gfx::draw_circle(525, 550, 100, gfx::RED);

    gfx::draw_frame();

    term::println("ZA WARUDO");

    auto seconds = 1;
    while (true) {
        term::println("%", seconds);
        time::sleep_ms(1000);
        if (seconds == 10) break;
        ++seconds;
    }

    term::println("OWARIDA");
}
