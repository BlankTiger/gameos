#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cstddef>
#include <cstdint>

#include "kstd.hh"
#include "terminal.hh"

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

extern "C" void kernel_main(uint32_t magic, const kstd::Multiboot_Info* mbi) {
    terminal_initialize();

    if (magic != 0x2BADB002) {
        terminal_writestring("Bad multiboot magic\n");
        return;
    }

    kstd::Frame_Allocator allocator(mbi);

    for (uint32_t i = 0; i < allocator.regions_.size; i++) {
        terminal_write_hex(allocator.regions_[i].base);
        terminal_writestring(" ");
        terminal_write_hex(allocator.regions_[i].length);
        terminal_writestring("\n");
    }
}
