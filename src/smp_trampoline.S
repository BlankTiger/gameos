/* AP real -> Protected Mode 32 -> Long Mode 64, then call ap_main */
#include "config.hh"

.set REAL_MODE_STACK_POINTER, 0x0E00

.section .text
.global smp_trampoline_start
.global smp_trampoline_end
.global smp_trampoline_pm32
.global smp_trampoline_lm64
.global smp_trampoline_size

.code16
smp_trampoline_start:
    cli
    cld
    mov %cs, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $REAL_MODE_STACK_POINTER, %sp
    movb $67, SMP_BREADCRUMB_ENTRY

    movw $SMP_OFFSET_GDTR, %bx
    data32 lgdt (%bx)
    movb $67, SMP_BREADCRUMB_GDTR

    mov %cr0, %eax
    or  $1, %eax
    mov %eax, %cr0 /* CR0.PE = 1 */
    movb $67, SMP_BREADCRUMB_PE

    /* data32 ljmp *%cs:SMP_OFFSET_FAR32 */
1:  hlt
    jmp 1b

.code32
smp_trampoline_pm32:
    /* ... protected 32 ... */

.code64
smp_trampoline_lm64:
    call ap_main
    /* ... long 64 ... */

smp_trampoline_end:
smp_trampoline_size:
    .quad smp_trampoline_end - smp_trampoline_start
