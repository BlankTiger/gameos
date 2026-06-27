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
        //
        term::initialize();
        term::println("Bad multiboot2 magic");
        return;
    }

    mem::initialize(mbi);
    const auto* framebuffer = mem::find_multiboot2_framebuffer_tag(mbi);
    const auto framebuffer_initialized = fb::initialize(framebuffer);
    if (framebuffer_initialized) {
        // @TODO: Timers, calculate dt, run at constant framerate.
        // while (true) {}
        fb::clear(fb::BLACK);
        fb::draw_text(10, 10, "Hello from GameOS!");
        fb::draw_text(10, 30, "Bitmap font rendering works.\nNewlines work too..", fb::GREEN);
        for (int i = 500; i < 900; i++) {
            for (int j = 200; j < 400; j++) {
                fb::set_pixel(i, j, fb::BLUE);
            }
        }
    }
}
