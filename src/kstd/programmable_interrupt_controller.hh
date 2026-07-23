#pragma once

#include "low_level_io.hh"
#include "assert.hh"

namespace pic {

union ICW1 {
    struct {
        u8 icw4_needed : 1;  // 1 = ICW4 will follow
        u8 single_mode : 1;  // 0 = cascade mode (two PICs)
        u8 interval4   : 1;  // 0 = 8-byte interrupt vectors (unused on x86)
        u8 level_trig  : 1;  // 0 = edge triggered
        u8 init        : 1;  // must be 1 — tells chip this is ICW1
        u8 _reserved   : 3;
    };
    u8 raw;
} __attribute__((packed));

union ICW2 {
    struct {
        u8 _unused : 3;  // must be 0 — low 3 bits are set by the PIC itself
        u8 vector  : 5;  // upper 5 bits of base vector (32 >> 3 = 4, 40 >> 3 = 5)
    };
    u8 raw;
} __attribute__((packed));

union ICW3_Master {
    struct {
    // @TODO: Maybe expand to single bits.
        u8 slave_irq : 8;  // bitmask — bit N = IRQ N has a slave attached
    };
    u8 raw;
} __attribute__((packed));

union ICW3_Slave {
    struct {
        u8 slave_id  : 3;  // which IRQ line on the master we're connected to
        u8 _reserved : 5;
    };
    u8 raw;
} __attribute__((packed));

union ICW4 {
    struct {
        u8 mode_8086  : 1;  // 1 = 8086 mode
        u8 auto_eoi   : 1;  // 0 = manual EOI
        u8 buf_slave  : 1;  // buffered mode — 0 for non-buffered
        u8 buf_enable : 1;  // buffered mode — 0 for non-buffered
        u8 sfnm       : 1;  // special fully nested mode — 0 normally
        u8 _reserved  : 3;
    };
    u8 raw;
} __attribute__((packed));

// Interrupt Mask Register - 1 = masked (disabled), 0 = unmasked (enabled).
// Master PIC covers IRQ0-IRQ7
union IMR {
    struct {
        u8 irq0_pit      : 1;  // IRQ0 - PIT timer
        u8 irq1_keyboard : 1;  // IRQ1 - PS/2 keyboard
        u8 irq2_cascade  : 1;  // IRQ2 - cascade to slave (must be unmasked for slave IRQs)
        u8 irq3_com2     : 1;  // IRQ3 - COM2
        u8 irq4_com1     : 1;  // IRQ4 - COM1
        u8 irq5_lpt2     : 1;  // IRQ5 - LPT2 / sound card
        u8 irq6_floppy   : 1;  // IRQ6 - floppy disk
        u8 irq7_lpt1     : 1;  // IRQ7 - LPT1 / spurious
    };
    u8 raw;
} __attribute__((packed));

// Slave PIC covers IRQ8-IRQ15.
union IMR_Slave {
    struct {
        u8 irq8_rtc      : 1;  // IRQ8  - real-time clock
        u8 irq9_acpi     : 1;  // IRQ9  - ACPI / legacy IRQ2 redirect
        u8 irq10         : 1;  // IRQ10 - open
        u8 irq11         : 1;  // IRQ11 - open
        u8 irq12_mouse   : 1;  // IRQ12 - PS/2 mouse
        u8 irq13_fpu     : 1;  // IRQ13 - FPU / coprocessor
        u8 irq14_ata1    : 1;  // IRQ14 - primary ATA
        u8 irq15_ata2    : 1;  // IRQ15 - secondary ATA
    };
    u8 raw;
} __attribute__((packed));

constexpr u8 END_OF_INTERRUPT     = 0x20;
constexpr u8 VECTOR_OFFSET_MASTER = 32;

auto send_eoi(u8 vector) -> void {
    kstd_debug_assert(vector >= VECTOR_OFFSET_MASTER);

    using namespace low_level_io;

    auto irq = vector - VECTOR_OFFSET_MASTER;
    if(irq >= 8) outb(PIC_CMD_PORT_SLAVE, END_OF_INTERRUPT);
    outb(PIC_CMD_PORT_MASTER, END_OF_INTERRUPT);
}

constexpr ICW1 INIT_CMD = {
    .icw4_needed = 1,
    .single_mode = 0,
    .interval4   = 0,
    .level_trig  = 0,
    .init        = 1,
    ._reserved   = 0,
};

// Master: vector=32, 32>>3=4 goes into the upper 5 bits
constexpr ICW2 VECTOR_OFFSET_MASTER_DATA = {
    ._unused = 0,
    .vector  = VECTOR_OFFSET_MASTER >> 3,
};

// Slave: vector=40, 40>>3=5
constexpr ICW2 VECTOR_OFFSET_SLAVE_DATA = {
    ._unused = 0,
    .vector  = 40 >> 3,
};

constexpr ICW3_Master CASCADE_WIRING_MASTER_DATA = { .slave_irq = 0x04 };
constexpr ICW3_Slave  CASCADE_WIRING_SLAVE_DATA  = { .slave_id  = 0x02, ._reserved = 0 };

constexpr ICW4 CPU_MODE_DATA = {
    .mode_8086  = 1,
    .auto_eoi   = 0,
    .buf_slave  = 0,
    .buf_enable = 0,
    .sfnm       = 0,
    ._reserved  = 0,
};

auto initialize() -> void {
    using namespace low_level_io;

    // Save current IMRs.
    u8 mask1 = inb(PIC_DATA_PORT_MASTER);
    u8 mask2 = inb(PIC_DATA_PORT_SLAVE);

    // ICW1, start initializing. This command makes the chip wait for 3 more instructions:
    // - ICW2 -- Its vector offset.
    // - ICW3 -- Tell it how it is wired to master/slaves.
    // - ICW4 -- Gives additional information about the environment.
    outb_with_delay(PIC_CMD_PORT_MASTER, INIT_CMD.raw);
    outb_with_delay(PIC_CMD_PORT_SLAVE,  INIT_CMD.raw);

    // ICW2
    outb_with_delay(PIC_DATA_PORT_MASTER, VECTOR_OFFSET_MASTER_DATA.raw);
    outb_with_delay(PIC_DATA_PORT_SLAVE,  VECTOR_OFFSET_SLAVE_DATA.raw);

    // ICW3
    outb_with_delay(PIC_DATA_PORT_MASTER, CASCADE_WIRING_MASTER_DATA.raw);
    outb_with_delay(PIC_DATA_PORT_SLAVE,  CASCADE_WIRING_SLAVE_DATA.raw);

    // ICW4
    outb_with_delay(PIC_DATA_PORT_MASTER, CPU_MODE_DATA.raw);
    outb_with_delay(PIC_DATA_PORT_SLAVE,  CPU_MODE_DATA.raw);

    (void)mask1;
    (void)mask2;
    constexpr IMR master_mask = {
        .irq0_pit      = 0,
        .irq1_keyboard = 0,
        .irq2_cascade  = 0,
        .irq3_com2     = 1,
        .irq4_com1     = 1,
        .irq5_lpt2     = 1,
        .irq6_floppy   = 1,
        .irq7_lpt1     = 1,
    };
    constexpr IMR_Slave slave_mask = {
        .irq8_rtc    = 1,
        .irq9_acpi   = 1,
        .irq10       = 1,
        .irq11       = 1,
        .irq12_mouse = 0,
        .irq13_fpu   = 1,
        .irq14_ata1  = 1,
        .irq15_ata2  = 1,
    };
    outb_with_delay(PIC_DATA_PORT_MASTER, master_mask.raw);
    outb_with_delay(PIC_DATA_PORT_SLAVE,  slave_mask.raw);
}

auto disable() -> void {
    using namespace low_level_io;

    // Mask all interrupts.
    constexpr IMR       all_masked        = { .raw = 0xFF };
    constexpr IMR_Slave all_slave_masked  = { .raw = 0xFF };
    outb_with_delay(PIC_DATA_PORT_MASTER, all_masked.raw);
    outb_with_delay(PIC_DATA_PORT_SLAVE,  all_slave_masked.raw);
}

}
