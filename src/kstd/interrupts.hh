#pragma once

namespace idt {

union Gate_Type_Attributes {
    struct {
        u8 gate_type                  : 4;
        u8 storage                    : 1;
        u8 descriptor_privilege_level : 2;
        u8 present                    : 1;
    };
    u8 raw;
} __attribute__((packed));

static_assert(sizeof(Gate_Type_Attributes) == 1);

constexpr Gate_Type_Attributes GATE_PRESENT_RING0_INT32 = {
    .gate_type                  = 0xE,
    .storage                    = 0,
    .descriptor_privilege_level = 0, // ring-0, kernel
    .present                    = 1,
};
constexpr u16 KERNEL_CODE_SEGMENT = 0x10;

struct Gate {
    u16                  handler_address_low;
    u16                  selector = KERNEL_CODE_SEGMENT;
    u8                   zero     = 0;
    Gate_Type_Attributes type     = GATE_PRESENT_RING0_INT32;
    u16                  handler_address_high;
} __attribute__((packed));

static_assert(sizeof(Gate) == 8);

struct Interrupt_Descriptor_Table_Register {
    u16 limit;
    u32 base;
} __attribute__((packed));

static_assert(sizeof(Interrupt_Descriptor_Table_Register) == 6);

constexpr auto NUM_VECTORS = 256;
inline Static_Array<Gate, NUM_VECTORS>     table;
inline Interrupt_Descriptor_Table_Register interrupt_descriptor_table_register;

enum struct Interrupt_Vector_Type : u8 {
    // CPU exceptions (vectors 0–31)
    DIVIDE_ERROR                  = 0,
    DEBUG                         = 1,
    NON_MASKABLE_INTERRUPT        = 2,
    BREAKPOINT                    = 3,
    OVERFLOW                      = 4,
    BOUND_RANGE_EXCEEDED          = 5,
    INVALID_OPCODE                = 6,
    DEVICE_NOT_AVAILABLE          = 7,
    DOUBLE_FAULT                  = 8,
    COPROCESSOR_SEGMENT_OVERRUN   = 9,
    INVALID_TSS                   = 10,
    SEGMENT_NOT_PRESENT           = 11,
    STACK_SEGMENT_FAULT           = 12,
    GENERAL_PROTECTION_FAULT      = 13,
    PAGE_FAULT                    = 14,
    RESERVED_15                   = 15,
    X87_FLOATING_POINT_EXCEPTION  = 16,
    ALIGNMENT_CHECK               = 17,
    MACHINE_CHECK                 = 18,
    SIMD_FLOATING_POINT_EXCEPTION = 19,
    VIRTUALISATION_EXCEPTION      = 20,
    CONTROL_PROTECTION_EXCEPTION  = 21,
    RESERVED_22                   = 22,
    RESERVED_23                   = 23,
    RESERVED_24                   = 24,
    RESERVED_25                   = 25,
    RESERVED_26                   = 26,
    RESERVED_27                   = 27,
    HYPERVISOR_INJECTION          = 28,
    VMM_COMMUNICATION             = 29,
    SECURITY_EXCEPTION            = 30,
    RESERVED_31                   = 31,

    // Hardware IRQs (vectors 32–47, after PIC remapping)
    PIT_TIMER                     = 32,
    PS2_KEYBOARD                  = 33,
    CASCADE                       = 34,
    COM2                          = 35,
    COM1                          = 36,
    LPT2                          = 37,
    FLOPPY                        = 38,
    LPT1_SPURIOUS                 = 39,
    RTC                           = 40,
    ACPI                          = 41,
    FREE_IRQ10                    = 42,
    FREE_IRQ11                    = 43,
    PS2_MOUSE                     = 44,
    FPU                           = 45,
    PRIMARY_ATA                   = 46,
    SECONDARY_ATA                 = 47,
};

auto initialize() -> void {}

#define ISR_NO_ERROR_CODE(NAME, NUMBER)                                                                         \
    __attribute__((naked)) auto _isr_handle_##NAME() -> void {                                                  \
        asm volatile(                                                                                           \
            "push $0\n\t"             /* dummy error code to make the stack uniform with `isr_handler_error` */ \
            "push $" #NUMBER "\n\t"   /* vector number                                                       */ \
            "pusha\n\t"               /* save cpu registers                                                  */ \
            "push %esp\n\t"           /* pass pointer to the saved registers to your dispatcher              */ \
                                                                                                                \
            "call isr_dispatch\n\t"                                                                             \
                                                                                                                \
            "add $4, %esp\n\t"  /* pop the pointer argument */                                                  \
            "popa\n\t"          /* restore registers        */                                                  \
            "add $8, %esp\n\t"  /* skip vector + error_code */                                                  \
            "iret\n\t"          /* restore eip, cs, eflags  */                                                  \
        );                                                                                                      \
    }

#define ISR_ERROR_CODE(NAME, NUMBER)                                                               \
    __attribute__((naked)) auto _isr_handle_##NAME() -> void {                                     \
        asm volatile(                                                                              \
            "push $" #NUMBER "\n\t"   /* vector number                                          */ \
            "pusha\n\t"               /* save cpu registers                                     */ \
            "push %esp\n\t"           /* pass pointer to the saved registers to your dispatcher */ \
                                                                                                   \
            "call isr_dispatch\n\t"                                                                \
                                                                                                   \
            "add $4, %esp\n\t"  /* pop the pointer argument */                                     \
            "popa\n\t"          /* restore registers        */                                     \
            "add $8, %esp\n\t"  /* skip vector + error_code */                                     \
            "iret\n\t"          /* restore eip, cs, eflags  */                                     \
        );                                                                                         \
    }

// CPU exceptions (vectors 0–31)
ISR_NO_ERROR_CODE (divide_error,                  0)   // #DE
ISR_NO_ERROR_CODE (debug,                         1)   // #DB
ISR_NO_ERROR_CODE (non_maskable_interrupt,        2)   // NMI
ISR_NO_ERROR_CODE (breakpoint,                    3)   // #BP
ISR_NO_ERROR_CODE (overflow,                      4)   // #OF
ISR_NO_ERROR_CODE (bound_range_exceeded,          5)   // #BR
ISR_NO_ERROR_CODE (invalid_opcode,                6)   // #UD
ISR_NO_ERROR_CODE (device_not_available,          7)   // #NM
ISR_ERROR_CODE    (double_fault,                  8)   // #DF
ISR_NO_ERROR_CODE (coprocessor_segment_overrun,   9)
ISR_ERROR_CODE    (invalid_tss,                   10)  // #TS
ISR_ERROR_CODE    (segment_not_present,           11)  // #NP
ISR_ERROR_CODE    (stack_segment_fault,           12)  // #SS
ISR_ERROR_CODE    (general_protection_fault,      13)  // #GP
ISR_ERROR_CODE    (page_fault,                    14)  // #PF
ISR_NO_ERROR_CODE (reserved_15,                   15)
ISR_NO_ERROR_CODE (x87_floating_point_exception,  16)  // #MF
ISR_ERROR_CODE    (alignment_check,               17)  // #AC
ISR_NO_ERROR_CODE (machine_check,                 18)  // #MC
ISR_NO_ERROR_CODE (simd_floating_point_exception, 19)  // #XM
ISR_NO_ERROR_CODE (virtualisation_exception,      20)  // #VE
ISR_ERROR_CODE    (control_protection_exception,  21)  // #CP
ISR_NO_ERROR_CODE (reserved_22,                   22)
ISR_NO_ERROR_CODE (reserved_23,                   23)
ISR_NO_ERROR_CODE (reserved_24,                   24)
ISR_NO_ERROR_CODE (reserved_25,                   25)
ISR_NO_ERROR_CODE (reserved_26,                   26)
ISR_NO_ERROR_CODE (reserved_27,                   27)
ISR_NO_ERROR_CODE (hypervisor_injection,          28)  // #HV
ISR_ERROR_CODE    (vmm_communication,             29)  // #VC
ISR_ERROR_CODE    (security_exception,            30)  // #SX
ISR_NO_ERROR_CODE (reserved_31,                   31)

// Hardware IRQs (vectors 32–47, after PIC remapping)
ISR_NO_ERROR_CODE (pit_timer,       32)  // IRQ0
ISR_NO_ERROR_CODE (ps2_keyboard,    33)  // IRQ1
ISR_NO_ERROR_CODE (cascade,         34)  // IRQ2
ISR_NO_ERROR_CODE (com2,            35)  // IRQ3
ISR_NO_ERROR_CODE (com1,            36)  // IRQ4
ISR_NO_ERROR_CODE (lpt2,            37)  // IRQ5
ISR_NO_ERROR_CODE (floppy,          38)  // IRQ6
ISR_NO_ERROR_CODE (lpt1_spurious,   39)  // IRQ7
ISR_NO_ERROR_CODE (rtc,             40)  // IRQ8
ISR_NO_ERROR_CODE (acpi,            41)  // IRQ9
ISR_NO_ERROR_CODE (free_irq10,      42)  // IRQ10
ISR_NO_ERROR_CODE (free_irq11,      43)  // IRQ11
ISR_NO_ERROR_CODE (ps2_mouse,       44)  // IRQ12
ISR_NO_ERROR_CODE (fpu,             45)  // IRQ13
ISR_NO_ERROR_CODE (primary_ata,     46)  // IRQ14
ISR_NO_ERROR_CODE (secondary_ata,   47)  // IRQ15

auto isr_unimplemented_handler(Interrupt_Vector_Type type, u32 error) -> void {
    term::println("Tell me why (%): %", type, error);
    halt_forever("Unimplemented interrupt fired.");
}

auto isr_handle_divide_error() -> void {
    halt_forever("Try not dividing by 0 m8.. glhf");
}

auto isr_handle_double_fault(u32 error) -> void {
    term::println("Double fault, caused by IDT entry: %", error);
    halt_forever("");
}

extern "C" auto isr_dispatch(u32* registers_pointer) -> void {
    static constexpr auto VECTOR_TYPE_OFFSET = 8;
    static constexpr auto ERROR_OFFSET       = 9;

    auto type  = static_cast<Interrupt_Vector_Type>(registers_pointer[VECTOR_TYPE_OFFSET]);
    u32  error = registers_pointer[ERROR_OFFSET];

    using enum Interrupt_Vector_Type;
    switch (type) {
        case DIVIDE_ERROR: isr_handle_divide_error();      break;
        case DOUBLE_FAULT: isr_handle_double_fault(error); break;

        default: isr_unimplemented_handler(type, error); break;
    }
}


auto set_gate(Interrupt_Vector_Type vector_type, u32 handler_address) -> void {
    auto& gate = table[static_cast<u8>(vector_type)];
    debug_assert(gate.selector == KERNEL_CODE_SEGMENT);
    debug_assert(gate.zero     == 0);
    debug_assert(gate.type.raw == GATE_PRESENT_RING0_INT32.raw);

    gate.handler_address_low  = static_cast<u16>(0xFFFF & (handler_address >> 0));
    gate.handler_address_high = static_cast<u16>(0xFFFF & (handler_address >> 16));
}

} // namespace idt
