#pragma once

#include "allocator.hh"
#include "basic.hh"
#include "assert.hh"
#include "array.hh"
#include "halt_format.hh"
#include "string_builder.hh"
#include "low_level_io.hh"
#include "time.hh"
#include "programmable_interrupt_controller.hh"
#include "global_descriptors.hh"
#include "local_apic.hh"
#include "cpu_local.hh"
#include "term.hh"
#include "ps2.hh"

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

constexpr Gate_Type_Attributes GATE_PRESENT_RING0_INT = {
    .gate_type                  = 0xE,
    .storage                    = 0,
    .descriptor_privilege_level = 0, // ring-0, kernel
    .present                    = 1,
};

struct Gate {
    u16                  handler_address_low;
    u16                  selector = gdt::KERNEL_CODE_SEGMENT;
    u8                   ist      = 0; // bits 2:0 = IST index; bits 7:3 = 0
    Gate_Type_Attributes type     = GATE_PRESENT_RING0_INT;
    u16                  handler_address_mid;
    u32                  handler_address_high;
    u32                  reserved = 0;
} __attribute__((packed));

static_assert(sizeof(Gate) == 16);

struct Interrupt_Descriptor_Table_Register {
    u16   limit;
    psize base;
} __attribute__((packed));

static_assert(sizeof(Interrupt_Descriptor_Table_Register) == 10);

constexpr auto NUM_VECTORS = 256;
inline Static_Array<Gate, NUM_VECTORS>     table;
inline Interrupt_Descriptor_Table_Register interrupt_descriptor_table_register;

enum struct Interrupt_Vector_Type : u8 {
    // CPU exceptions (vectors 0-31)
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

    // Hardware IRQs (vectors 32-47, after PIC remapping)
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

    // This should be outside the 8259 remap window.
    LOCAL_APIC_TIMER              = lapic::TIMER_INTERRUPT_VECTOR,
    LOCAL_APIC_SPURIOUS           = lapic::SPURIOUS_INTERRUPT_VECTOR,
};


//
// Stack at isr_dispatch (low address = %rsp):
//   r15..r8, rbp, rdi, rsi, rdx, rcx, rbx, rax,
//   vector, error_code,
//   rip, cs, rflags, rsp, ss
//
// Long mode always pushes SS:RSP on interrupt (Intel SDM).
//
struct Interrupt_Frame {
    u64 r15, r14, r13, r12, r11, r10, r9, r8;
    u64 rbp, rdi, rsi, rdx, rcx, rbx, rax;
    u64 vector;
    u64 error_code;
    u64 rip, cs, rflags, rsp, ss;
};

//
// Common tail after vector/error are on the stack:
//   push GPRs (matches Interrupt_Frame field order when read upward from %rsp),
//   fxsave to Core_Info via %gs:0, call isr_dispatch(%rdi = frame), fxrstor, pop, iretq.
//
// Stack alignment: after 15 GPRs + vector + error (17 qwords) + CPU frame,
// we align %rsp to 16 before `call` per SysV.
//

#define ISR_COMMON_BODY                                                                         \
    "push %rax\n\t"                                                                             \
    "push %rbx\n\t"                                                                             \
    "push %rcx\n\t"                                                                             \
    "push %rdx\n\t"                                                                             \
    "push %rsi\n\t"                                                                             \
    "push %rdi\n\t"                                                                             \
    "push %rbp\n\t"                                                                             \
    "push %r8\n\t"                                                                              \
    "push %r9\n\t"                                                                              \
    "push %r10\n\t"                                                                             \
    "push %r11\n\t"                                                                             \
    "push %r12\n\t"                                                                             \
    "push %r13\n\t"                                                                             \
    "push %r14\n\t"                                                                             \
    "push %r15\n\t"                                                                             \
    "mov %rsp, %rdi\n\t"                                                                        \
    "mov %rsp, %rbp\n\t"                                                                        \
    "and $-16, %rsp\n\t"                                                                        \
    "movq %gs:0, %rax\n\t"                                                                      \
    "fxsave  " CPU_LOCAL_ASM_STR(CPU_LOCAL_FPU_IRQ_SAVE_OFFSET) "(%rax)\n\t"                    \
    "call isr_dispatch\n\t"                                                                     \
    "movq %gs:0, %rax\n\t"                                                                      \
    "fxrstor " CPU_LOCAL_ASM_STR(CPU_LOCAL_FPU_IRQ_SAVE_OFFSET) "(%rax)\n\t"                    \
    "mov %rbp, %rsp\n\t"                                                                        \
    "pop %r15\n\t"                                                                              \
    "pop %r14\n\t"                                                                              \
    "pop %r13\n\t"                                                                              \
    "pop %r12\n\t"                                                                              \
    "pop %r11\n\t"                                                                              \
    "pop %r10\n\t"                                                                              \
    "pop %r9\n\t"                                                                               \
    "pop %r8\n\t"                                                                               \
    "pop %rbp\n\t"                                                                              \
    "pop %rdi\n\t"                                                                              \
    "pop %rsi\n\t"                                                                              \
    "pop %rdx\n\t"                                                                              \
    "pop %rcx\n\t"                                                                              \
    "pop %rbx\n\t"                                                                              \
    "pop %rax\n\t"                                                                              \
    "add $16, %rsp\n\t"                                                                         \
    "iretq\n\t"

#define ISR_NO_ERROR_CODE(NAME, NUMBER)                                                         \
    __attribute__((naked)) auto _isr_handle_##NAME() -> void {                                  \
        asm volatile(                                                                           \
            "push $0\n\t"                                                                       \
            "push $" #NUMBER "\n\t"                                                             \
            ISR_COMMON_BODY                                                                     \
        );                                                                                      \
    }

#define ISR_ERROR_CODE(NAME, NUMBER)                                                            \
    __attribute__((naked)) auto _isr_handle_##NAME() -> void {                                  \
        asm volatile(                                                                           \
            "push $" #NUMBER "\n\t"                                                             \
            ISR_COMMON_BODY                                                                     \
        );                                                                                      \
    }

// CPU exceptions (vectors 0-31)
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

// Hardware IRQs (vectors 32-47, after PIC remapping)
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

ISR_NO_ERROR_CODE (local_apic_timer,    64)   // lapic::TIMER_INTERRUPT_VECTOR
ISR_NO_ERROR_CODE (local_apic_spurious, 255)  // lapic::SPURIOUS_INTERRUPT_VECTOR

namespace hidden {
    // 8 KiB more than enough for error messages in interrupt handlers.
    Static_Array<u8, 8 * 1024> error_message_buffer{};
    inline mem::Arena_Allocator emergency_error_message_allocator{error_message_buffer};
}

auto isr_unimplemented_handler(Interrupt_Vector_Type type, u64 error) -> void {
    auto error_message = csprint(
        &hidden::emergency_error_message_allocator,
        "Unimplemented interrupt fired. Tell me why (%): %",
        type, error
    );
    halt::forever(error_message);
}

auto isr_handle_divide_error() -> void {
    halt::forever("Try not dividing by 0 m8.. glhf");
}

auto isr_handle_double_fault(u64 error) -> void {
    auto error_message = csprint(
        &hidden::emergency_error_message_allocator,
        "Double fault, caused by IDT entry: %",
        error
    );
    halt::forever(error_message);
}

auto isr_handle_programmable_interval_timer() -> void {
    ktime::on_tick();
}

auto isr_handle_local_apic_timer() -> void {
    cpu_local::current().lapic_ticks += 1;
    ktime::on_tick();
}

auto isr_handle_local_apic_spurious() -> void {
    // Hardware spurious: no end-of-interrupt required (Intel SDM).
}

extern "C" auto isr_dispatch(Interrupt_Frame* frame) -> void {
    auto type  = static_cast<Interrupt_Vector_Type>(frame->vector);
    u64  error = frame->error_code;

    using enum Interrupt_Vector_Type;
    switch (type) {
        case DIVIDE_ERROR:        isr_handle_divide_error();                break;
        case DOUBLE_FAULT:        isr_handle_double_fault(error);           break;
        case PS2_KEYBOARD:        ps2::isr_handle_ps2_keyboard();           break;
        case PS2_MOUSE:           ps2::isr_handle_ps2_mouse();              break;
        case PIT_TIMER:           isr_handle_programmable_interval_timer(); break;
        case LOCAL_APIC_TIMER:    isr_handle_local_apic_timer();            break;
        case LOCAL_APIC_SPURIOUS: isr_handle_local_apic_spurious();         break;

        default: isr_unimplemented_handler(type, error); break;
    }

    const u8 vector = static_cast<u8>(type);
    if (vector >= 32 && vector < 48) {
        // 8259-sourced lines (remapped IRQs).
        pic::send_end_of_interrupt(vector);
    } else if (type == LOCAL_APIC_TIMER) {
        lapic::signal_end_of_interrupt();
    }
}

auto set_gate(Interrupt_Vector_Type vector_type, auto (*handler_function)() -> void, u8 ist = 0) -> void {
    auto& gate = table[static_cast<u8>(vector_type)];
    kstd_debug_assert(gate.selector == gdt::KERNEL_CODE_SEGMENT);
    kstd_debug_assert(gate.type.raw == GATE_PRESENT_RING0_INT.raw);
    kstd_debug_assert(ist <= 7);

    auto handler_address = reinterpret_cast<psize>(handler_function);
    gate.handler_address_low  = static_cast<u16>(handler_address);
    gate.handler_address_mid  = static_cast<u16>(handler_address >> 16);
    gate.handler_address_high = static_cast<u32>(handler_address >> 32);
    gate.ist                  = ist;
    gate.reserved             = 0;
}

auto initialize() -> void {
    {
        using enum Interrupt_Vector_Type;

        // CPU exceptions (vectors 0-31)
        set_gate(DIVIDE_ERROR,                  _isr_handle_divide_error);
        set_gate(DEBUG,                         _isr_handle_debug);
        set_gate(NON_MASKABLE_INTERRUPT,        _isr_handle_non_maskable_interrupt);
        set_gate(BREAKPOINT,                    _isr_handle_breakpoint);
        set_gate(OVERFLOW,                      _isr_handle_overflow);
        set_gate(BOUND_RANGE_EXCEEDED,          _isr_handle_bound_range_exceeded);
        set_gate(INVALID_OPCODE,                _isr_handle_invalid_opcode);
        set_gate(DEVICE_NOT_AVAILABLE,          _isr_handle_device_not_available);
        set_gate(DOUBLE_FAULT,                  _isr_handle_double_fault, 1); // IST1
        set_gate(COPROCESSOR_SEGMENT_OVERRUN,   _isr_handle_coprocessor_segment_overrun);
        set_gate(INVALID_TSS,                   _isr_handle_invalid_tss);
        set_gate(SEGMENT_NOT_PRESENT,           _isr_handle_segment_not_present);
        set_gate(STACK_SEGMENT_FAULT,           _isr_handle_stack_segment_fault);
        set_gate(GENERAL_PROTECTION_FAULT,      _isr_handle_general_protection_fault);
        set_gate(PAGE_FAULT,                    _isr_handle_page_fault);
        set_gate(RESERVED_15,                   _isr_handle_reserved_15);
        set_gate(X87_FLOATING_POINT_EXCEPTION,  _isr_handle_x87_floating_point_exception);
        set_gate(ALIGNMENT_CHECK,               _isr_handle_alignment_check);
        set_gate(MACHINE_CHECK,                 _isr_handle_machine_check);
        set_gate(SIMD_FLOATING_POINT_EXCEPTION, _isr_handle_simd_floating_point_exception);
        set_gate(VIRTUALISATION_EXCEPTION,      _isr_handle_virtualisation_exception);
        set_gate(CONTROL_PROTECTION_EXCEPTION,  _isr_handle_control_protection_exception);
        set_gate(RESERVED_22,                   _isr_handle_reserved_22);
        set_gate(RESERVED_23,                   _isr_handle_reserved_23);
        set_gate(RESERVED_24,                   _isr_handle_reserved_24);
        set_gate(RESERVED_25,                   _isr_handle_reserved_25);
        set_gate(RESERVED_26,                   _isr_handle_reserved_26);
        set_gate(RESERVED_27,                   _isr_handle_reserved_27);
        set_gate(HYPERVISOR_INJECTION,          _isr_handle_hypervisor_injection);
        set_gate(VMM_COMMUNICATION,             _isr_handle_vmm_communication);
        set_gate(SECURITY_EXCEPTION,            _isr_handle_security_exception);
        set_gate(RESERVED_31,                   _isr_handle_reserved_31);

        // Hardware IRQs (vectors 32-47)
        set_gate(PIT_TIMER,                     _isr_handle_pit_timer);
        set_gate(PS2_KEYBOARD,                  _isr_handle_ps2_keyboard);
        set_gate(CASCADE,                       _isr_handle_cascade);
        set_gate(COM2,                          _isr_handle_com2);
        set_gate(COM1,                          _isr_handle_com1);
        set_gate(LPT2,                          _isr_handle_lpt2);
        set_gate(FLOPPY,                        _isr_handle_floppy);
        set_gate(LPT1_SPURIOUS,                 _isr_handle_lpt1_spurious);
        set_gate(RTC,                           _isr_handle_rtc);
        set_gate(ACPI,                          _isr_handle_acpi);
        set_gate(FREE_IRQ10,                    _isr_handle_free_irq10);
        set_gate(FREE_IRQ11,                    _isr_handle_free_irq11);
        set_gate(PS2_MOUSE,                     _isr_handle_ps2_mouse);
        set_gate(FPU,                           _isr_handle_fpu);
        set_gate(PRIMARY_ATA,                   _isr_handle_primary_ata);
        set_gate(SECONDARY_ATA,                 _isr_handle_secondary_ata);

        set_gate(LOCAL_APIC_TIMER,              _isr_handle_local_apic_timer);
        set_gate(LOCAL_APIC_SPURIOUS,           _isr_handle_local_apic_spurious);
    }

    interrupt_descriptor_table_register.limit = table.size * sizeof(Gate) - 1;
    interrupt_descriptor_table_register.base  = reinterpret_cast<psize>(&table[0]);
    asm volatile("lidt %0" : : "m"(interrupt_descriptor_table_register));
}

force_inline auto enable_interrupts() -> void {
    asm volatile("sti");
}

} // namespace idt
