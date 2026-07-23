# Development

    nix develop -c fish
    cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain-i686-elf.cmake && cmake --build build

# Running

    qemu-system-i386 -serial stdio -cdrom build/gameos.iso

# Testing

Host-side unit tests for kstd headers that don't need real hardware I/O
(ports, interrupts, ...) live under `tests/`. They're built with normal g++:

    nix develop -c fish
    cmake -S tests -B build/tests -G Ninja -DCMAKE_CXX_COMPILER=g++ && cmake --build build/tests
    ctest --test-dir build/tests

kstd headers use their own `kstd_assert`/`kstd_memcpy`/`kstd_memset`/`kstd_strlen` (never the
bare libc names) specifically so they can be included in the same translation
unit as `<gtest/gtest.h>` without colliding with `<cassert>`/ `<cstring>`.
`kstd/assert.hh` detects a hosted build (`__STDC_HOSTED__`) and makes `kstd_assert`
print + `abort()` instead of halting the CPU, so anything that halts can be
tested with `EXPECT_DEATH`.

# Terminal interface (switched by using a different namespace)

Must implement all forward declared functions in `kstd/term.hh`. Management of
the current row, column, scrolling should be handled internally.

# TODO:

8-bit slowly drifting

# Abbreviations used

## Serial / UART registers / signals

- IER - Interrupt Enable Register (the UART register that masks which events generate interrupts)
- FCR - FIFO Control Register (enables and configures the UART's send/receive FIFO buffers)
- LCR - Line Control Register (sets data bits, stop bits, parity, and the DLAB mode switch)
- MCR - Modem Control Register (drives the DTR/RTS/OUT2 handshake lines)
- LSR - Line Status Register (reports transmit-ready, data-ready, and error conditions)
- DLAB - Divisor Latch Access Bit (LCR bit 7; repurposes two UART registers into the baud-rate divisor)
- THR - Transmitter Holding Register (holds the next byte waiting to be shifted out)
- THRE - Transmitter Holding Register Empty (LSR bit meaning the UART is ready for another byte)
- DTR - Data Terminal Ready (MCR handshake line signaling the local end is ready to communicate)
- RTS - Request To Send (MCR handshake line signaling the local end wants to transmit)
- FIFO - First In, First Out (the UART's internal send/receive buffer queues)

## Hardware chips / subsystems

- PIC - Programmable Interrupt Controller (the 8259A chip that manages hardware interrupt signals)
- PIT - Programmable Interval Timer (the 8253/8254 chip that generates periodic clock pulses)

## CPU data structures

- IDT - Interrupt Descriptor Table (the array of gate descriptors the CPU looks up when an interrupt fires)
- IDTR - Interrupt Descriptor Table Register (the 6-byte CPU register that holds the address and size of the IDT)
- GDT - Global Descriptor Table (a similar table that describes memory segments; the kernel CS selector comes from here)

## Interrupt terminology

- IRQ - Interrupt ReQuest (a hardware signal from a device saying "I need attention")
- ISR - Interrupt Service Routine (the handler function that runs when an interrupt fires)
- EOI - End Of Interrupt (a command you send back to the PIC to tell it you finished handling the IRQ)
- NMI - Non-Maskable Interrupt (an interrupt that cannot be disabled with `cli`; used for serious hardware errors)

## PIC initialization words

- ICW - Initialization Command Word (the sequence of bytes you write to configure the PIC; there are 4 of them, ICW1–ICW4)
- OCW - Operation Command Word (commands sent to the PIC after init, e.g. setting the mask)
- IMR - Interrupt Mask Register (an 8-bit register in the PIC; a `1` bit disables that IRQ line)

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
- IRET - Interrupt RETurn (the special return instruction used at the end of an ISR; restores `eflags`, `cs`, and `eip` from the stack)
- PUSHA / POPA - Push/Pop All registers (saves or restores all 8 general-purpose registers at once)
- FP - Fixed Point (a way to represent fractional values using integers, e.g. multiplying by 256 to get sub-pixel precision without floats)
