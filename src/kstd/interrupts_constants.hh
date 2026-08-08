#pragma once

// @TODO(blanktiger): Generate an enum out of this automatically. @enum maybe? Currently Interrupt_Vector_Type is made by hand.
#define VECTOR_DIVIDE_ERROR                  0
#define VECTOR_DEBUG                         1
#define VECTOR_NON_MASKABLE_INTERRUPT        2
#define VECTOR_BREAKPOINT                    3
#define VECTOR_OVERFLOW                      4
#define VECTOR_BOUND_RANGE_EXCEEDED          5
#define VECTOR_INVALID_OPCODE                6
#define VECTOR_DEVICE_NOT_AVAILABLE          7
#define VECTOR_DOUBLE_FAULT                  8
#define VECTOR_COPROCESSOR_SEGMENT_OVERRUN   9
#define VECTOR_INVALID_TSS                   10
#define VECTOR_SEGMENT_NOT_PRESENT           11
#define VECTOR_STACK_SEGMENT_FAULT           12
#define VECTOR_GENERAL_PROTECTION_FAULT      13
#define VECTOR_PAGE_FAULT                    14
#define VECTOR_RESERVED_15                   15
#define VECTOR_X87_FLOATING_POINT_EXCEPTION  16
#define VECTOR_ALIGNMENT_CHECK               17
#define VECTOR_MACHINE_CHECK                 18
#define VECTOR_SIMD_FLOATING_POINT_EXCEPTION 19
#define VECTOR_VIRTUALISATION_EXCEPTION      20
#define VECTOR_CONTROL_PROTECTION_EXCEPTION  21
#define VECTOR_RESERVED_22                   22
#define VECTOR_RESERVED_23                   23
#define VECTOR_RESERVED_24                   24
#define VECTOR_RESERVED_25                   25
#define VECTOR_RESERVED_26                   26
#define VECTOR_RESERVED_27                   27
#define VECTOR_HYPERVISOR_INJECTION          28
#define VECTOR_VMM_COMMUNICATION             29
#define VECTOR_SECURITY_EXCEPTION            30
#define VECTOR_RESERVED_31                   31

#define VECTOR_PIT_TIMER     32
#define VECTOR_PS2_KEYBOARD  33
#define VECTOR_CASCADE       34
#define VECTOR_COM2          35
#define VECTOR_COM1          36
#define VECTOR_LPT2          37
#define VECTOR_FLOPPY        38
#define VECTOR_LPT1_SPURIOUS 39
#define VECTOR_RTC           40
#define VECTOR_ACPI          41
#define VECTOR_FREE_IRQ10    42
#define VECTOR_FREE_IRQ11    43
#define VECTOR_PS2_MOUSE     44
#define VECTOR_FPU           45
#define VECTOR_PRIMARY_ATA   46
#define VECTOR_SECONDARY_ATA 47

#define VECTOR_LOCAL_APIC_TIMER         64
#define VECTOR_LOCAL_APIC_TLB_SHOOTDOWN 251
#define VECTOR_LOCAL_APIC_STOP          252
#define VECTOR_LOCAL_APIC_SPURIOUS      255
