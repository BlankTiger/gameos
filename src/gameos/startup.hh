#pragma once

#include "kstd/assert.hh"

#include "gameos/advanced_configuration_and_power_interface.hh"
#include "gameos/application_processor.hh"
#include "gameos/cpu_local.hh"
#include "gameos/cpuid.hh"
#include "gameos/dwarf.hh"
#include "gameos/gfx.hh"
#include "gameos/global_constructor_handling.hh"
#include "gameos/global_descriptors.hh"
#include "gameos/interrupts.hh"
#include "gameos/input.hh"
#include "gameos/ioapic.hh"
#include "gameos/local_apic.hh"
#include "gameos/memory.hh"
#include "gameos/multiboot2.hh"
#include "gameos/power.hh"
#include "gameos/programmable_interrupt_controller.hh"
#include "gameos/ps2.hh"
#include "gameos/random.hh"
#include "gameos/serial_format.hh"
#include "gameos/term.hh"
#include "gameos/thread_local_storage.hh"
#include "gameos/threads.hh"
#include "gameos/time.hh"
#include "gameos/translation_lookaside_buffer.hh"

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
    tls::initialize_bsp({
        .allocator           = &mem::buddy,
        .temporary_allocator = &mem::temporary_allocator,
    });

    // Global constructors are called here, after the allocator is live, so any
    // constructor that calls operator new has a valid global allocator (must
    // be called after mem::initialize).
    run_global_constructors();

    dwarf::build_debug_info();

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
    ioapic::route_device_irqs_to_application_processor();
    // Keep ticks on the BSP. kernel_main runs there and sleep_ticks() depends
    // on its local timer to wake the graphics loop.
    ktime::set_tick_cpu(0);

    if (ap::online_count() > 1)
        threads::smoke_test();

    krand::initialize();
}
