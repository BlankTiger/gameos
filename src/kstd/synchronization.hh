#pragma once

#include <atomic>

#include "basic.hh"

namespace sync {

#if HOSTED

force_inline auto interrupts_enabled() -> bool {
    return true;
}

force_inline auto disable_interrupts() -> void {}

force_inline auto enable_interrupts() -> void {}

#else

force_inline auto interrupts_enabled() -> bool {
    u64 flags;
    asm volatile("pushfq\n\tpopq %0" : "=r"(flags));
    return (flags & (1ull << 9)) != 0;  // RFLAGS.IF
}

force_inline auto disable_interrupts() -> void {
    asm volatile("cli");
}

force_inline auto enable_interrupts() -> void {
    asm volatile("sti");
}

#endif

struct Interrupt_Guard {
    bool enabled;

    Interrupt_Guard() : enabled(interrupts_enabled()) {
        disable_interrupts();
    }

    ~Interrupt_Guard() {
        if (enabled) enable_interrupts();
    }
};

struct Spinlock {
    std::atomic_flag locked{};

    auto lock() -> void {
        while (locked.test_and_set(std::memory_order_acquire))
            asm volatile("pause");
    }

    auto try_lock() -> bool {
        return !locked.test_and_set(std::memory_order_acquire);
    }

    auto unlock() -> void {
        locked.clear(std::memory_order_release);
    }

    struct Scoped {
        Spinlock& spinlock;

        explicit Scoped(Spinlock& lock) : spinlock(lock) {
            spinlock.lock();
        }

        ~Scoped() {
            spinlock.unlock();
        }

        Scoped(const Scoped&) = delete;
        auto operator=(const Scoped&) -> Scoped& = delete;
    };

    [[nodiscard]] auto scoped_lock() -> Scoped {
        return Scoped{*this};
    }

    struct Scoped_IRQ {
        Spinlock&       spinlock;
        Interrupt_Guard irq_guard;

        explicit Scoped_IRQ(Spinlock& lock) : spinlock(lock) {
            spinlock.lock();
        }

        ~Scoped_IRQ() {
            spinlock.unlock();
        }

        Scoped_IRQ(const Scoped_IRQ&) = delete;
        auto operator=(const Scoped_IRQ&) -> Scoped_IRQ& = delete;
    };

    [[nodiscard]] auto scoped_irq_lock() -> Scoped_IRQ {
        return Scoped_IRQ{*this};
    }
};

}
