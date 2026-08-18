#pragma once

#include <atomic>

namespace synchronization {

struct Spinlock {
    std::atomic_flag locked{};

    auto lock() -> void {
        while (locked.test_and_set(std::memory_order_acquire)) {}
    }

    auto try_lock() -> bool {
        return !locked.test_and_set(std::memory_order_acquire);
    }

    auto unlock() -> void {
        locked.clear(std::memory_order_release);
    }

    struct Scoped {
        Spinlock& spinlock;

        explicit Scoped(Spinlock& lock) : spinlock(lock) { spinlock.lock(); }
        ~Scoped() { spinlock.unlock(); }

        Scoped(const Scoped&) = delete;
        auto operator=(const Scoped&) -> Scoped& = delete;
    };

    [[nodiscard]] auto scoped_lock() -> Scoped { return Scoped{*this}; }

    // For gameos tests.
    struct Scoped_IRQ { ~Scoped_IRQ() {} };
    [[nodiscard]] auto scoped_irq_lock() -> Scoped_IRQ { return Scoped_IRQ{}; }
};

}
