#include "gameos/startup.hh"
#include "demos/cubes_main.hh"
#include "tetris/main.hh"

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    kernel_startup(magic, mbi);
    cubes_main();
    tetris_main();
}
