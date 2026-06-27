#include "global_constructor_handling.hh"
#include "kstd.hh"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

extern "C" auto kernel_main(u32 magic, const mem::Multiboot2_Info* mbi) -> void {
    if (magic != mem::MULTIBOOT2_MAGIC) {
        //
        // Not multiboot2 but try to show anything anyway if possible...
        // This might not work completely or might(?) be dangerous depending on the term backend.
        //
        const auto ok = term::initialize(mbi);
        (void)ok;
        term::println("Bad multiboot2 magic");
        return;
    }

    mem::initialize(mbi);
    const auto term_initialized = term::initialize(mbi);
    assert(term_initialized);

    const auto gfx_initialized = gfx::initialize(mbi);
    assert(gfx_initialized);

    // @TODO: Timers, calculate dt, run at constant framerate.
    // while (true) {}
    gfx::clear(gfx::BLACK);

    term::println("Hello from GameOS!");
    term::println(
        "Bitmap font rendering works. Newlines work too..Newlines work too..Newlines work too..Newlines work "
        "too..Newlines work too..Newlines work too..Newlines work too.. %",
        5);
    term::println(
        "Bitmap font rendering works. Newlines work too..Newlines work too..Newlines work too..Newlines work "
        "too..Newlines work too..Newlines work too..Newlines work too..");

    gfx::draw_rect(250, 250, 100, 100, gfx::RED);
    gfx::draw_rect(400, 250, 100, 100, gfx::BLUE);
    gfx::draw_rect(550, 250, 100, 100, gfx::GREEN);
    gfx::draw_rect(700, 250, 100, 100, gfx::WHITE);
}
