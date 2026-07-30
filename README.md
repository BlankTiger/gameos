This project uses `just` as its task runner. Run `just` to list all available
targets. Use `just <target>` for development tasks.

# Development

    just build

# Running

    just run

# Testing

    just test

Tests build with host `g++` (not the freestanding i686 toolchain), so
`__STDC_HOSTED__` is set and `HOSTED` is 1 (`kstd/basic.hh`). That changes
several kstd paths:

- `kstd_assert` prints to stderr and calls `abort()` instead of halting the
  CPU. Use `EXPECT_DEATH` for failing assertions.
- `kstd_memcpy` / `kstd_memset` / `kstd_memset32` call libc (or a plain loop)
  instead of freestanding `rep movs*` / `rep stos*` asm.
- Allocators use `Hosted_Allocator` (`operator new` / `delete`). Under
  `UNIT_TEST`, the global allocator is a `Debug_Allocator` over that heap.
  A fixed buffer backs the temporary allocator (no kernel buddy).

kstd headers keep their own names (`kstd_assert`, `kstd_memcpy`, and so on).
They never use bare libc names. That avoids collisions with
`<gtest/gtest.h>`, `<cassert>`, or `<cstring>` in the same translation unit.

# Abbreviations

## Serial / UART registers / signals

- IER - Interrupt Enable Register (the UART register that masks which events generate interrupts)
- FCR - FIFO Control Register (enables and configures the UART's send/receive FIFO buffers)
- LCR - Line Control Register (sets data bits, stop bits, parity, and the DLAB mode switch)
- MCR - Modem Control Register (drives the DTR/RTS/OUT2 handshake lines)
- LSR - Line Status Register (reports transmit-ready, data-ready, and error conditions)
- DLAB - Divisor Latch Access Bit (LCR bit 7). It repurposes two UART registers into the baud-rate divisor.
- THR - Transmitter Holding Register (holds the next byte waiting to be shifted out)
- THRE - Transmitter Holding Register Empty (LSR bit that shows the UART is ready for another byte)
- DTR - Data Terminal Ready (MCR handshake line that signals the local end is ready to communicate)
- RTS - Request To Send (MCR handshake line that signals the local end wants to transmit)
- FIFO - First In, First Out (the UART's internal send/receive buffer queues)

## Hardware chips / subsystems

- PIC - Programmable Interrupt Controller (the 8259A chip that manages hardware interrupt signals)
- PIT - Programmable Interval Timer (the 8253/8254 chip that generates periodic clock pulses)

## CPU data structures

- IDT - Interrupt Descriptor Table (the array of gate descriptors the CPU looks up when an interrupt fires)
- IDTR - Interrupt Descriptor Table Register (the 6-byte CPU register that holds the address and size of the IDT)
- GDT - Global Descriptor Table (a similar table that describes memory segments. The kernel CS selector comes from here.)

## Interrupt terminology

- IRQ - Interrupt ReQuest (a hardware signal from a device saying "I need attention")
- ISR - Interrupt Service Routine (the handler function that runs when an interrupt fires)
- EOI - End Of Interrupt (a command you send back to the PIC to tell it you finished handling the IRQ)
- NMI - Non-Maskable Interrupt (an interrupt that cannot be disabled with `cli`. It is used for serious hardware errors.)

## PIC initialization words

- ICW - Initialization Command Word (the sequence of bytes you write to configure the PIC. There are 4 of them, ICW1-ICW4.)
- OCW - Operation Command Word (commands sent to the PIC after init, e.g. setting the mask)
- IMR - Interrupt Mask Register (an 8-bit register in the PIC. A `1` bit disables that IRQ line.)

## Gate descriptor fields

- DPL - Descriptor Privilege Level (0 = ring 0 kernel only, 3 = userspace can also trigger via `int n`)
- CS - Code Segment (the segment selector the CPU switches to when jumping to your handler)

## CPU exception names (the # prefix is Intel's notation)

- #DE - Divide Error (division by zero)
- #GP - General Protection fault (invalid memory access, bad segment, etc.)
- #PF - Page Fault (accessed a non-present or protected page)
- #DF - Double Fault (an exception occurred while handling another exception)
- #UD - Undefined/invalid opcode
- #NM - No Math coprocessor (FPU not available)
- #BR - Bound Range exceeded
- #OF - Overflow
- #BP - BreakPoint
- #DB - DeBug exception

## Misc

- HLT - Halt (CPU instruction that pauses execution until the next interrupt)
- STI - Set Interrupt flag (enables interrupts)
- CLI - CLear Interrupt flag (disables interrupts)
- IRET - Interrupt RETurn (the special return instruction used at the end of an ISR. It restores `eflags`, `cs`, and `eip` from the stack.)
- PUSHA / POPA - Push/Pop All registers (saves or restores all 8 general-purpose registers at once)
- FP - Fixed Point (a way to represent fractional values using integers, e.g. multiplying by 256 to get sub-pixel precision without floats)
