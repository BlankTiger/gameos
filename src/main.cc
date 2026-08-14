#include "gameos/startup.hh"
#include "demos/cubes_main.hh"
#include "demos/stack_trace_unwinding.hh"
#include "tetris/main.hh"
#include "tetris/block_tests.hh"

extern "C" auto kernel_main(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    kernel_startup(magic, mbi);
    stack_trace_main();
    // cubes_main();
    // tetris_main();
    // blocks_tests_main();
}
