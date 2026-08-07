/*
 * boot.S - Multiboot2 boot stub for GameOS (x86_64 long mode).
 *
 * GRUB hands off in 32-bit protected mode (Multiboot2 ARCH=0). This stub
 * builds an identity map of the low 4 GiB with 4 KiB pages, enables
 * Physical Address Extension (PAE) + long mode + paging, far-jumps into a
 * 64-bit code segment (CS), then calls kernel_main(magic, mbi).
 */

#include "config.hh"

/* Multiboot2 constants */
.set MAGIC,    0xE85250D6 /* Multiboot2 header magic number.               */
.set ARCH,     0          /* Architecture: 0 = 32-bit protected-mode i386. */

/* Page-table entry (PTE) flags */
.set PTE_PRESENT,  (1 << 0)
.set PTE_WRITABLE, (1 << 1)
.set PTE_TABLE,    (PTE_PRESENT | PTE_WRITABLE)
.set PTE_PAGE_4K,  (PTE_PRESENT | PTE_WRITABLE)

/* Multiboot2 header */
/*
 * The header must be 8-byte aligned and placed in a dedicated .multiboot
 * section so the linker script can put it near the start of the binary where
 * the bootloader will scan for it (first 32 KiB).
 *
 * Layout (per the Multiboot2 spec):
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
    .long ARCH                                           /* architecture (i386 handoff)  */
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
     *   u32 width  = GFX_WIDTH
     *   u32 height = GFX_HEIGHT
     *   u32 depth  = 32  (bits per pixel)
     */
    .align 8
    .short 5                            /* tag type: framebuffer request      */
    .short 0                            /* flags: 0 = required                */
    .long  20                           /* tag size in bytes                  */
    .long  GFX_WIDTH                    /* requested width  (pixels)          */
    .long  GFX_HEIGHT                   /* requested height (pixels)          */
    .long  32                           /* requested depth  (bits per pixel)  */

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
 * Multiboot does not define %esp/%rsp. We allocate 8 MiB in a dedicated
 * .bootstrap_stack section marked "nobits" so the kernel binary stays small.
 * Stack grows downward; System V AMD64 wants 16-byte alignment at call sites.
 */
.section .bootstrap_stack, "aw", @nobits
.align 16
.global stack_bottom
stack_bottom:
    .skip 8388608 /* 8 MiB */
.global stack_top
stack_top:

/*
 * Early identity page tables (4-level). Cover low 4 GiB with 4 KiB pages:
 *   1 Page Map Level 4 (PML4)
 *   -> 1 Page Directory Pointer Table (PDPT)
 *   -> 4 Page Directories (PD)
 *   -> 2048 Page Tables (PT)
 * Each PT covers 2 MiB. Each PD covers 1 GiB.
 * Identity: virtual address V maps to physical address V. Covers kernel at
 * 2 MiB, stack, Multiboot, typical QEMU RAM, and common framebuffer
 * placements under 4 GiB.
 */
.section .bss
.align 4096
boot_pml4:
    .skip 4096
boot_pdpt:
    .skip 4096
boot_pd0:
    .skip 4096 * 4   /* 4 contiguous page directories */
boot_pt0:
    .skip 4096 * 2048 /* 2048 contiguous page tables (8 MiB) */

.section .rodata
.align 8
.global boot_pml4_physical_address
boot_pml4_physical_address:
    .quad boot_pml4

/*
 * Temporary Global Descriptor Table (GDT):
 *   null descriptor
 *   | 64-bit kernel code at selector 0x08
 *   | kernel data at selector 0x10
 */
.section .rodata
.align 16
gdt64:
    .quad 0x0000000000000000 /* 0x00: null                                   */
    .quad 0x00AF9A000000FFFF /* 0x08: 64-bit code, L=1 D=0, present DPL0 S code RX */
    .quad 0x00CF92000000FFFF /* 0x10: data, present DPL0 S data RW, 32-bit gran    */
gdt64_end:

.align 8
gdt64_ptr:
    .word gdt64_end - gdt64 - 1
    .long gdt64

/* Entry point */
/*
 * _start is the ELF entry. Bootloader jumps here after loading the kernel.
 *
 * On entry the CPU is in 32-bit protected mode with:
 *   %eax = multiboot2 magic value (0x36D76289)
 *   %ebx = physical address of the multiboot2 info structure
 *   Interrupts disabled, paging disabled, A20 gate enabled.
 *
 * We must not return from _start - there is nothing to return to.
 */
.section .text
.code32
.global _start
.type _start, @function
_start:
    /* Save Multiboot args in regs the fill loops will not touch. */
    mov %eax, %ebp          /* magic */
    mov %ebx, %esi          /* Multiboot2 info (mbi) */

    /*
     * Identity-map 0 .. 4 GiB with 4 KiB pages.
     *
     * Page tables sit contiguously at boot_pt0. Entry j of page table i:
     *   physical address = i * 2 MiB + j * 4 KiB
     *   flags = present | writable (0x03)
     * Page directories sit contiguously at boot_pd0. Each page-directory
     * entry points to one page table.
     */
    mov $boot_pt0, %ebx
    xor %eax, %eax          /* physical base of current 4 KiB page */
1:
    mov %eax, %edx
    or  $PTE_PAGE_4K, %edx
    mov %edx, (%ebx)
    movl $0, 4(%ebx)
    add $0x1000, %eax       /* +4 KiB */
    add $8, %ebx
    cmp $(boot_pt0 + 4096 * 2048), %ebx
    jb 1b

    /* Page-directory entries -> physical(boot_pt_i) | present|writable ; 4 PDs * 512 entries */
    mov $boot_pd0, %ebx
    mov $boot_pt0, %eax
    or  $PTE_TABLE, %eax
    mov $(512 * 4), %ecx
2:
    mov %eax, (%ebx)
    movl $0, 4(%ebx)
    add $4096, %eax         /* next page table (contiguous) */
    add $8, %ebx
    loop 2b

    /* Page Directory Pointer Table[i] = physical(boot_pd_i) | present|writable */
    mov $boot_pdpt, %ebx
    mov $boot_pd0, %eax
    or  $PTE_TABLE, %eax
    mov $4, %ecx
3:
    mov %eax, (%ebx)
    movl $0, 4(%ebx)
    add $4096, %eax         /* next page directory (contiguous) */
    add $8, %ebx
    loop 3b

    /* Page Map Level 4[0] = physical(boot_pdpt) | present|writable ; rest stay zero from BSS */
    mov $boot_pdpt, %eax
    or  $PTE_TABLE, %eax
    mov %eax, boot_pml4
    movl $0, boot_pml4 + 4

    /* Load GDT with 64-bit code segment before the far jump. */
    lgdt gdt64_ptr

    /* Control Register 4 bit PAE = 1 (required for long-mode paging). */
    mov %cr4, %eax
    or  $(1 << 5), %eax
    mov %eax, %cr4

    /* IA32_EFER.LME = 1 (Extended Feature Enable Register, Long Mode Enable, bit 8). */
    mov $0xC0000080, %ecx
    rdmsr
    or  $(1 << 8), %eax
    wrmsr

    /* Control Register 3 = physical address of PML4 (identity while paging still off). */
    mov $boot_pml4, %eax
    mov %eax, %cr3

    /* Control Register 0 bit PG = 1 -> compatibility mode (still 32-bit code, paging on). */
    mov %cr0, %eax
    or  $(1 << 31), %eax
    mov %eax, %cr0

    /* Far jump into 64-bit code segment (selector 0x08). */
    ljmp $0x08, $long_mode_entry

.code64
long_mode_entry:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs

    mov $stack_top, %rsp

    /*
     * Enable Streaming SIMD Extensions (SSE) so freestanding x86_64 codegen
     * (and graphics math) can use XMM registers.
     * Control Register 0: clear EM, set MP + NE.
     * Control Register 4: OSFXSR + OSXMMEXCPT.
     */
    mov %cr0, %rax
    and $~0x4, %rax         /* EM (Emulation) = 0 */
    or  $0x2, %rax          /* MP (Monitor Coprocessor) = 1 */
    or  $(1 << 5), %rax     /* NE (Numeric Error) = 1 */
    mov %rax, %cr0

    mov %cr4, %rax
    or  $(1 << 9), %rax     /* OSFXSR: OS supports FXSAVE/FXRSTOR */
    or  $(1 << 10), %rax    /* OSXMMEXCPT: OS supports unmasked SIMD FP exceptions */
    mov %rax, %cr4

    fninit

    /* AMD64 System V ABI: arg1 = %rdi (magic), arg2 = %rsi (mbi). */
    mov %ebp, %edi          /* zero-extend magic into %rdi */
    mov %esi, %esi          /* zero-extend Multiboot2 info into %rsi */

    call kernel_main
    call run_global_destructors

    cli
1:  hlt
    jmp 1b

.size _start, . - _start
