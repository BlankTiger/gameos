#pragma once

#include <atomic>

#include "kstd/basic.hh"

#include "gameos/synchronization.hh"

namespace tlb {

inline constexpr u64 FLUSH_ALL = ~psize{0};

struct Shootdown_State {
    synchronization::Spinlock lock;

    std::atomic<psize> virtual_address{FLUSH_ALL};
    std::atomic<u32> acknowledgments{0};
    std::atomic<u32> required_acknowledgments{0};
};

inline Shootdown_State state{};

}
