#pragma once

#include <bit>

#include "kstd/basic.hh"

namespace gdt {

enum struct Executable : u8 {
    DATA = 0,
    CODE = 1,
};

enum struct Code_Or_Data : u8 {
    SYSTEM       = 0,
    CODE_OR_DATA = 1,
};

enum struct Descriptor_Privilege_Level : u8 {
    RING0     = 0,
    USERSPACE = 3,
};

enum struct Granularity : u8 {
    _1B   = 0,
    _4KiB = 1,
};

struct Segment_Descriptor {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;

    u8 accessed                    : 1;
    u8 rw                          : 1; // data    : writable; code   : readable
    u8 dc                          : 1; // data    : expand-down; code: conforming
    Executable executable          : 1;
    Code_Or_Data code_or_data      : 1;
    Descriptor_Privilege_Level dpl : 2;
    u8 present                     : 1;

    u8 limit_high                  : 4;
    u8 available                   : 1;
    u8 long_mode                   : 1; // L (code): 1 = 64-bit
    u8 op_size                     : 1; // D/B; must be 0 if long_mode
    Granularity granularity        : 1;

    u8 base_high;
} __attribute__((packed));

static_assert(size_of(Segment_Descriptor) == 8);
static_assert(std::bit_cast<u64>(Segment_Descriptor{}) == 0);

constexpr Segment_Descriptor KERNEL_CODE_SEGMENT_DESCRIPTOR = {
    .limit_low    = 0xFFFF,
    .base_low     = 0,
    .base_mid     = 0,
    .accessed     = 0,
    .rw           = 1,
    .dc           = 0,
    .executable   = Executable::CODE,
    .code_or_data = Code_Or_Data::CODE_OR_DATA,
    .dpl          = Descriptor_Privilege_Level::RING0,
    .present      = 1,
    .limit_high   = 0xF,
    .available    = 0,
    .long_mode    = 1,
    .op_size      = 0,
    .granularity  = Granularity::_4KiB,
    .base_high    = 0,
};

static_assert(std::bit_cast<u64>(KERNEL_CODE_SEGMENT_DESCRIPTOR) == 0x00AF9A000000FFFF);

constexpr Segment_Descriptor KERNEL_DATA_SEGMENT_DESCRIPTOR = {
    .limit_low    = 0xFFFF,
    .base_low     = 0,
    .base_mid     = 0,
    .accessed     = 0,
    .rw           = 1,
    .dc           = 0,
    .executable   = Executable::DATA,
    .code_or_data = Code_Or_Data::CODE_OR_DATA,
    .dpl          = Descriptor_Privilege_Level::RING0,
    .present      = 1,
    .limit_high   = 0xF,
    .available    = 0,
    .long_mode    = 0,
    .op_size      = 1,
    .granularity  = Granularity::_4KiB,
    .base_high    = 0,
};

static_assert(std::bit_cast<u64>(KERNEL_DATA_SEGMENT_DESCRIPTOR) == 0x00CF92000000FFFF);

struct Task_State_Segment {
    u32 reserved0;
    u64 rsp0;
    u64 rsp1;
    u64 rsp2;
    u64 reserved1;
    u64 ist1;
    u64 ist2;
    u64 ist3;
    u64 ist4;
    u64 ist5;
    u64 ist6;
    u64 ist7;
    u64 reserved2;
    u16 reserved3;
    u16 iomap_base;
} __attribute__((packed));

static_assert(size_of(Task_State_Segment) == 104);

// 16-byte system descriptor (occupies two GDT slots).
struct Task_State_Segment_Descriptor {
    u16 limit_low;
    u16 base_low;
    u8  base_mid;

    u8 type         : 4; // 0x9 = available 64-bit TSS
    u8 code_or_data : 1; // S: must be 0 (system)
    u8 dpl          : 2;
    u8 present      : 1;

    u8 limit_high   : 4;
    u8 available    : 1;
    u8 long_mode    : 1; // must be 0 for TSS
    u8 op_size      : 1; // must be 0 for TSS
    u8 granularity  : 1;

    u8  base_high;
    u32 base_upper;
    u32 reserved;
} __attribute__((packed));

static_assert(size_of(Task_State_Segment_Descriptor) == 16);

struct Global_Descriptor_Table_Register {
    u16   limit;
    psize base;
} __attribute__((packed));

static_assert(size_of(Global_Descriptor_Table_Register) == 10);

struct Global_Descriptor_Table {
    Segment_Descriptor null{};
    Segment_Descriptor code = KERNEL_CODE_SEGMENT_DESCRIPTOR;
    Segment_Descriptor data = KERNEL_DATA_SEGMENT_DESCRIPTOR;
    Task_State_Segment_Descriptor tss{};
} __attribute__((packed));

static_assert(size_of(Global_Descriptor_Table) == 40);

namespace hidden {
    constexpr usize IST1_STACK_SIZE = 4 * 1024;

    alignas(16) inline Global_Descriptor_Table global_descriptor_table{};
    inline Global_Descriptor_Table_Register    global_descriptor_table_register{};
    alignas(16) inline Task_State_Segment      tss{};
    alignas(16) inline u8                      ist1_stack[IST1_STACK_SIZE]{};
}

// Matches boot.S temp GDT: null @ 0x00, 64-bit code @ 0x08, data @ 0x10.
// TSS @ 0x18 (16-byte descriptor).
constexpr u16 KERNEL_CODE_SEGMENT = 0x08;
constexpr u16 KERNEL_DATA_SEGMENT = 0x10;
constexpr u16 TASK_STATE_SEGMENT  = 0x18;

auto make_tss_descriptor(Task_State_Segment* tss_ptr) -> Task_State_Segment_Descriptor {
    auto base  = reinterpret_cast<u64>(tss_ptr);
    auto limit = static_cast<u32>(size_of(Task_State_Segment) - 1);

    return Task_State_Segment_Descriptor{
        .limit_low    = static_cast<u16>(limit),
        .base_low     = static_cast<u16>(base),
        .base_mid     = static_cast<u8>(base >> 16),
        .type         = 0x9,
        .code_or_data = 0,
        .dpl          = 0,
        .present      = 1,
        .limit_high   = static_cast<u8>((limit >> 16) & 0xF),
        .available    = 0,
        .long_mode    = 0,
        .op_size      = 0,
        .granularity  = 0,
        .base_high    = static_cast<u8>(base >> 24),
        .base_upper   = static_cast<u32>(base >> 32),
        .reserved     = 0,
    };
}

auto initialize() -> void {
    using namespace hidden;

    tss.ist1       = reinterpret_cast<u64>(ist1_stack + IST1_STACK_SIZE);
    tss.iomap_base = static_cast<u16>(size_of(Task_State_Segment));

    global_descriptor_table.code = KERNEL_CODE_SEGMENT_DESCRIPTOR;
    global_descriptor_table.data = KERNEL_DATA_SEGMENT_DESCRIPTOR;
    global_descriptor_table.tss  = make_tss_descriptor(&tss);

    global_descriptor_table_register.limit = size_of(global_descriptor_table) - 1;
    global_descriptor_table_register.base  = reinterpret_cast<psize>(&global_descriptor_table);

    asm volatile("lgdt %0" : : "m"(global_descriptor_table_register));

    asm volatile(
        "mov %[sel], %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        "mov %%ax, %%gs\n"
        :
        : [sel] "r"(KERNEL_DATA_SEGMENT)
        : "ax"
    );

    asm volatile("ltr %0" : : "m"(TASK_STATE_SEGMENT));
}

auto load_shared() -> void {
    asm volatile("lgdt %0" : : "m"(hidden::global_descriptor_table_register));

    asm volatile(
        "mov %[sel], %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%ss\n"
        :
        : [sel] "r"(KERNEL_DATA_SEGMENT)
        : "ax"
    );

    asm volatile(
        "pushq %[code_sel]\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        :
        : [code_sel] "i"(KERNEL_CODE_SEGMENT)
        : "rax", "memory"
    );
}

}
