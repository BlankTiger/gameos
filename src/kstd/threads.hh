#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>

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

using Thread_Procedure = auto (*)(void*) -> void;

namespace hidden {

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
    void*            stack{};
    u64              stack_size{};
    mem::Allocator*  allocator{};
    void*            storage{};
    u64              storage_size{};
    u64              storage_alignment{};
    void*            typed_control{};
    void*            typed_result{};
    auto (*typed_destroy)(void*) -> void{};
    std::atomic<State> state{State::FREE};

    Thread() = default;

    Thread(const Thread& other)
        : context(other.context),
          procedure(other.procedure),
          argument(other.argument),
          stack(other.stack),
          stack_size(other.stack_size),
          allocator(other.allocator),
          storage(other.storage),
          storage_size(other.storage_size),
          storage_alignment(other.storage_alignment),
          typed_control(other.typed_control),
          typed_result(other.typed_result),
          typed_destroy(other.typed_destroy),
          state(other.state.load(std::memory_order_relaxed)) {}

    auto operator = (const Thread& other) -> Thread& {
        context           = other.context;
        procedure         = other.procedure;
        argument          = other.argument;
        stack             = other.stack;
        stack_size        = other.stack_size;
        allocator         = other.allocator;
        storage           = other.storage;
        storage_size      = other.storage_size;
        storage_alignment = other.storage_alignment;
        typed_control     = other.typed_control;
        typed_result      = other.typed_result;
        typed_destroy     = other.typed_destroy;
        state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }
};

static_assert(std::atomic<State>::is_always_lock_free);

}

template <typename Result_Type>
struct Thread_Handle {
    u32 index;

    bool operator == (const Thread_Handle&) const = default;
};

namespace hidden {
    inline Static_Array<Thread, THREAD_MAX_COUNT> threads;
    inline Ring_Buffer<u32,     THREAD_MAX_COUNT> ready_queue;
    inline synchronization::Spinlock ready_lock;

    inline Static_Array<Context, acpi::MAX_CPUS> idle_contexts;
    inline Static_Array<Thread*, acpi::MAX_CPUS> current_threads;
}

auto initialize() -> void {
    using namespace hidden;
    threads.fill({});
    current_threads.fill(nullptr);

    ready_queue.clear();
}

namespace hidden {

auto finish_current() -> void {
    const auto cpu = cpu_local::current().cpu_index;
    auto* thread = current_threads[cpu];
    kstd_assert(thread != nullptr, "No current thread even tho there should be one");

    thread->state.store(State::DONE, std::memory_order_release);
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

template <typename T>
auto enqueue_thread(Thread_Handle<T> handle) -> void {
    auto guard = ready_lock.scoped_irq_lock();
    kstd_assert(!ready_queue.full(), "Ready queue full");
    ready_queue.push_back(handle.index);
}

template <typename T>
auto create_thread(
    Thread_Procedure procedure,
    void*            data,
    void*            stack,
    u64              stack_size,
    mem::Allocator*  allocator,
    void*            storage,
    u64              storage_size,
    u64              storage_alignment,
    psize            entry
) -> Thread_Handle<T> {
    u32 handle = THREAD_MAX_COUNT;
    {
        auto guard = ready_lock.scoped_irq_lock();

        for (u32 index = 0; index < THREAD_MAX_COUNT; ++index) {
            if (threads[index].state.load(std::memory_order_relaxed) == State::FREE) {
                handle = index;
                break;
            }
        }

        kstd_assert(handle < THREAD_MAX_COUNT, "Thread table full");
        kstd_assert(!ready_queue.full(), "Ready queue full");
        threads[handle].state.store(State::READY, std::memory_order_relaxed);
    }

    auto& thread = threads[handle];
    thread.procedure         = procedure;
    thread.argument          = data;
    thread.stack             = stack;
    thread.stack_size        = stack_size;
    thread.allocator         = allocator;
    thread.storage           = storage;
    thread.storage_size      = storage_size;
    thread.storage_alignment = storage_alignment;

    auto stack_top = ptr_addr(thread.stack) + stack_size;
    stack_top &= ~u64{AP_STACK_ALIGNMENT - 1}; // Make sure it's really aligned correctly.
    stack_top -= 8; // SysV x86_64 ABI expects that at function entry rsp % 16 == 8

    *addr_as<u64*>(stack_top) = 0;

    thread.context = {};
    thread.context.rsp = stack_top;
    thread.context.rip = entry;

    return {.index = handle};
}

}

[[nodiscard]] auto spawn(Thread_Procedure procedure, void* data, u64 stack_size = THREAD_STACK_SIZE) -> Thread_Handle<void> {
    using namespace hidden;
    auto* allocator = mem::resolve_allocator();
    auto* stack = allocator->alloc(stack_size, AP_STACK_ALIGNMENT);
    kstd_assert(stack != nullptr, "Thread stack allocation failed");

    auto handle = create_thread<void>(
        procedure,
        data,
        stack,
        stack_size,
        allocator,
        stack,
        stack_size,
        AP_STACK_ALIGNMENT,
        ptr_addr(thread_start)
    );
    enqueue_thread(handle);

    return handle;
}

auto yield() -> void {
    using namespace hidden;
    auto cpu = cpu_local::current().cpu_index;

    Thread* current;
    {
        auto guard = ready_lock.scoped_irq_lock();

        current = current_threads[cpu];
        kstd_assert(current != nullptr);
        kstd_assert(current->state.load(std::memory_order_relaxed) == State::RUNNING);
        kstd_assert(!ready_queue.full());

        u32 handle = static_cast<u32>(current - threads.elements());

        current->state.store(State::READY, std::memory_order_relaxed);
        ready_queue.push_back(handle);

        current_threads[cpu] = nullptr;
    }

    threads_context_switch(&current->context, &idle_contexts[cpu]);
}

auto idle_poll() -> bool {
    using namespace hidden;
    auto cpu = cpu_local::current().cpu_index;

    u32 handle;
    {
        auto guard = ready_lock.scoped_irq_lock();

        if (ready_queue.empty()) return false;
        handle = ready_queue.pop_front();
    }

    auto& thread = threads[handle];
    kstd_assert(thread.state.load(std::memory_order_relaxed) == State::READY, "Dequeued thread is not ready");

    thread.state.store(State::RUNNING, std::memory_order_relaxed);
    current_threads[cpu] = &thread;

    threads_context_switch(&idle_contexts[cpu], &thread.context);
    current_threads[cpu] = nullptr;

    return true;
}


//
// Typed APIs.
//

namespace hidden {

template <typename Procedure, typename... Arguments>
using Procedure_Result_Type = std::invoke_result_t<Procedure&, std::decay_t<Arguments>&...>;

template <typename Procedure, typename Result_Type, typename... Arguments>
struct Typed_Control {
    Procedure                              procedure;
    std::tuple<std::decay_t<Arguments>...> arguments;
    alignas(Result_Type) u8                result_storage[sizeof(Result_Type)];
    bool                                   result_ready = false;

    auto result() -> Result_Type* {
        return reinterpret_cast<Result_Type*>(result_storage);
    }
};

template <typename Procedure, typename Result_Type, typename... Arguments>
auto run_typed_control(void* raw) -> void {
    using Control = Typed_Control<Procedure, Result_Type, Arguments...>;
    auto& control = *static_cast<Control*>(raw);

    new (control.result_storage) Result_Type(std::apply(
        [&](auto&... arguments) -> Result_Type {
            return std::invoke(control.procedure, arguments...);
        },
        control.arguments
    ));
    control.result_ready = true;
}

template <typename Procedure, typename Result_Type, typename... Arguments>
auto typed_thread_start() -> void {
    const auto cpu = cpu_local::current().cpu_index;
    auto* thread = current_threads[cpu];
    kstd_assert(thread != nullptr, "No current typed thread");

    run_typed_control<Procedure, Result_Type, Arguments...>(thread->typed_control);
    finish_current();

    unreachable("Finished typed thread resumed");
}

template <typename Procedure, typename Result_Type, typename... Arguments>
auto destroy_typed_control(void* raw) -> void {
    using Control = Typed_Control<Procedure, Result_Type, Arguments...>;
    auto* control = static_cast<Control*>(raw);

    if (control->result_ready)
        control->result()->~Result_Type();
    control->~Control();
}

template <typename Procedure, typename... Arguments>
concept Typed_Procedure =
    std::invocable<Procedure&, std::decay_t<Arguments>&...> &&
    !std::is_void_v<Procedure_Result_Type<Procedure, Arguments...>>;

}

// @TODO(blanktiger): Allow specifying thread parameters as the last argument (do an overload like in format.hh).
template <typename Procedure, typename... Arguments>
requires hidden::Typed_Procedure<Procedure, Arguments...>
auto spawn(Procedure procedure, Arguments&&... args) -> Thread_Handle<hidden::Procedure_Result_Type<Procedure, Arguments...>> {
    using namespace hidden;
    using Result_Type = Procedure_Result_Type<Procedure, Arguments...>;
    static_assert(!std::is_void_v<Result_Type>);
    static_assert(!std::is_reference_v<Result_Type>);

    // To reduce allocations we allocate only once for Control and for the thread's stack.
    using Control = Typed_Control<Procedure, Result_Type, Arguments...>;
    constexpr usize control_alignment = alignof(Control) > AP_STACK_ALIGNMENT ? alignof(Control) : AP_STACK_ALIGNMENT;
    constexpr usize stack_offset      = (sizeof(Control) + AP_STACK_ALIGNMENT - 1) & ~(AP_STACK_ALIGNMENT - 1);
    constexpr usize storage_size      = stack_offset + THREAD_STACK_SIZE;

    auto* allocator = mem::resolve_allocator();
    auto* storage = allocator->alloc(storage_size, control_alignment);
    auto* control = static_cast<Control*>(storage);
    kstd_assert(control != nullptr, "Typed thread control allocation failed");

    new (control) Control {
        .procedure      = std::move(procedure),
        .arguments      = std::tuple<std::decay_t<Arguments>...>(std::forward<Arguments>(args)...),
        .result_storage = {},
        .result_ready   = false,
    };

    auto* stack = addr_as<void*>(ptr_addr(storage) + stack_offset);
    auto handle = create_thread<Result_Type>(
        nullptr,
        nullptr,
        stack,
        THREAD_STACK_SIZE,
        allocator,
        storage,
        storage_size,
        control_alignment,
        ptr_addr(typed_thread_start<Procedure, Result_Type, Arguments...>)
    );

    auto& thread = threads[handle.index];
    thread.typed_control = control;
    thread.typed_result  = control->result_storage;
    thread.typed_destroy = &destroy_typed_control<Procedure, Result_Type, Arguments...>;
    enqueue_thread(handle);

    return handle;
}

template <typename Result_Type>
auto join(Thread_Handle<Result_Type> handle) -> Result_Type {
    using namespace hidden;
    kstd_assert(handle.index < THREAD_MAX_COUNT, "Invalid typed thread handle");

    const auto cpu = cpu_local::current().cpu_index;
    while (threads[handle.index].state.load(std::memory_order_acquire) != State::DONE) {
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

    auto guard = ready_lock.scoped_irq_lock();
    auto& thread = threads[handle.index];
    kstd_assert(thread.state.load(std::memory_order_acquire) == State::DONE, "Thread is not done");

    if constexpr (std::is_void_v<Result_Type>) {
        kstd_assert(thread.typed_control == nullptr, "Void handle belongs to typed thread");
        thread.allocator->free(thread.storage, thread.storage_size, thread.storage_alignment);
        thread = {};
        return;
    } else {
        kstd_assert(thread.typed_control != nullptr, "Result handle belongs to untyped thread");
        kstd_assert(thread.typed_result  != nullptr, "Thread has no typed result");
        kstd_assert(thread.typed_destroy != nullptr, "Thread has no typed destructor");

        auto* result = static_cast<Result_Type*>(thread.typed_result);
        Result_Type value = std::move(*result);

        thread.typed_destroy(thread.typed_control);
        thread.allocator->free(thread.storage, thread.storage_size, thread.storage_alignment);
        thread = {};

        return value;
    }

    unreachable();
}

auto smoke_test() -> void {
    // Will assert if we leaked anything in this smoke test.
    mem::Debug_Allocator dbg_allocator{mem::resolve_allocator()};
    PUSH_ALLOCATOR(&dbg_allocator);

    const auto smoke_proc = [](void* data) -> void {
        auto* sum = reinterpret_cast<u32*>(data);
        for (u32 index = 0; index < 100; ++index) {
            *sum += index;
        }
    };

    {
        u32 result = 0;
        auto thread = spawn(smoke_proc, &result);
        join(thread);

        constexpr auto expected = 4950;
        kstd_assert(result == expected, ctprint("Actual result: %, expected: %", result, expected));
    }

    // Verify yielding allows for switching tasks on the same cores.
    {
        const auto yield_test_proc = [](void* u32_id) -> void {
            auto id = *static_cast<u32*>(u32_id);

            for (u32 step = 0; step < 3; ++step) {
                serial::println("yield test task=% step=%", id, step);
                if (step != 2) yield();
            }
        };

        u32 id_a = 1;
        u32 id_b = 2;

        auto thread_a = spawn(yield_test_proc, &id_a);
        auto thread_b = spawn(yield_test_proc, &id_b);

        join(thread_a);
        join(thread_b);
    }

    // Verify handle reuse (thread is correctly freed after the work is done in join).
    {
        u32 result_a = 0;
        u32 result_b = 0;

        auto thread_a = spawn(smoke_proc, &result_a);
        join(thread_a);

        auto thread_b = spawn(smoke_proc, &result_b);
        join(thread_b);

        kstd_assert(thread_a == thread_b);
    }

    // Typed APIs
    {
        const auto typed_test_sum_proc = [](u32 start, u32 end) -> u64 {
            u64 sum = 0;
            for (u32 index = start; index < end; ++index) {
                sum += index;
            }
            return sum;
        };

        auto thread_a = spawn(typed_test_sum_proc, 1, 3);
        auto thread_b = spawn(typed_test_sum_proc, 2, 3);

        auto result_a = join(thread_a);
        auto result_b = join(thread_b);

        kstd_assert(result_a == 3);
        kstd_assert(result_b == 2);
    }
}

}
