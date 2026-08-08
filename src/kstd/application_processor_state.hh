#pragma once

#include "advanced_configuration_and_power_interface.hh"
#include "array.hh"
#include "cpu_local.hh"

namespace ap {

alignas(64) inline Static_Array<volatile bool, acpi::MAX_CPUS> cpus_online;
alignas(64) inline Static_Array<volatile bool, acpi::MAX_CPUS> cpus_frozen;

template <bool EXCLUDE_SELF = false>
auto online_count() -> u32 {
    u32 count = 0;
    u32 self = cpu_local::current().cpu_index;
    for (u32 index = 0; index < acpi::MAX_CPUS; ++index) {
        if ((EXCLUDE_SELF && index == self) || !cpus_online[index]) continue;
        ++count;
    }
    return count;
}

force_inline auto online_count_excluding_self() -> u32 {
    return online_count<true>();
}

}
