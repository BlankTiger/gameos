#pragma once

#include "assert.hh"
#include "basic.hh"
#include "cpuid.hh"
#include "low_level_io.hh"
#include "pointer_utils.hh"
#include "serial_format.hh"
#include "time.hh"

//
// Local Advanced Programmable Interrupt Controller in xAPIC memory-mapped mode.
// The default physical base is usually 0xFEE00000. Read it from IA32_APIC_BASE.
// Register layouts follow Intel SDM Vol. 3 (Local APIC).
//
namespace lapic {

// Interrupt Descriptor Table vectors for the local APIC.
// These sit outside the 8259 remap window (32 through 47).
constexpr u8 TIMER_INTERRUPT_VECTOR    = 64;
constexpr u8 SPURIOUS_INTERRUPT_VECTOR = 255;

// Byte offsets from the local APIC memory-mapped base.
enum struct Register_Offset : u32 {
    ID                         = 0x020,
    VERSION                    = 0x030,
    TASK_PRIORITY              = 0x080,
    END_OF_INTERRUPT           = 0x0B0,
    SPURIOUS_INTERRUPT_VECTOR  = 0x0F0,
    INTERRUPT_COMMAND_LOW      = 0x300,
    INTERRUPT_COMMAND_HIGH     = 0x310,
    LOCAL_VECTOR_TABLE_TIMER   = 0x320,
    LOCAL_VECTOR_TABLE_LINT0   = 0x350,
    LOCAL_VECTOR_TABLE_LINT1   = 0x360,
    LOCAL_VECTOR_TABLE_ERROR   = 0x370,
    TIMER_INITIAL_COUNT        = 0x380,
    TIMER_CURRENT_COUNT        = 0x390,
    TIMER_DIVIDE_CONFIGURATION = 0x3E0,
};

constexpr u32 IA32_APIC_BASE_MODEL_SPECIFIC_REGISTER_INDEX = 0x1B;

union IA32_Apic_Base_Model_Specific_Register {
    struct {
        u64 reserved_low        : 8;
        u64 bootstrap_processor : 1;  // BSP flag (read-only)
        u64 reserved_bit_9      : 1;
        u64 x2apic_mode_enable  : 1;  // 1 = x2APIC (MSRs); keep 0 for xAPIC
        u64 global_enable       : 1;  // APIC global enable
        u64 physical_base_frame : 52; // physical base bits 63:12 (4 KiB aligned)
    };
    u64 raw;
} __attribute__((packed));

static_assert(sizeof(IA32_Apic_Base_Model_Specific_Register) == 8);

// At Register_Offset::ID
union Identification_Register {
    struct {
        u32 reserved : 24;
        u32 apic_id  : 8; // bits 31:24
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Identification_Register) == 4);

// At Register_Offset::VERSION
union Version_Register {
    struct {
        u32 version                                       : 8;
        u32 reserved_low                                  : 8;
        u32 max_local_vector_table_entry                  : 8; // number of LVT entries minus one
        u32 suppress_end_of_interrupt_broadcast_supported : 1;
        u32 reserved_high                                 : 7;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Version_Register) == 4);

// At Register_Offset::TASK_PRIORITY
union Task_Priority_Register {
    struct {
        u32 priority_sub_class : 4;
        u32 priority_class     : 4;
        u32 reserved           : 24;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Task_Priority_Register) == 4);

// At Register_Offset::SPURIOUS_INTERRUPT_VECTOR
union Spurious_Interrupt_Vector_Register {
    struct {
        u32 vector                                 : 8;
        u32 apic_software_enable                   : 1;
        u32 focus_processor_checking               : 1; // Legacy. Usually 0.
        u32 reserved_mid                           : 2;
        u32 end_of_interrupt_broadcast_suppression : 1;
        u32 reserved_high                          : 19;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Spurious_Interrupt_Vector_Register) == 4);

enum struct Timer_Mode : u32 {
    ONE_SHOT                   = 0b00,
    PERIODIC                   = 0b01,
    TIMESTAMP_COUNTER_DEADLINE = 0b10,
};

// At Register_Offset::LOCAL_VECTOR_TABLE_TIMER
union Local_Vector_Table_Timer_Register {
    struct {
        u32        vector          : 8;
        u32        reserved_low    : 4;
        u32        delivery_status : 1; // read-only
        u32        reserved_mid    : 3;
        u32        masked          : 1;
        Timer_Mode timer_mode      : 2;
        u32        reserved_high   : 13;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Local_Vector_Table_Timer_Register) == 4);

enum struct Delivery_Mode : u32 {
    FIXED                       = 0b000,
    LOWEST_PRIORITY             = 0b001,
    SYSTEM_MANAGEMENT_INTERRUPT = 0b010,
    NON_MASKABLE_INTERRUPT      = 0b100,
    INIT                        = 0b101,
    START_UP                    = 0b110,
    EXTERNAL_INTERRUPT          = 0b111,
};

// At Register_Offset::LOCAL_VECTOR_TABLE_LINT0 and Register_Offset::LOCAL_VECTOR_TABLE_LINT1
union Local_Vector_Table_Lint_Register {
    struct {
        u32           vector                       : 8;
        Delivery_Mode delivery_mode                : 3;
        u32           reserved_bit_11              : 1;
        u32           delivery_status              : 1; // read-only
        u32           interrupt_input_pin_polarity : 1;
        u32           remote_interrupt_request     : 1; // read-only
        u32           trigger_mode                 : 1; // 0 = edge, 1 = level
        u32           masked                       : 1;
        u32           reserved_high                : 15;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Local_Vector_Table_Lint_Register) == 4);

// Local Vector Table error entry at offset 0x370.
union Local_Vector_Table_Error_Register {
    struct {
        u32 vector          : 8;
        u32 reserved_low    : 4;
        u32 delivery_status : 1; // read-only
        u32 reserved_mid    : 3;
        u32 masked          : 1;
        u32 reserved_high   : 15;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Local_Vector_Table_Error_Register) == 4);

//
// At Register_Offset::TIMER_DIVIDE_CONFIGURATION
//
// The divisor uses bits 0, 1, and 3. Bit 2 stays 0.
// Encoding: 000=2, 001=4, 010=8, 011=16, 100=32, 101=64, 110=128, 111=1.
//
union Timer_Divide_Configuration_Register {
    struct {
        u32 divide_select_low  : 2; // encoding bits 1:0
        u32 reserved_bit_2     : 1; // must be 0
        u32 divide_select_high : 1; // encoding bit 2 (hardware bit 3)
        u32 reserved_high      : 28;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Timer_Divide_Configuration_Register) == 4);

// divide_select_high:low = 0b011 selects divide by 16.
constexpr Timer_Divide_Configuration_Register TIMER_DIVIDE_BY_16 = {
    .divide_select_low  = 0b11,
    .reserved_bit_2     = 0,
    .divide_select_high = 0,
    .reserved_high      = 0,
};

enum struct Destination_Mode : u32 {
    PHYSICAL = 0,
    LOGICAL  = 1,
};

enum struct Destination_Shorthand : u32 {
    NONE               = 0b00,
    SELF               = 0b01,
    ALL_INCLUDING_SELF = 0b10,
    ALL_EXCLUDING_SELF = 0b11,
};

// At Register_Offset::INTERRUPT_COMMAND_LOW
union Interrupt_Command_Register_Low {
    struct {
        u32                   vector                : 8;
        Delivery_Mode         delivery_mode         : 3;
        Destination_Mode      destination_mode      : 1;
        u32                   delivery_pending      : 1; // read-only; 1 while delivery in flight
        u32                   reserved_bit_13       : 1;
        u32                   level                 : 1; // 0 = deassert, 1 = assert (INIT)
        u32                   trigger_mode          : 1; // 0 = edge, 1 = level
        u32                   reserved_mid          : 2;
        Destination_Shorthand destination_shorthand : 2;
        u32                   reserved_high         : 12;
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Interrupt_Command_Register_Low) == 4);

// At Register_Offset::INTERRUPT_COMMAND_HIGH (xAPIC).
union Interrupt_Command_Register_High {
    struct {
        u32 reserved            : 24;
        u32 destination_apic_id : 8; // bits 31:24
    };
    u32 raw;
} __attribute__((packed));

static_assert(sizeof(Interrupt_Command_Register_High) == 4);

namespace hidden {
    inline volatile u32* memory_mapped_registers = nullptr;
    inline u32 bootstrap_processor_apic_id = 0;
    inline u32 timer_counts_per_second = 0;
}

force_inline auto memory_mapped_register_base() -> volatile u32* {
    return hidden::memory_mapped_registers;
}

force_inline auto read_register(Register_Offset offset) -> u32 {
    const auto byte_offset = static_cast<u32>(offset);
    kstd_debug_assert(hidden::memory_mapped_registers != nullptr);
    kstd_debug_assert((byte_offset & 3) == 0);  // Make sure byte_offset is divisible by 4, because we are shifting a u32*
    return *ptr_offset(hidden::memory_mapped_registers, byte_offset);
}

force_inline auto write_register(Register_Offset offset, u32 value) -> void {
    const auto byte_offset = static_cast<u32>(offset);
    kstd_debug_assert(hidden::memory_mapped_registers != nullptr);
    kstd_debug_assert((byte_offset & 3) == 0);  // Make sure byte_offset is divisible by 4, because we are shifting a u32*
    *ptr_offset(hidden::memory_mapped_registers, byte_offset) = value;
}

template <typename Register>
force_inline auto read_register_as(Register_Offset offset) -> Register {
    Register value{};
    value.raw = read_register(offset);
    return value;
}

template <typename Register>
force_inline auto write_register(Register_Offset offset, Register value) -> void {
    write_register(offset, value.raw);
}

force_inline auto signal_end_of_interrupt() -> void {
    write_register(Register_Offset::END_OF_INTERRUPT, 0u);
}

force_inline auto local_apic_id() -> u32 {
    return read_register_as<Identification_Register>(Register_Offset::ID).apic_id;
}

// Can be called only after calling initialize_bootstrap_processor.
force_inline auto bootstrap_processor_apic_id() -> u32 {
    return hidden::bootstrap_processor_apic_id;
}

force_inline auto physical_base_address(IA32_Apic_Base_Model_Specific_Register value) -> u64 {
    auto physical_base_frame = static_cast<u64>(value.physical_base_frame);
     // Look at the memory layout of the register type to understand the shift.
    return physical_base_frame << (64 - 52);
}

auto resolve_memory_mapped_base() -> void {
    IA32_Apic_Base_Model_Specific_Register apic_base{};
    apic_base.raw = low_level_io::read_model_specific_register(
        IA32_APIC_BASE_MODEL_SPECIFIC_REGISTER_INDEX
    );

    kstd_assert(apic_base.global_enable != 0,      "IA32_APIC_BASE global enable bit clear");
    kstd_assert(apic_base.x2apic_mode_enable == 0, "x2APIC enabled; this path is xAPIC memory-mapped only for now");

    const u64 physical_base = physical_base_address(apic_base);
    hidden::memory_mapped_registers = reinterpret_cast<volatile u32*>(physical_base);
}

// Enable the local APIC in software on the bootstrap processor.
// Leave LINT0 unmasked so the 8259 virtual-wire path still works.
auto initialize_bootstrap_processor() -> void {
    kstd_assert(cpu::features().apic, "CPUID reports no local APIC");

    resolve_memory_mapped_base();

    write_register(
        Register_Offset::SPURIOUS_INTERRUPT_VECTOR,
        Spurious_Interrupt_Vector_Register {
            .vector                                 = SPURIOUS_INTERRUPT_VECTOR,
            .apic_software_enable                   = 1,
            .focus_processor_checking               = 0,
            .reserved_mid                           = 0,
            .end_of_interrupt_broadcast_suppression = 0,
            .reserved_high                          = 0,
        }
    );

    write_register(
        Register_Offset::TASK_PRIORITY,
        Task_Priority_Register {
            .priority_sub_class = 0,
            .priority_class     = 0,
            .reserved           = 0,
        }
    );

    // Mask the timer and error entries until they are programmed.
    // Do not mask LINT0. With the local APIC enabled, legacy 8259 delivery
    // uses ExtINT on LINT0.
    write_register(
        Register_Offset::LOCAL_VECTOR_TABLE_TIMER,
        Local_Vector_Table_Timer_Register {
            .vector          = TIMER_INTERRUPT_VECTOR,
            .reserved_low    = 0,
            .delivery_status = 0,
            .reserved_mid    = 0,
            .masked          = 1,
            .timer_mode      = Timer_Mode::ONE_SHOT,
            .reserved_high   = 0,
        }
    );
    write_register(
        Register_Offset::LOCAL_VECTOR_TABLE_ERROR,
        Local_Vector_Table_Error_Register {
            .vector          = 0,
            .reserved_low    = 0,
            .delivery_status = 0,
            .reserved_mid    = 0,
            .masked          = 1,
            .reserved_high   = 0,
        }
    );

    hidden::bootstrap_processor_apic_id = local_apic_id();

    const auto version = read_register_as<Version_Register>(Register_Offset::VERSION);
    serial::println(
        "local APIC id=% version=% max_lvt=% base=%",
        hidden::bootstrap_processor_apic_id,
        version.version,
        version.max_local_vector_table_entry,
        reinterpret_cast<psize>(hidden::memory_mapped_registers)
    );
}

//
// Calibrate the local APIC timer against the programmable interval timer that
// already drives ktime. Then start the local APIC timer at frequency_hz.
//
// The caller must enable interrupts and run ktime before this call. After
// return, mask programmable interval timer IRQ0 so only the local APIC
// advances ktime.
//
auto calibrate_and_start_timer(u32 frequency_hz) -> void {
    kstd_assert(hidden::memory_mapped_registers != nullptr);
    kstd_assert(frequency_hz > 0);

    constexpr u64 CALIBRATION_DURATION_MILLISECONDS = 50;

    write_register(Register_Offset::TIMER_DIVIDE_CONFIGURATION, TIMER_DIVIDE_BY_16);

    // One-shot and masked: count down without an interrupt.
    write_register(
        Register_Offset::LOCAL_VECTOR_TABLE_TIMER,
        Local_Vector_Table_Timer_Register {
            .vector          = TIMER_INTERRUPT_VECTOR,
            .reserved_low    = 0,
            .delivery_status = 0,
            .reserved_mid    = 0,
            .masked          = 1,
            .timer_mode      = Timer_Mode::ONE_SHOT,
            .reserved_high   = 0,
        }
    );
    write_register(Register_Offset::TIMER_INITIAL_COUNT, 0xFFFFFFFFu);

    ktime::sleep_ms(CALIBRATION_DURATION_MILLISECONDS);

    const u64 counts_remaining = read_register(Register_Offset::TIMER_CURRENT_COUNT);
    const u64 counts_elapsed   = 0xFFFFFFFFu - counts_remaining;
    kstd_assert(counts_elapsed > 0, "local APIC timer did not count during calibration");

    hidden::timer_counts_per_second = static_cast<u32>(counts_elapsed * 1000 / CALIBRATION_DURATION_MILLISECONDS);
    kstd_assert(hidden::timer_counts_per_second >= frequency_hz);

    const u32 counts_per_interrupt = hidden::timer_counts_per_second / frequency_hz;
    kstd_assert(counts_per_interrupt > 0, "local APIC timer counts per interrupt underflows");

    write_register(
        Register_Offset::LOCAL_VECTOR_TABLE_TIMER,
        Local_Vector_Table_Timer_Register {
            .vector          = TIMER_INTERRUPT_VECTOR,
            .reserved_low    = 0,
            .delivery_status = 0,
            .reserved_mid    = 0,
            .masked          = 0,
            .timer_mode      = Timer_Mode::PERIODIC,
            .reserved_high   = 0,
        }
    );
    write_register(Register_Offset::TIMER_INITIAL_COUNT, counts_per_interrupt);

    serial::println(
        "local APIC timer counts_per_second=% counts_per_interrupt=% frequency_hz=%",
        hidden::timer_counts_per_second,
        counts_per_interrupt,
        frequency_hz
    );
}

// Write destination_apic_id into Interrupt Command Register high bits 31:24
// (xAPIC). The command holds the vector, delivery mode, level, trigger, and
// shorthand. Poll delivery_pending until it clears.
auto send_inter_processor_interrupt(
    u32 destination_apic_id,
    Interrupt_Command_Register_Low command
) -> void {
    kstd_assert(hidden::memory_mapped_registers != nullptr);
    kstd_assert(destination_apic_id <= 0xFF);

    write_register(
        Register_Offset::INTERRUPT_COMMAND_HIGH,
        Interrupt_Command_Register_High {
            .reserved            = 0,
            .destination_apic_id = destination_apic_id,
        }
    );
    write_register(Register_Offset::INTERRUPT_COMMAND_LOW, command);

    while (read_register_as<Interrupt_Command_Register_Low>(Register_Offset::INTERRUPT_COMMAND_LOW).delivery_pending) {
        asm volatile("pause");
    }
}

}
