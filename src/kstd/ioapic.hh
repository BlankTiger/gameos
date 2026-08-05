#pragma once

#include "local_apic.hh"
#include "serial_format.hh"

namespace ioapic {

auto initialize() -> void {
    using namespace lapic;

    auto lint_register = read_register_as<Local_Vector_Table_Lint_Register>(Register_Offset::LOCAL_VECTOR_TABLE_LINT0);
    serial::println("lint_register: %", lint_register);
}

}
