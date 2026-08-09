#include "smp_constants.hh"

.global threads_context_switch
.type threads_context_switch, @function

threads_context_switch:
    /* rdi = previous */
    /* rsi = next     */

    movq %rbx, CONTEXT_SWITCH_OFFSET_RBX(%rdi)
    movq %rbp, CONTEXT_SWITCH_OFFSET_RBP(%rdi)
    movq %r12, CONTEXT_SWITCH_OFFSET_R12(%rdi)
    movq %r13, CONTEXT_SWITCH_OFFSET_R13(%rdi)
    movq %r14, CONTEXT_SWITCH_OFFSET_R14(%rdi)
    movq %r15, CONTEXT_SWITCH_OFFSET_R15(%rdi)
    movq %rsp, CONTEXT_SWITCH_OFFSET_RSP(%rdi)

    leaq .Lresume(%rip), %rax
    movq %rax, CONTEXT_SWITCH_OFFSET_RIP(%rdi)

    movq CONTEXT_SWITCH_OFFSET_RBX(%rsi), %rbx
    movq CONTEXT_SWITCH_OFFSET_RBP(%rsi), %rbp
    movq CONTEXT_SWITCH_OFFSET_R12(%rsi), %r12
    movq CONTEXT_SWITCH_OFFSET_R13(%rsi), %r13
    movq CONTEXT_SWITCH_OFFSET_R14(%rsi), %r14
    movq CONTEXT_SWITCH_OFFSET_R15(%rsi), %r15
    movq CONTEXT_SWITCH_OFFSET_RSP(%rsi), %rsp

    jmp *CONTEXT_SWITCH_OFFSET_RIP(%rsi)

.Lresume:
    ret
