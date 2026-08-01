This project uses `just` as its task runner. Run `just` to list all available
targets. Use `just <target>` for development tasks.

# Development

    just build

# Running

    just run

# Testing

    just test

Tests build with host `g++` (not the freestanding x86_64 toolchain), so
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

# Allocations

All heaps implement `mem::Allocator` (`alloc` / `free`). Null allocator args
resolve to the current global via `mem::resolve_allocator`.

| Allocator | Role |
| --- | --- |
| `Buddy_Allocator` | Kernel default. Page buddy over multiboot regions (`mem::initialize`). |
| `Hosted_Allocator` | Host/tests only. Wraps `operator new` / `delete`. |
| `Temporary_Allocator` | Bump arena. `free` is a no-op. Call `reset()` to reclaim. |
| `Arena_Allocator` | Bump over a backing allocator. |
| `Debug_Allocator` | Tracks live allocs. Asserts no leaks on destroy (tests wrap hosted). |
| `Null_Allocator` | `alloc` / `free` call `unreachable`. Use when no heap is valid / no allocations should happen. |

**Global scope.** `PUSH_ALLOCATOR(a)` sets the global for the current scope
(RAII). `operator new` / `delete` go through the global too.

**Containers.** `Array`, `String_Builder`, and friends store the
`Allocator*` from construction. Free on the same heap that allocated.

**Strings.** `string` is a non-owning `{data, size}` view. It never frees in
its destructor.

- Long-lived heap: `sprint` / `copy_string` / `to_string`, then
  `defer(free_string(s))` (or pass the matching allocator).
- Null-terminated C string: `csprint` (c string print) /
  `to_c_string`, then `defer(free_c_string(p))`.
- Scratch: `tprint` / `tcopy` / `talloc` / `temp_c_string` on
  `mem::temporary_allocator`. Valid until the next `temporary_allocator.reset()`.
- Scratch C string: `ctprint` (c temporary print). Same lifetime rules.

Do not free temp bytes with `free_string` on the global heap. Do not keep
temp pointers across a reset.

# Abbreviations

## Serial / UART registers / signals

- UART - Universal Asynchronous Receiver/Transmitter (serial port hardware, e.g. COM1)
- IER - Interrupt Enable Register (masks which UART events raise interrupts)
- FCR - FIFO Control Register (enables and configures UART send/receive FIFOs)
- LCR - Line Control Register (data bits, stop bits, parity, and the DLAB switch)
- MCR - Modem Control Register (drives DTR/RTS/OUT2 handshake lines)
- LSR - Line Status Register (transmit-ready, data-ready, and error status)
- DLAB - Divisor Latch Access Bit (LCR bit 7). Repurposes two UART registers as the baud-rate divisor.
- THR - Transmitter Holding Register (next byte waiting to be shifted out)
- THRE - Transmitter Holding Register Empty (LSR bit: UART ready for another byte)
- DTR - Data Terminal Ready (MCR line: local end is ready)
- RTS - Request To Send (MCR line: local end wants to transmit)
- FIFO - First In, First Out (UART internal send/receive queues)

## Legacy interrupt chips

- PIC - Programmable Interrupt Controller (8259A chip that routes IRQs on uniprocessor setups)
- PIT - Programmable Interval Timer (8253/8254 chip that generates periodic clock pulses)
- ICW - Initialization Command Word (bytes ICW1-ICW4 that configure the PIC)
- OCW - Operation Command Word (PIC commands after init, e.g. set mask)
- IMR - Interrupt Mask Register (PIC 8-bit mask. A `1` bit disables that IRQ line.)

## APIC / multiprocessor

- SMP - Symmetric MultiProcessing (more than one CPU runs the same kernel image)
- BSP - Bootstrap Processor (CPU that runs firmware, bootloader, and early kernel init)
- AP - Application Processor (other CPU. Stays asleep until the BSP wakes it.)
- APIC - Advanced Programmable Interrupt Controller (modern IRQ + IPI hardware)
- LAPIC - Local APIC (per-CPU APIC. Timer, EOI, and IPI endpoint.)
- IOAPIC - I/O APIC (routes device IRQs to LAPICs. Replaces 8259 routing.)
- xAPIC - classic APIC programming model via MMIO (default base `0xFEE00000`)
- x2APIC - APIC programming model via MSRs (better for many CPUs)
- IPI - Inter-Processor Interrupt (one CPU signals another through the LAPIC)
- INIT - INIT IPI (resets an AP into a wait-for-SIPI state)
- SIPI - Startup IPI (wakes an AP and sets its real-mode start vector)
- ICR - Interrupt Command Register (LAPIC register used to send IPIs)
- LVT - Local Vector Table (LAPIC entries for timer, LINT0/1, error, etc.)
- LINT0 / LINT1 - Local Interrupt pins on the LAPIC (often tied to ExtINT/NMI)
- ExtINT - External Interrupt delivery mode (bridge from 8259-style INTR)
- TPR - Task Priority Register (LAPIC. Blocks interrupt priorities below a threshold.)
- SVR - Spurious Interrupt Vector Register (LAPIC software enable + spurious vector)
- GSI - Global System Interrupt (IOAPIC pin numbering. ISA IRQ N is not always GSI N.)
- MMIO - Memory-Mapped I/O (device registers accessed as memory loads/stores)

## ACPI tables

- ACPI - Advanced Configuration and Power Interface (firmware tables for hardware layout)
- RSDP - Root System Description Pointer (entry point. Signature `"RSD PTR "`.)
- RSDT - Root System Description Table (32-bit phys pointers to other tables)
- XSDT - Extended System Description Table (64-bit phys pointers. Prefer over RSDT.)
- SDT - System Description Table (generic ACPI table with signature + checksum)
- MADT - Multiple APIC Description Table (signature `"APIC"`. CPU list, IOAPIC, overrides.)
- EBDA - Extended BIOS Data Area (low memory. One place to search for the RSDP.)
- BDA - BIOS Data Area (classic PC low-memory firmware data)
- BIOS - Basic Input/Output System (legacy firmware)
- UEFI / EFI - Unified Extensible Firmware Interface (modern firmware. EFI is the short name.)

## CPU tables / segments / privilege

- GDT - Global Descriptor Table (segment descriptors. Kernel CS comes from here.)
- GDTR - GDT Register (base + limit loaded with `lgdt`)
- IDT - Interrupt Descriptor Table (gate descriptors the CPU indexes on interrupt)
- IDTR - IDT Register (base + limit loaded with `lidt`)
- TSS - Task State Segment (holds RSP0 and IST stack pointers in long mode)
- IST - Interrupt Stack Table (TSS fields. CPU switches stack before some exceptions.)
- IVT - Interrupt Vector Table (real-mode 256-entry table at physical 0)
- DPL - Descriptor Privilege Level (0 = ring 0 only, 3 = userspace may use `int n`)
- CS - Code Segment (selector the CPU loads when entering a handler or far jump)
- DS / ES / SS / FS / GS - data / extra / stack / FS / GS segment selectors
- FSGSBASE - CPUID feature + CR4 bit. Allows `wrfsbase` / `wrgsbase` without MSRs.

## Control registers / MSRs / mode bits

- CR0 / CR2 / CR3 / CR4 - Control Registers (paging, PE/PG, fault address, features)
- PE - Protection Enable (CR0). Turns on protected mode.
- PG - Paging (CR0). Turns on the page walker.
- WP - Write Protect (CR0). Kernel writes to read-only user pages fault.
- MP / EM / TS / NE - CR0 FPU bits (monitor coprocessor, emulate, task switched, native errors)
- PAE - Physical Address Extension (CR4). Required path into long mode.
- PGE - Page Global Enable (CR4). Global pages skip full TLB flush on CR3 load.
- PSE - Page Size Extension (large pages in older modes)
- OSFXSR - CR4 bit. Enables FXSAVE/FXRSTOR and SSE state.
- OSXMMEXCPT - CR4 bit. Enables #XF for SIMD numeric errors.
- FSGSBASE (CR4) - enables user/kernel FS/GS base instructions
- PCID - Process Context Identifier (CR4.PCIDE). Tags TLB entries by address space.
- LA57 - 5-level paging (CR4). 57-bit virtual addresses. Not used yet.
- SMEP / SMAP - Supervisor Mode Execution/Access Prevention (CR4)
- EFER - Extended Feature Enable Register (MSR). Long mode and NX live here.
- LME - Long Mode Enable (EFER). Arm long mode before paging.
- LMA - Long Mode Active (EFER). CPU is actually in long mode.
- NX / NXE - No-eXecute / NX Enable (EFER.NXE + page bit. Marks pages non-executable.)
- MSR - Model Specific Register (touched with `rdmsr` / `wrmsr`)
- IA32_APIC_BASE - MSR `0x1B` (LAPIC base phys address + global/x2APIC enable)
- IA32_GS_BASE - MSR `0xC0000101` (kernel GS base. Common Cpu_Local pointer.)
- XCR0 - Extended Control Register 0 (XSAVE feature mask)

## Paging

- PML4 - Page Map Level 4 (top table in 4-level long mode)
- PDPT - Page Directory Pointer Table (level under PML4)
- PD - Page Directory
- PT - Page Table (leaf level for 4 KiB pages)
- PML4E / PDPTE / PDE / PTE - entries in those tables
- PS - Page Size bit (PDE/PDPTE. Marks a large page instead of a lower table.)
- PCD - Page Cache Disable (page or PAT path. Often set on MMIO.)
- PWT - Page Write-Through
- PAT - Page Attribute Table (finer memory types than PCD/PWT alone)
- TLB - Translation Lookaside Buffer (cached virtual-to-physical translations)
- HHDM - Higher-Half Direct Map (physical memory also mapped in high virtual space)
- FB - Framebuffer (pixel buffer from Multiboot/GOP)

## FPU / SIMD

- FPU - Floating Point Unit (x87 register stack and instructions)
- SIMD - Single Instruction Multiple Data (packed vector ops)
- MMX / SSE / SSE2 / SSE3 / SSSE3 / SSE4 / AVX - SIMD extension families
- XMM - 128-bit SSE registers
- FXSR - CPUID feature. FXSAVE/FXRSTOR available.
- FXSAVE / FXRSTOR - save/restore x87 + MXCSR + XMM (512-byte aligned area)
- XSAVE / XRSTOR - extensible save/restore (needs XCR0 setup)
- #NM - Device Not Available (TS set or FPU disabled. Used for lazy FPU.)
- #MF - x87 Floating-Point Exception
- #XF - SIMD Floating-Point Exception (SSE numeric error when OSXMMEXCPT is on)

## Interrupt / exception terms

- IRQ - Interrupt ReQuest (device signal that needs service)
- ISR - Interrupt Service Routine (handler that runs for an interrupt or exception)
- EOI - End Of Interrupt (ack to PIC or LAPIC after handling)
- NMI - Non-Maskable Interrupt (cannot mask with `cli`. Serious hardware events.)
- MCE - Machine Check Exception (hardware error reporting path)
- IF - Interrupt Flag (in EFLAGS/RFLAGS. `sti` sets, `cli` clears.)

## CPU exception names (`#` is Intel notation)

- #DE - Divide Error
- #DB - Debug
- #BP - Breakpoint
- #OF - Overflow
- #BR - Bound Range exceeded
- #UD - Invalid Opcode
- #NM - Device Not Available (no FPU / TS)
- #DF - Double Fault
- #TS - Invalid TSS
- #NP - Segment Not Present
- #SS - Stack Segment fault
- #GP - General Protection fault
- #PF - Page Fault
- #MF - x87 Floating-Point Exception
- #AC - Alignment Check
- #MC - Machine Check
- #XM / #XF - SIMD Floating-Point Exception

## CPUID / features / time

- CPUID - CPU Identification instruction (feature leaves and vendor string)
- TSC - Time Stamp Counter (`rdtsc`. Cycle counter.)
- RDRAND - hardware random number instruction (CPUID feature bit)
- SDM - Intel Software Developer's Manual (architecture reference)

## Instructions / flags

- HLT - Halt until the next interrupt
- STI - Set Interrupt flag
- CLI - CLear Interrupt flag
- IRET / IRETQ - Interrupt Return (32-bit / 64-bit). Pops frame including flags.
- PUSHA / POPA - Push/Pop All GPRs (32-bit only. Gone in long mode.)
- SWAPGS - swap GS base with kernel GS base MSR (syscall/entry helper)
- LGDT / LIDT / LTR - load GDTR / IDTR / task register
- INVLPG - invalidate one TLB page
- MFENCE / SFENCE / LFENCE - memory ordering fences
- WBINVD / CLFLUSH - cache writeback/invalidate helpers
- RDMSR / WRMSR - read/write MSR
- RFLAGS / EFLAGS - flags register (64-bit / 32-bit name)
- RIP / RSP / EIP / ESP - instruction and stack pointers (64 / 32)

## ISA / boot / toolchain

- ISA - Industry Standard Architecture (legacy PC IRQ numbering in MADT overrides)
- A20 - A20 address line gate (legacy >1 MiB wrap. Bootloader usually opens it.)
- GRUB - GRand Unified Bootloader (Multiboot2 loader used here)
- Multiboot2 - boot protocol (memory map, framebuffer, ACPI tags, etc.)
- ELF / ELF32 / ELF64 - Executable and Linkable Format (kernel binary object format)
- ABI - Application Binary Interface (calling convention, register roles)
- SysV - System V AMD64 ABI (common x86_64 C calling convention)
- LP64 - data model: long and pointers are 64-bit, int is 32-bit
- AMD64 - 64-bit x86 architecture (also called x86_64)
- QEMU - emulator used to run and test the kernel
- GOP - Graphics Output Protocol (UEFI framebuffer. Multiboot may pass a similar FB.)

## Sync / scheduling shorthand

- TLS - Thread-Local Storage
- TCB - Thread Control Block
- SPSC - Single Producer Single Consumer (queue)
- MPSC - Multi Producer Single Consumer
- SPMC - Single Producer Multi Consumer
- RCU - Read-Copy-Update (deferred reclamation concurrency pattern)
- RMW - Read-Modify-Write (atomic update pattern)

## Misc

- FP - Fixed Point (fractional values via scaled integers, e.g. `x * 256`)
- COM - PC serial port name (COM1 is the usual debug UART)
- PS/2 - legacy keyboard/mouse controller interface
- KiB / MiB / GiB - 2^10 / 2^20 / 2^30 bytes
