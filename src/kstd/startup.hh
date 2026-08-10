#pragma once

#include "kstd/advanced_configuration_and_power_interface.hh"
#include "kstd/application_processor.hh"
#include "kstd/assert.hh"
#include "kstd/cpu_local.hh"
#include "kstd/cpuid.hh"
#include "kstd/gfx.hh"
#include "kstd/global_constructor_handling.hh"
#include "kstd/global_descriptors.hh"
#include "kstd/interrupts.hh"
#include "kstd/ioapic.hh"
#include "kstd/local_apic.hh"
#include "kstd/memory.hh"
#include "kstd/multiboot2.hh"
#include "kstd/programmable_interrupt_controller.hh"
#include "kstd/ps2.hh"
#include "kstd/random.hh"
#include "kstd/serial_format.hh"
#include "kstd/term.hh"
#include "kstd/threads.hh"
#include "kstd/time.hh"
#include "kstd/translation_lookaside_buffer.hh"

#if !defined(__x86_64__)
#error "This kernel needs an x86_64-elf compiler"
#endif

inline auto kernel_startup(u32 magic, const boot::Multiboot2_Info* mbi) -> void {
    serial::initialize();

    kstd_assert(magic == boot::MULTIBOOT2_MAGIC, "bad multiboot2 magic");

    cpu::detect_features();
    const auto& f = cpu::features();
    serial::println("CPUID vendor=% max_basic=%", f.vendor, f.max_basic_leaf);
    serial::println(
        "CPUID apic=% x2apic=% sse=% sse2=% avx=% fxsr=% fsgsbase=% rdrand=% nx=%",
        f.apic, f.x2apic, f.sse, f.sse2, f.avx, f.fxsr, f.fsgsbase, f.rdrand, f.nx
    );
    serial::println("CPUID initial_apic_id=% cache_line_size=%", f.initial_apic_id, f.cache_line_size);

    serial::println("Initializing mem");
    mem::initialize(mbi);

    // Global constructors are called here, after the allocator is live, so any
    // constructor that calls operator new has a valid global allocator (must
    // be called after mem::initialize).
    run_global_constructors();

    serial::println("Parsing ACPI MADT");
    auto madt_ok = acpi::parse_madt(mbi);
    kstd_assert(madt_ok);

    serial::println("Initializing gfx");
    const auto gfx_initialized = gfx::initialize(mbi);
    kstd_assert(gfx_initialized);

    serial::println("Initializing term");
    const auto term_initialized = term::initialize();
    kstd_assert(term_initialized);

    gdt::initialize();
    idt::initialize();
    pic::initialize();
    lapic::initialize_bootstrap_processor();
    cpu_local::initialize_bootstrap_processor();
    ktime::initialize();
    ps2::initialize();
    idt::enable_interrupts();

    // Calibrate against the programmable interval timer, then hand ticks to
    // the local APIC and mask IRQ0 so both sources cannot advance ktime.
    lapic::calibrate_and_start_timer(ktime::TICK_RATE);

    pic::disable();
    ioapic::initialize();
    lapic::stop_listening_to_pic_by_masking_lint0();

    threads::initialize();
    ap::initialize_aps();
    if (ap::online_count() > 1) {
        ktime::set_tick_cpu(1);
        lapic::stop_timer();
    } else {
        ktime::set_tick_cpu(0);
    }
    threads::smoke_test();

    krand::initialize();
}
