/*
 * boot.s - Multiboot2 boot stub for GameOS (i686 / x86 32-bit).
 *
 * This file is the very first code the bootloader jumps to.  It sets up the
 * multiboot2 header so GRUB (or any multiboot2-compliant loader) knows how to
 * load us, allocates a bootstrap stack, and then hands control to the C++
 * kernel_main() function.
 */

/* Multiboot2 constants */
.set MAGIC,    0xE85250D6 /* Multiboot2 header magic number.               */
.set ARCH,     0          /* Architecture: 0 = 32-bit protected-mode i386. */

/* Multiboot2 header */
/*
 * The header must be 8-byte aligned and placed in a dedicated .multiboot
 * section so the linker script can put it near the start of the binary where
 * the bootloader will scan for it (first 32 KiB).
 *
 * Layout (per the multiboot2 spec):
 *   u32 magic
 *   u32 architecture
 *   u32 header_length
 *   u32 checksum          - must make (magic + arch + len + checksum) == 0 (mod 2^32)
 *   ... optional tags ...
 *   end tag (type 0, size 8)
 */
.section .multiboot
.align 8
header_start:
    .long MAGIC                                          /* magic                        */
    .long ARCH                                           /* architecture (i386)          */
    .long header_end - header_start                      /* total header length in bytes */
    .long -(MAGIC + ARCH + (header_end - header_start))  /* checksum                     */

    /*
     * Framebuffer tag (type 5) - asks the bootloader to set up a linear
     * framebuffer instead of (or in addition to) VGA text mode.
     *
     * Layout:
     *   u16 type   = 5   (MULTIBOOT_HEADER_TAG_FRAMEBUFFER)
     *   u16 flags  = 0   (not optional - the bootloader must honour this)
     *   u32 size   = 20  (bytes, including the type/flags/size fields)
     *   u32 width  = 1024
     *   u32 height = 768
     *   u32 depth  = 32  (bits per pixel)
     */
    .align 8
    .short 5                            /* tag type: framebuffer request      */
    .short 0                            /* flags: 0 = required                */
    .long  20                           /* tag size in bytes                  */
    .long  1024                         /* requested width  (pixels)          */
    .long  768                          /* requested height (pixels)          */
    .long  32                           /* requested depth  (bpp)             */

    /*
     * End tag (type 0) - signals the end of the multiboot2 header tag list.
     *
     * Layout:
     *   u16 type  = 0
     *   u16 flags = 0
     *   u32 size  = 8
     */
    .align 8
    .short 0                            /* tag type: end                      */
    .short 0                            /* flags                              */
    .long  8                            /* tag size                           */
header_end:

/* Bootstrap stack */
/*
 * The multiboot standard does not define the value of the stack pointer
 * register (esp) and it is up to the kernel to provide a stack.
 *
 * We allocate 16 KiB in a dedicated .bootstrap_stack section marked "nobits"
 * so the kernel binary stays small (the section occupies no space on disk -
 * only in memory once loaded).
 *
 * The stack grows downward on x86, so %esp is initialised to stack_top.
 * The System V ABI (and its de-facto extensions) require 16-byte alignment.
 */
.section .bootstrap_stack, "aw", @nobits
.align 16
stack_bottom:
    .skip 16384 /* 16 KiB */
stack_top:

/* Entry point */
/*
 * _start is the ELF entry point specified by the linker script.  The
 * bootloader jumps here after loading the kernel into memory.
 *
 * On entry the CPU is in 32-bit protected mode with:
 *   %eax = multiboot2 magic value (0x36D76289)
 *   %ebx = physical address of the multiboot2 info structure
 *   Interrupts disabled, paging disabled, A20 gate enabled.
 *
 * We must not return from _start - there is nothing to return to.
 */
.section .text
.global _start
.type _start, @function
_start:
    /* 1. Set up the stack pointer. */
    mov $stack_top, %esp

    /*
     * 2. Push kernel_main(u32 magic, const Multiboot2_Info* mbi) arguments.
     *
     *    The i386 System V calling convention passes arguments on the stack,
     *    right-to-left.  We also subtract 8 extra bytes first so that %esp
     *    is 16-byte aligned at the upcoming CALL instruction (the CALL itself
     *    pushes a 4-byte return address, making %esp 16-byte aligned inside
     *    the callee as the ABI requires).
     *
     *    Stack after the pushes (growing downward):
     *      [%esp+12]  (8 bytes padding)
     *      [%esp+4]   %ebx  - pointer to multiboot2 info struct (arg 2)
     *      [%esp+0]   %eax  - multiboot2 magic number           (arg 1)
     */
    sub $8, %esp /* alignment padding              */
    push %ebx    /* arg 2: multiboot2 info pointer */
    push %eax    /* arg 1: multiboot2 magic        */

    /*
     * 3. Call into the C++ runtime and kernel.
     *
     *    run_global_constructors - invokes all global/static C++ constructors
     *    kernel_main             - the high-level kernel entry point
     *    run_global_destructors  - invokes all global/static C++ destructors
     */
    call run_global_constructors
    call kernel_main
    call run_global_destructors

    /* Clean up the 16 bytes we pushed (8 padding + 2 * 4-byte args). */
    add $16, %esp

    /*
     * 4. Halt forever.
     *
     *    cli - clear the interrupt flag so no IRQs can wake us.
     *    hlt - halt the CPU until the next interrupt (which won't come).
     *    jmp - loop back in case a non-maskable interrupt (NMI) fires.
     */
    cli
1:  hlt
    jmp 1b

/*
 * Record the size of _start for debugging and call-tracing purposes.
 */
.size _start, . - _start
