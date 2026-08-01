#include "kstd/startup.hh"
#include "tetris/main.hh"

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    kernel_startup(magic, mbi);
    tetris_main();
}
