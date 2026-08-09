#pragma once

#include <atomic>
#include <cstddef>

#include "advanced_configuration_and_power_interface.hh"
#include "allocator.hh"
#include "application_processor_state.hh"
#include "array.hh"
#include "assert.hh"
#include "basic.hh"
#include "cpu_local.hh"
#include "ring_buffer.hh"
#include "smp_constants.hh"
#include "synchronization.hh"
#include "string_builder.hh"

namespace threads {

struct Context {
    u64 rbx, rbp, r12, r13, r14, r15;
    u64 rsp, rip;
};

static_assert(sizeof(Context) == 64);
static_assert(offsetof(Context, rbx) == CONTEXT_SWITCH_OFFSET_RBX);
static_assert(offsetof(Context, rbp) == CONTEXT_SWITCH_OFFSET_RBP);
static_assert(offsetof(Context, r12) == CONTEXT_SWITCH_OFFSET_R12);
static_assert(offsetof(Context, r13) == CONTEXT_SWITCH_OFFSET_R13);
static_assert(offsetof(Context, r14) == CONTEXT_SWITCH_OFFSET_R14);
static_assert(offsetof(Context, r15) == CONTEXT_SWITCH_OFFSET_R15);
static_assert(offsetof(Context, rsp) == CONTEXT_SWITCH_OFFSET_RSP);
static_assert(offsetof(Context, rip) == CONTEXT_SWITCH_OFFSET_RIP);

extern "C" auto threads_context_switch(Context* previous, Context* next) -> void;

using Thread_Procedure = auto (*)(void*) -> void;

enum struct State : u8 {
    FREE,
    READY,
    RUNNING,
    DONE,
};

struct Thread {
    Context          context{};
    Thread_Procedure procedure{};
    void*            argument{};
    u8*              stack{};
    State            state = State::FREE;
};

inline Static_Array<Thread, THREAD_MAX_COUNT> threads;
inline Ring_Buffer<u32,     THREAD_MAX_COUNT> ready_queue;
inline synchronization::Spinlock ready_lock;

inline Static_Array<Context, acpi::MAX_CPUS> idle_contexts;
inline Static_Array<Thread*, acpi::MAX_CPUS> current_threads;

auto initialize() -> void {
    threads.fill({});
    current_threads.fill(nullptr);

    ready_queue.clear();
}

auto finish_current() -> void {
    const auto cpu = cpu_local::current().cpu_index;
    auto* thread = current_threads[cpu];
    kstd_assert(thread != nullptr, "No current thread even tho there should be one");

    thread->state = State::DONE;
    current_threads[cpu] = nullptr;

    threads_context_switch(&thread->context, &idle_contexts[cpu]);

    unreachable("Finished thread resumed");
}

auto thread_start() -> void {
    auto cpu = cpu_local::current().cpu_index;
    auto* thread = current_threads[cpu];

    thread->procedure(thread->argument);

    finish_current();

    unreachable("finish_current must never return");
}

// @TODO(blanktiger): Handle types.
[[nodiscard]] auto spawn(Thread_Procedure procedure, void* data, u64 stack_size = THREAD_STACK_SIZE) -> u32 {
    auto guard = ready_lock.scoped_irq_lock();

    u32 handle = THREAD_MAX_COUNT;
    for (u32 index = 0; index < THREAD_MAX_COUNT; ++index) {
        if (threads[index].state == State::FREE) {
            handle = index;
            break;
        }
    }
    kstd_assert(handle < THREAD_MAX_COUNT, "Thread table full");
    kstd_assert(!ready_queue.full(), "Ready queue full");

    auto& thread = threads[handle];
    thread.stack = reinterpret_cast<u8*>(mem::resolve_allocator()->alloc(stack_size, AP_STACK_ALIGNMENT));
    kstd_assert(thread.stack != nullptr, "Thread stack allocation failed");

    auto stack_top = ptr_addr(thread.stack) + stack_size;
    stack_top &= ~u64{AP_STACK_ALIGNMENT - 1}; // Make sure it's really aligned correctly.
    stack_top -= 8; // SysV x86_64 ABI expects that at function entry rsp % 16 == 8

    *addr_as<u64*>(stack_top) = 0;

    thread.context = {};
    thread.context.rsp = stack_top;
    thread.context.rip = ptr_addr(thread_start);
    thread.procedure   = procedure;
    thread.argument    = data;
    thread.state       = State::READY;

    ready_queue.push_back(handle);

    return handle;
}

auto yield() -> void {
    auto cpu = cpu_local::current().cpu_index;

    Thread* current;
    u32 handle;
    {
        auto guard = ready_lock.scoped_irq_lock();

        current = current_threads[cpu];
        kstd_assert(current != nullptr);
        kstd_assert(current->state == State::RUNNING);
        kstd_assert(!ready_queue.full());

        handle = static_cast<u32>(current - threads.elements());

        current->state = State::READY;
        ready_queue.push_back(handle);

        current_threads[cpu] = nullptr;
    }

    threads_context_switch(&current->context, &idle_contexts[cpu]);
}

auto idle_poll() -> bool {
    auto cpu = cpu_local::current().cpu_index;

    u32 handle;
    {
        auto guard = ready_lock.scoped_irq_lock();

        if (ready_queue.empty()) return false;

        handle = ready_queue.pop_front();
    }

    auto& thread = threads[handle];
    kstd_assert(thread.state == State::READY, "Dequeued thread is not ready");

    thread.state = State::RUNNING;
    current_threads[cpu] = &thread;

    threads_context_switch(&idle_contexts[cpu], &thread.context);
    current_threads[cpu] = nullptr;

    return true;
}

//
// @TODO(blanktiger): Currently BSP is treated differently from all the
// additional cores we manage to get. If there is more than 1 then we don't
// ever switch it's task. That however is a detail that the user of this
// library probably wants to control so we have to figure out a way to switch
// the core configuration in a way that doesn't hinder performance and is
// easy/intuitive to use.
//
auto join(u32 handle) -> void {
    // @TODO(blanktiger): Make state atomic or something like that, currently there is a race here.
    kstd_assert(handle < THREAD_MAX_COUNT, "Invalid thread handle");

    const auto cpu = cpu_local::current().cpu_index;
    while (threads[handle].state != State::DONE) {
        if (current_threads[cpu] != nullptr) {
            yield();
        } else if (ap::online_count() <= 1) {
            // No AP exists. BSP must run work.
            if (!idle_poll())
                asm volatile("pause");
        } else {
            // APs are available. Leave ready work for them.
            asm volatile("pause");
        }
    }
}

struct Yield_Test_Args {
    u32 id;
};

auto smoke_test() -> void {
    {
        const auto smoke_proc = [](void* data) -> void {
            auto* sum = reinterpret_cast<u32*>(data);
            for (u32 index = 0; index < 100; ++index) {
                *sum += index;
            }
        };

        u32 result = 0;
        auto thread = spawn(smoke_proc, &result);
        join(thread);

        constexpr auto expected = 4950;
        kstd_assert(result == expected, ctprint("Actual result: %, expected: %", result, expected));
    }

    {
        const auto yield_test_proc = [](void* data) -> void {
            auto* args = reinterpret_cast<Yield_Test_Args*>(data);

            for (u32 step = 0; step < 3; ++step) {
                serial::println("yield test task=% step=%", args->id, step);
                if (step != 2) yield();
            }
        };

        Yield_Test_Args args_a{.id = 1};
        Yield_Test_Args args_b{.id = 2};

        auto thread_a = spawn(yield_test_proc, &args_a);
        auto thread_b = spawn(yield_test_proc, &args_b);

        join(thread_a);
        join(thread_b);
    }
}

}
