#include "kstd/startup.hh"
#include "tetris/main.hh"
#include "gfx_tests/cubes_main.hh"

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    kernel_startup(magic, mbi);
    cubes_main();
    tetris_main();
}
