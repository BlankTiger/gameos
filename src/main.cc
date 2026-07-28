#include "game.hh"
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
#include "kstd/serial.hh"
#include "kstd/random.hh"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

auto kernel_init(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    serial::initialize();

    kstd_assert(magic == boot::MULTIBOOT2_MAGIC, "bad multiboot2 magic");

    serial::println("Initializing mem");
    mem::initialize(mbi);

    // Global constructors are called here, after the allocator is live, so any
    // constructor that calls operator new has a valid global_allocator (must
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
    time::initialize();
    ps2::initialize();
    idt::enable_interrupts();

    rand::initialize();
}

auto main() -> void {
    term::println("Hello from GameOS!");
    game_main();
}

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    kernel_init(magic, mbi);
    main();
}
