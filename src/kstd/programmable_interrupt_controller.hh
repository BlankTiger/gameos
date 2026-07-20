#pragma once

#include "low_level_io.hh"
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

// PIC1
constexpr u16 PIC_CMD_PORT_MASTER  = 0x20;
constexpr u16 PIC_DATA_PORT_MASTER = 0x21;

// PIC2
constexpr u16 PIC_CMD_PORT_SLAVE  = 0xA0;
constexpr u16 PIC_DATA_PORT_SLAVE = 0xA1;

constexpr u8 END_OF_INTERRUPT     = 0x20;
constexpr u8 VECTOR_OFFSET_MASTER = 32;

auto send_eoi(u8 vector) -> void {
    debug_assert(vector >= VECTOR_OFFSET_MASTER);

    auto irq = vector - VECTOR_OFFSET_MASTER;
    if(irq >= 8) low_level_io::outb(PIC_CMD_PORT_SLAVE, END_OF_INTERRUPT);
    low_level_io::outb(PIC_CMD_PORT_MASTER, END_OF_INTERRUPT);
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

    // Mask everything except IRQ0 (PIT timer).
    (void)mask1;
    (void)mask2;
    outb_with_delay(PIC_DATA_PORT_MASTER, 0b11111110); // unmask IRQ0 only
    outb_with_delay(PIC_DATA_PORT_SLAVE,  0xFF);       // mask all slave IRQs
}

auto disable() -> void {
    using namespace low_level_io;

    // Done by masking all the interrupts.
    outb_with_delay(PIC_DATA_PORT_MASTER, 0xff);
    outb_with_delay(PIC_DATA_PORT_SLAVE,  0xff);
}

}
