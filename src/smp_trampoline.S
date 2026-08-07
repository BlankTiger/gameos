/* AP real -> Protected Mode 32 -> Long Mode 64, then call ap_main */
#include "config.hh"

.set REAL_MODE_STACK_POINTER, 0x0E00

.section .text
.global smp_trampoline_start
.global smp_trampoline_end
.global smp_trampoline_pm32
.global smp_trampoline_lm64
.global smp_trampoline_size

.macro breadcrumb offset
    movl $(TRAMPOLINE_PHYSICAL_ADDRESS + \offset), %ebx /* Has to be like this because of a change in addressing ĄĄĄą */
    movb $SMP_BREADCRUMB_OKAY_VALUE, (%ebx)
.endm

.code16
smp_trampoline_start:
    cli
    cld
    mov %cs, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $REAL_MODE_STACK_POINTER, %sp
    movb $SMP_BREADCRUMB_OKAY_VALUE, SMP_BREADCRUMB_ENTRY

    movw $SMP_OFFSET_GDTR, %bx
    data32 lgdt (%bx)
    movb $SMP_BREADCRUMB_OKAY_VALUE, SMP_BREADCRUMB_GDTR

    mov %cr0, %eax
    or  $1, %eax
    mov %eax, %cr0 /* CR0.PE = 1 */
    movb $SMP_BREADCRUMB_OKAY_VALUE, SMP_BREADCRUMB_PE

    /* Can't jump directly to label because we are in 16 bit mode now */
    data32 ljmp *%cs:SMP_OFFSET_FAR32

.code32
smp_trampoline_pm32:
    mov $SMP_SEGMENT_OFFSET_IN_GDT_PM32_DATA, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov %ax, %fs
    mov %ax, %gs
    breadcrumb SMP_BREADCRUMB_PM32

    mov $(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_CR3), %ebx
    mov (%ebx), %eax
    mov %eax, %cr3
    breadcrumb SMP_BREADCRUMB_CR3

    mov %cr4, %eax
    or  $(1 << 5), %eax /* PAE */
    mov %eax, %cr4

    mov $0xC0000080, %ecx /* EFER MSR */
    rdmsr
    or  $(1 << 8), %eax /* enable long-mode */
    wrmsr
    breadcrumb SMP_BREADCRUMB_LME

    mov %cr0, %eax
    or  $(1 << 31), %eax /* enable compatibility mode */
    mov %eax, %cr0
    breadcrumb SMP_BREADCRUMB_PG

    mov $(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_FAR64), %ebx
    ljmp *(%ebx)

.code64
smp_trampoline_lm64:
    mov $SMP_SEGMENT_OFFSET_IN_GDT_PM32_DATA, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    breadcrumb SMP_BREADCRUMB_LM64

    /* SSE init copied over from boot.S */
    mov %cr0, %rax
    and $~0x4, %rax
    or  $0x2, %rax
    or  $(1 << 5), %rax
    mov %rax, %cr0
    mov %cr4, %rax
    or  $(1 << 9), %rax
    or  $(1 << 10), %rax
    mov %rax, %cr4
    fninit

    /* Set up the APs stack */
    mov $(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_STACK), %rbx
    mov (%rbx), %rsp
    and $~AP_STACK_ALIGNMENT, %rsp /* Make sure it's correctly aligned */
    breadcrumb SMP_BREADCRUMB_STACK

    mov $(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_CORE_INFO), %rbx
    movl (%rbx), %eax  /* We take the 32 bits of Core_Info* (we can because we are below 4 GiB) */
    mov  $IA32_GS_BASE, %ecx
    xor  %edx, %edx
    wrmsr
    breadcrumb SMP_BREADCRUMB_GS

    mov $(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_CPU_INDEX), %rbx
    movl (%rbx), %edi
    breadcrumb SMP_BREADCRUMB_ENTRY64

    mov $(TRAMPOLINE_PHYSICAL_ADDRESS + SMP_OFFSET_ENTRY), %rbx
    call *(%rbx)

    cli
1:  hlt
    jmp 1b

smp_trampoline_end:
smp_trampoline_size:
    .quad smp_trampoline_end - smp_trampoline_start
