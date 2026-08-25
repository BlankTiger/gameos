#pragma once

#include <cstddef>
#include <new>
#include <type_traits>

#include "kstd/allocator_types.hh"
#include "kstd/context.hh"
#include "kstd/basic.hh"
#include "kstd/assert.hh"
#include "kstd/cstring.hh"
#include "kstd/enum_flags.hh"
#include "kstd/math.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/synchronization.hh"

template <typename T, usize N>
struct Static_Array;

namespace mem {

constexpr usize TEMPORARY_STORAGE_SIZE = 16 * 1024;


force_inline auto resolve_allocator(Allocator allocator) -> Allocator;

force_inline constexpr auto result(void* memory, Allocator_Error error = Allocator_Error::NONE) -> Allocator_Result {
    return { .memory = memory, .error = error };
}

template <typename T>
force_inline constexpr auto query_result(T&& value, Allocator_Query_Error error = Allocator_Query_Error::NONE) -> Allocator_Query_Result<std::remove_cvref_t<T>> {
    return { .value = value, .error = error };
}

force_inline auto get_features(Allocator allocator) -> Allocator_Features;

force_inline auto call_allocator(Allocator allocator, Allocator_Mode mode, s64 size, s64 alignment, s64 old_size, void* old_memory) -> Allocator_Result {
    //
    // @NOTE: We don't resolve the allocator here, because we assume that what
    // you pass is what you actually want to use, and because this is a low
    // level helper. Higher level helpers like alloc / free do resolve the
    // allocator and use the one from the context if you don't provide an
    // override exactly because they provide the option to not pass any
    // allocator at all.
    //
    return allocator.proc(mode, size, alignment, old_size, old_memory, allocator.data);
}

force_inline auto alloc(usize size, usize alignment = alignof(std::max_align_t), Allocator allocator = {}) -> Allocator_Result {
    if (size > static_cast<usize>(S64_MAX))
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    if (alignment == 0 || (!math::is_power_of_two(alignment) || alignment > static_cast<usize>(S64_MAX)))
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    allocator = resolve_allocator(allocator);
    return call_allocator(allocator, Allocator_Mode::ALLOCATE, static_cast<s64>(size), static_cast<s64>(alignment), 0, nullptr);
}

force_inline auto alloc(usize size, Allocator allocator) -> Allocator_Result {
    return alloc(size, alignof(std::max_align_t), allocator);
}

force_inline auto free(void* pointer, usize size, usize alignment, Allocator allocator = {}) -> Allocator_Error {
    if (size > static_cast<usize>(S64_MAX) || alignment > static_cast<usize>(S64_MAX))
        return Allocator_Error::INVALID_ARGUMENT;

    allocator = resolve_allocator(allocator);
    auto result = call_allocator(allocator, Allocator_Mode::FREE, 0, static_cast<s64>(alignment), static_cast<s64>(size), pointer);
    return result.error;
}

force_inline auto free(void* pointer, usize size = 0, Allocator allocator = {}) -> Allocator_Error {
    return free(pointer, size, 0, allocator);
}

//
// The assumption is that you want to (AND HAVE TO) use the same allocator you
// used for the original allocation.
//
force_inline auto realloc(void* pointer, usize old_size, usize new_size, usize alignment, Allocator allocator = {}) -> Allocator_Result {
    if (old_size > static_cast<usize>(S64_MAX) || new_size > static_cast<usize>(S64_MAX))
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    if (pointer == nullptr && new_size == 0) return result(nullptr);

    allocator = resolve_allocator(allocator);
    auto direct = call_allocator(allocator, Allocator_Mode::RESIZE, static_cast<s64>(new_size), static_cast<s64>(alignment), static_cast<s64>(old_size), pointer);
    if (direct.error != Allocator_Error::MODE_NOT_IMPLEMENTED)
        return direct;

    const auto features = get_features(allocator);
    const bool can_free = has_flag(features, Allocator_Features::FREE);

    if (!can_free)
        return direct;

    if (pointer != nullptr && old_size == 0)
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    if (new_size == 0) {
        auto error = free(pointer, old_size, alignment, allocator);
        return result(nullptr, error);
    }

    const bool resize_shrink_is_no_op = has_flag(features, Allocator_Features::RESIZE_SHRINK_NO_OP);
    if (pointer != nullptr && new_size <= old_size && resize_shrink_is_no_op)
        return result(pointer);

    auto allocation = alloc(new_size, alignment, allocator);
    if (allocation.memory == nullptr) return allocation;

    const auto copy_size = old_size < new_size ? old_size : new_size;
    if (pointer != nullptr && copy_size > 0)
        kstd_memcpy(allocation.memory, pointer, copy_size);

    auto error = free(pointer, old_size, alignment, allocator);
    if (error != Allocator_Error::NONE) {
        // @TODO(blanktiger): Transform Allocator_Error into enum_flags an then
        // OR the flags here. Think of how to make it intuitive, cause then you
        // can't just switch on the error to check what happened.
        (void)free(allocation.memory, new_size, alignment, allocator);
        return result(nullptr, error);
    }

    return allocation;
}

force_inline auto get_features(Allocator allocator) -> Allocator_Features {
    auto features = Allocator_Features::NONE;
    auto query = call_allocator(allocator, Allocator_Mode::FEATURES, 0, 0, 0, &features);

    // Allocator_Mode::FEATURES must be implemented by all allocators.
    kstd_assert(query.error == Allocator_Error::NONE, "Allocator features query failed");

    return features;
}

force_inline auto is_this_yours(void* pointer, Allocator allocator) -> Allocator_Query_Result<bool> {
    auto query = call_allocator(allocator, Allocator_Mode::IS_THIS_YOURS, 0, 0, 0, pointer);

    if (query.error == Allocator_Error::MODE_NOT_IMPLEMENTED) {
        return query_result(false, Allocator_Query_Error::QUERY_NOT_IMPLEMENTED);
    }

    kstd_assert(query.error == Allocator_Error::NONE, "Allocator ownership query failed");
    return query_result(query.memory != nullptr);
}

force_inline auto get_info(void* pointer, Allocator allocator) -> Allocator_Query_Result<Allocator_Info> {
    Allocator_Info info{ .pointer = pointer };
    auto query = call_allocator(allocator, Allocator_Mode::INFO, 0, 0, 0, &info);

    if (query.error == Allocator_Error::MODE_NOT_IMPLEMENTED) {
        return query_result(info, Allocator_Query_Error::QUERY_NOT_IMPLEMENTED);
    }

    kstd_assert(query.error == Allocator_Error::NONE, "Allocator info query failed");
    return query_result(info);
}

template <typename T>
force_inline auto align_up(T value, T alignment) -> T {
    return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T>
force_inline auto align_down(T value, T alignment) -> T {
    return value & ~(alignment - 1);
}

// @TODO(blanktiger): Implement overflow pages. Implement DEBUG like on Arena_Allocator_State. Implement high water mark.
struct Temporary_Allocator_State {
    using enum Allocator_Features;
    static constexpr auto FEATURES = RESIZE_SHRINK_NO_OP | FAST_BUMP_ALLOCATOR | PER_FRAME_TEMPORARY_STORAGE;

    u8*       base{};
    u8*       current{};
    u8*       end{};
    Allocator backing_allocator{};

    Temporary_Allocator_State() = default;

    explicit Temporary_Allocator_State(usize size) : Temporary_Allocator_State(context.allocator, size) {}

    Temporary_Allocator_State(void* memory, usize size)
        : base(static_cast<u8*>(memory)),
          current(base),
          end(base + size) {
        kstd_assert(base != nullptr);
        kstd_assert(size > 0);
    }

    Temporary_Allocator_State(Allocator backing, usize size) : backing_allocator(backing) {
        kstd_assert(backing_allocator.valid());
        kstd_assert(size > 0);

        auto allocation = alloc(size, backing_allocator);
        kstd_assert(allocation.memory != nullptr);
        kstd_assert(allocation.error == Allocator_Error::NONE);

        base    = static_cast<u8*>(allocation.memory);
        current = base;
        end     = base + size;
    }

    // @NOTE(blanktiger): Couldn't figure out a way to avoid ownership issues with those two implemented.
    Temporary_Allocator_State(const Temporary_Allocator_State&) = delete;
    auto operator = (const Temporary_Allocator_State&) -> Temporary_Allocator_State& = delete;

    Temporary_Allocator_State(Temporary_Allocator_State&& from) noexcept
        : base(from.base),
          current(from.current),
          end(from.end),
          backing_allocator(from.backing_allocator) {
        from.base              = nullptr;
        from.current           = nullptr;
        from.end               = nullptr;
        from.backing_allocator = {};
    }

    auto operator = (Temporary_Allocator_State&&) -> Temporary_Allocator_State& = delete;

    ~Temporary_Allocator_State() {
        if (backing_allocator.valid() && base != nullptr) {
            auto error = mem::free(base, static_cast<usize>(end - base), backing_allocator);
            kstd_debug_assert(error == Allocator_Error::NONE);
        }
    }

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = this };
    }

    auto reset() -> void {
        current = base;
    }

    auto bytes_used() const -> usize {
        return static_cast<usize>(current - base);
    }

    auto bytes_left() const -> usize {
        return static_cast<usize>(end - current);
    }

    auto mark() const -> void* {
        return current;
    }

    auto rewind(const void* mark_point) -> void {
        kstd_assert(mark_point >= base && mark_point <= end);
        current = const_cast<u8*>(static_cast<const u8*>(mark_point));
    }

    static auto proc(Allocator_Mode mode, s64 size, s64 alignment, s64 old_size, void* old_memory, void* temporary_allocator_state) -> Allocator_Result {
        auto* state = static_cast<Temporary_Allocator_State*>(temporary_allocator_state);

        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (state->base == nullptr)
                    return result(nullptr, Allocator_Error::USE_OF_UNINITIALIZED_ALLOCATOR);

                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                auto* aligned = reinterpret_cast<u8*>(align_up(ptr_addr(state->current), static_cast<usize>(alignment)));
                auto* next = aligned + size;
                if (next < aligned || next > state->end)
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                state->current = next;
                return result(aligned);
            } break;

            case Allocator_Mode::RESIZE: {
                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0 || size <= old_size)
                    return result(size == 0 ? nullptr : old_memory);

                auto allocation = proc(Allocator_Mode::ALLOCATE, size, alignment, 0, nullptr, temporary_allocator_state);
                if (allocation.memory == nullptr)
                    return allocation;

                if (old_memory != nullptr && old_size > 0)
                    kstd_memcpy(allocation.memory, old_memory, static_cast<usize>(old_size));

                return allocation;
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = static_cast<Allocator_Features*>(old_memory);
                if (features != nullptr) *features = FEATURES;
                else                     return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                return result(nullptr);
            } break;

            case Allocator_Mode::IS_THIS_YOURS: {
                auto address = ptr_addr(old_memory);
                return result(address >= ptr_addr(state->base) && address < ptr_addr(state->end) ? old_memory : nullptr);
            } break;

            default: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
        }

        unreachable();
    }
};

inline Temporary_Allocator_State temporary_allocator_state{};

// @Important: Don't use the global temporary_allocator_state directly in the
// functions below. They are meant to work with the one set in the context.

force_inline auto talloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* {
    auto allocation = alloc(size, alignment, context.temporary_allocator);
    kstd_assert(allocation.memory != nullptr, "Temporary allocator exhausted.");
    return allocation.memory;
}

force_inline auto reset_temporary_allocator() -> void {
    context.temporary_state->reset();
}

force_inline auto temporary_allocator_mark() -> void* {
    return context.temporary_state->mark();
}

force_inline auto temporary_allocator_rewind(const void* mark) -> void {
    context.temporary_state->rewind(mark);
}

constexpr usize DEFAULT_ARENA_RESERVE_SIZE = 1 * 1024 * 1024;

template <bool DEBUG = false>
struct Arena_Allocator_State {
    using enum Allocator_Features;
    static constexpr auto FEATURES = RESIZE_SHRINK_NO_OP | FAST_BUMP_ALLOCATOR | IS_THIS_YOURS;

    Allocator backing_allocator{};
    usize     allocated{};

    u8* memory_base   = nullptr;
    u8* current_point = nullptr;
    u8* address_limit = nullptr;

    Arena_Allocator_State(usize reserve = DEFAULT_ARENA_RESERVE_SIZE, Allocator backing_allocator = {})
        : backing_allocator(mem::resolve_allocator(backing_allocator)),
          allocated(align_up(reserve, mem::PAGE_SIZE)),
          memory_base(nullptr),
          current_point(nullptr),
          address_limit(nullptr) {
        auto allocation = alloc(allocated, backing_allocator);
        kstd_assert(allocation.memory != nullptr);
        kstd_assert(allocation.error == Allocator_Error::NONE);

        memory_base   = static_cast<u8*>(allocation.memory);
        current_point = memory_base;
        address_limit = memory_base + allocated;
    }

    // Does not own buffer. Destructor will not free it.
    Arena_Allocator_State(void* memory, usize size)
        : backing_allocator({}),
          allocated(size),
          memory_base(static_cast<u8*>(memory)),
          current_point(memory_base),
          address_limit(memory_base + allocated) {
        kstd_assert(memory != nullptr);
        kstd_assert(size > 0);
    }

    // Does not own buffer. Destructor will not free it.
    template <usize N>
    Arena_Allocator_State(Static_Array<u8, N>& buffer) : Arena_Allocator_State(buffer.data, N) {}

    ~Arena_Allocator_State() {
        reset();
        if (backing_allocator.valid()) {
            auto error = mem::free(memory_base, allocated, backing_allocator);
            kstd_debug_assert(error == Allocator_Error::NONE);
        }
    }

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = this };
    }

    auto resize_and_dont_copy_old_memory(usize reserve) -> void {
        reserve = align_up(reserve, mem::PAGE_SIZE);
        auto allocation = alloc(reserve, backing_allocator);
        kstd_assert(allocation.memory != nullptr);

        auto error = mem::free(memory_base, allocated, backing_allocator);
        kstd_debug_assert(error == Allocator_Error::NONE);

        allocated     = reserve;
        memory_base   = static_cast<u8*>(allocation.memory);
        current_point = memory_base;
        address_limit = memory_base + allocated;
    }

    auto reset() -> void {
        if constexpr (DEBUG) {
            static constexpr auto STAMP = 0xCC;
            kstd_memset(memory_base, STAMP, current_point - memory_base);
        }
        current_point = memory_base;
    }

    auto bytes_left() const -> usize {
        return static_cast<usize>(address_limit - current_point);
    }

    auto bytes_used() const -> usize {
        return static_cast<usize>(current_point - memory_base);
    }

    static auto proc(Allocator_Mode mode, s64 size, s64 alignment, s64 old_size, void* old_memory, void* arena_state) -> Allocator_Result {
        auto* state = static_cast<Arena_Allocator_State*>(arena_state);

        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (state->memory_base == nullptr)
                    return result(nullptr, Allocator_Error::USE_OF_UNINITIALIZED_ALLOCATOR);

                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0)
                    return result(nullptr);

                auto* aligned = reinterpret_cast<u8*>(align_up(ptr_addr(state->current_point), static_cast<usize>(alignment)));
                auto* next = aligned + size;
                if (next < aligned || next > state->address_limit)
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                state->current_point = next;
                return result(aligned);
            } break;

            case Allocator_Mode::RESIZE: {
                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0 || size <= old_size)
                    return result(size == 0 ? nullptr : old_memory);

                auto allocation = proc(Allocator_Mode::ALLOCATE, size, alignment, 0, nullptr, arena_state);
                if (allocation.memory == nullptr)
                    return allocation;

                if (old_memory != nullptr && old_size > 0)
                    kstd_memcpy(allocation.memory, old_memory, static_cast<usize>(old_size));

                return allocation;
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = static_cast<Allocator_Features*>(old_memory);
                if (features != nullptr) *features = FEATURES;
                else                     return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                return result(nullptr);
            } break;

            case Allocator_Mode::IS_THIS_YOURS: {
                auto address = ptr_addr(old_memory);
                return result(address >= ptr_addr(state->memory_base) && address < ptr_addr(state->address_limit) ? old_memory : nullptr);
            } break;

            default: {
                return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
            } break;
        }

        unreachable();
    }
};

// Thin leak-checking wrapper. Forwards allocation requests to a backing allocator.
struct Debug_Allocator_State {
    using enum Allocator_Features;
    // @TODO(blanktiger): Consider leaving only DEBUG_ALLOCATOR here because it
    // then acts like a better wrapper that doesn't add to much that would lead
    // to issues after removing the wrapper if someone depended on some flag
    // that comes only from the wrapper.
    static constexpr Allocator_Features FEATURES = IS_THIS_YOURS | INFO | DEBUG_ALLOCATOR;

    struct Allocation_Record {
        void*              pointer;
        usize              size;
        usize              alignment;
        Allocation_Record* next;
    };

    Allocator                 backing{};
    synchronization::Spinlock guard;
    bool                      synchronized{};
    Allocation_Record*        live_head  = nullptr;
    usize                     live_count = 0;

    explicit Debug_Allocator_State(Allocator backing_allocator)
        : backing(backing_allocator),
          synchronized(backing_allocator.valid() && has_flag(get_features(backing_allocator), THREADSAFE)),
          live_head(nullptr),
          live_count(0) {
        kstd_assert(backing.valid());
    }

    ~Debug_Allocator_State() {
        kstd_assert(live_count == 0, "Debug_Allocator_State: leaked allocations");
        kstd_assert(live_head == nullptr, "Debug_Allocator_State: leaked allocations");
    }

    auto get_allocator() -> Allocator { return {.proc = proc, .data = this}; }

    static auto dispatch(Debug_Allocator_State* state, Allocator_Mode mode, s64 size, s64 alignment, s64 old_size, void* old_memory) -> Allocator_Result {
        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                auto allocation = call_allocator(state->backing, mode, size, alignment, old_size, old_memory);
                if (allocation.error != Allocator_Error::NONE || allocation.memory == nullptr)
                    return allocation;

                // @TODO(blanktiger): Merge this with the original allocation.
                auto record_allocation = alloc(sizeof(Allocation_Record), alignof(Allocation_Record), state->backing);
                if (record_allocation.memory == nullptr) {
                    // @TODO(blanktiger): Merge errors once they are enum_flags.
                    auto error = free(allocation.memory, size, static_cast<usize>(alignment), state->backing);
                    (void)error;
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);
                }

                new(record_allocation.memory) Allocation_Record {
                    .pointer = allocation.memory,
                    .size = static_cast<usize>(size),
                    .alignment = static_cast<usize>(alignment),
                    .next = state->live_head
                };

                state->live_head = static_cast<Allocation_Record*>(record_allocation.memory);
                state->live_count++;
                return allocation;
            } break;

            case Allocator_Mode::RESIZE: {
                auto allocation = call_allocator(state->backing, mode, size, alignment, old_size, old_memory);
                if (allocation.error != Allocator_Error::NONE)
                    return allocation;

                Allocation_Record* record = state->live_head;
                while (record != nullptr && record->pointer != old_memory)
                    record = record->next;

                if (old_memory == nullptr) {
                    if (allocation.memory == nullptr)
                        return allocation;

                    auto record_allocation = alloc(sizeof(Allocation_Record), alignof(Allocation_Record), state->backing);
                    if (record_allocation.memory == nullptr)
                        return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                    new(record_allocation.memory) Allocation_Record {
                        .pointer   = allocation.memory,
                        .size      = static_cast<usize>(size),
                        .alignment = static_cast<usize>(alignment),
                        .next      = state->live_head
                    };
                    state->live_head = static_cast<Allocation_Record*>(record_allocation.memory);
                    state->live_count++;
                    return allocation;
                }

                kstd_assert(record != nullptr, "Debug allocator resize of unknown pointer");
                if (size == 0) {
                    Allocation_Record** link = &state->live_head;
                    while (*link != record)
                        link = &(*link)->next;

                    *link = record->next;
                    state->live_count--;
                    (void)free(record, sizeof(Allocation_Record), alignof(Allocation_Record), state->backing);
                    return allocation;
                }

                record->pointer   = allocation.memory;
                record->size      = static_cast<usize>(size);
                record->alignment = static_cast<usize>(alignment);
                return allocation;
            } break;

            case Allocator_Mode::FREE: {
                if (old_memory == nullptr) return result(nullptr);

                Allocation_Record** link = &state->live_head;
                while (*link != nullptr) {
                    auto* record = *link;
                    if (record->pointer == old_memory) {
                        if (old_size != 0)
                            kstd_assert(record->size == static_cast<usize>(old_size), "Debug allocator free size mismatch");

                        *link = record->next;
                        state->live_count--;

                        auto free_result        = free(record->pointer, record->size, record->alignment, state->backing);
                        auto record_free_result = free(record, sizeof(Allocation_Record), alignof(Allocation_Record), state->backing);
                        (void)record_free_result;
                        return result(nullptr, free_result);
                    }

                    link = &record->next;
                }

                unreachable("Debug_Allocator_State: double free");
            } break;

            case Allocator_Mode::IS_THIS_YOURS: {
                for (auto* record = state->live_head; record != nullptr; record = record->next)
                    if (record->pointer == old_memory)
                        return result(old_memory);
                return result(nullptr);
            } break;

            case Allocator_Mode::INFO: {
                auto* info = static_cast<Allocator_Info*>(old_memory);
                if (info == nullptr)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                for (auto* record = state->live_head; record != nullptr; record = record->next) {
                    if (record->pointer == info->pointer) {
                        info->size      = static_cast<s64>(record->size);
                        info->alignment = static_cast<s64>(record->alignment);
                        return result(nullptr);
                    }
                }

                return result(nullptr, Allocator_Error::INVALID_POINTER);
            } break;

            default: unreachable();
        }

        unreachable();
    }

    static auto proc(Allocator_Mode mode, s64 size, s64 alignment, s64 old_size, void* old_memory, void* debug_state) -> Allocator_Result {
        auto* state = static_cast<Debug_Allocator_State*>(debug_state);

        switch (mode) {
            case Allocator_Mode::ALLOCATE:
            case Allocator_Mode::FREE:
            case Allocator_Mode::RESIZE:
            case Allocator_Mode::IS_THIS_YOURS:
            case Allocator_Mode::INFO: {
                if (state->synchronized) {
                    auto scoped_lock = state->guard.scoped_lock();
                    return dispatch(state, mode, size, alignment, old_size, old_memory);
                }

                kstd_assert(state->guard.try_lock(), "Debug_Allocator_State: concurrent access on non-threadsafe backing");
                auto allocation = dispatch(state, mode, size, alignment, old_size, old_memory);
                state->guard.unlock();
                return allocation;
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = static_cast<Allocator_Features*>(old_memory);
                if (features == nullptr)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                *features = *features | get_features(state->backing) | FEATURES;
                return result(nullptr);
            } break;

            default: return call_allocator(state->backing, mode, size, alignment, old_size, old_memory);
        }

        unreachable();
    }
};

struct Null_Allocator_State {
    static constexpr auto FEATURES = Allocator_Features::NONE;

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = nullptr };
    }

    static auto proc(Allocator_Mode mode, s64 size, s64, s64, void* old_memory, void*) -> Allocator_Result {
        switch (mode) {
            case Allocator_Mode::FEATURES: {
                auto* features = static_cast<Allocator_Features*>(old_memory);
                if (features != nullptr) *features = FEATURES;
                else                     return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                return result(nullptr);
            } break;

            case Allocator_Mode::FREE: {
                if (old_memory == nullptr) return result(nullptr);
            } break;

            case Allocator_Mode::ALLOCATE:
            case Allocator_Mode::RESIZE: {
                if (size == 0) return result(nullptr);
            } break;

            default: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
        }

        unreachable("Null allocator operation called.");
    }
};

inline Null_Allocator_State null_allocator_state{};
inline Allocator null_allocator = null_allocator_state.get_allocator();

force_inline auto set_allocator(Allocator allocator) -> void {
    context.allocator = allocator.valid() ? allocator : null_allocator;
}

force_inline auto set_temporary_allocator(Temporary_Allocator_State* new_storage) -> void {
    if (new_storage == nullptr) {
        context.temporary_allocator = {};
        context.temporary_state     = nullptr;
        return;
    }

    context.temporary_allocator = new_storage->get_allocator();
    context.temporary_state     = new_storage;
}

force_inline auto resolve_allocator(Allocator allocator) -> Allocator {
    if (allocator.valid()) return allocator;
    if (context.allocator.valid()) return context.allocator;
    return null_allocator;
}

struct Push_Allocator {
    Allocator previous_allocator;

    Push_Allocator(Allocator new_allocator) : previous_allocator(context.allocator) {
        set_allocator(new_allocator);
    }

    ~Push_Allocator() {
        set_allocator(previous_allocator);
    }
};

struct Push_Temporary_Allocator {
    Temporary_Allocator_State* previous_state;

    Push_Temporary_Allocator(Temporary_Allocator_State* new_state) : previous_state(context.temporary_state) {
        set_temporary_allocator(new_state);
    }

    ~Push_Temporary_Allocator() {
        set_temporary_allocator(previous_state);
    }
};

struct Auto_Rewind_Temporary {
    void* mark;

    Auto_Rewind_Temporary() : mark(temporary_allocator_mark()) {}
    ~Auto_Rewind_Temporary() { temporary_allocator_rewind(mark); }
};

}  // namespace mem

#define PUSH_ALLOCATOR(allocator) mem::Push_Allocator DEFER_UNIQ(_push_allocator_)(allocator)
#define PUSH_TEMPORARY_ALLOCATOR(allocator) mem::Push_Temporary_Allocator DEFER_UNIQ(_push_temporary_allocator_)(allocator)
#define AUTO_REWIND_TEMPORARY() mem::Auto_Rewind_Temporary DEFER_UNIQ(_auto_rewind_temporary_)

#if OS == GAMEOS
#include "gameos/allocator.hh"
#elif OS == LINUX
#include "linux/allocator.hh"
#else
#error "Unsupported OS"
#endif

#ifdef UNIT_TESTS_KSTD_ALLOCATOR

#include "array.hh"

TEST(Allocator, dispatches_to_state_data) {
    struct State {
        u32 calls = 0;
        s64 free_alignment = 0;
        static auto proc(mem::Allocator_Mode mode, s64, s64 alignment, s64, void*, void* data) -> mem::Allocator_Result {
            auto* state = static_cast<State*>(data);
            if (mode == mem::Allocator_Mode::ALLOCATE) state->calls++;
            if (mode == mem::Allocator_Mode::FREE) state->free_alignment = alignment;
            return {.memory = state, .error = mem::Allocator_Error::NONE};
        }
        auto get_allocator() -> mem::Allocator { return {.proc = proc, .data = this}; }
    } state;

    auto allocation = mem::alloc(16, alignof(u64), state.get_allocator());
    ASSERT_EQ(allocation.memory, &state);
    ASSERT_EQ(state.calls, 1);
    ASSERT_EQ(mem::free(&state, 16, 64, state.get_allocator()), mem::Allocator_Error::NONE);
    ASSERT_EQ(state.free_alignment, 64);
    ASSERT_EQ(mem::free(&state, 16, state.get_allocator()), mem::Allocator_Error::NONE);
    ASSERT_EQ(state.free_alignment, 0);
}

TEST(Allocator, features_are_queried_through_dispatch) {
    mem::Temporary_Allocator_State state{256};
    auto features = mem::get_features(state.get_allocator());
    ASSERT_TRUE(mem::has_feature(features, mem::Allocator_Features::FAST_BUMP_ALLOCATOR));
    ASSERT_TRUE(mem::has_feature(features, mem::Allocator_Features::PER_FRAME_TEMPORARY_STORAGE));
}

TEST(Allocator, realloc_moves_and_preserves_memory) {
    mem::Hosted_Allocator_State state{};
    auto allocator = state.get_allocator();
    auto allocation = mem::alloc(8, 16, allocator);
    ASSERT_NE(allocation.memory, nullptr);
    kstd_memset(allocation.memory, 0xAB, 8);

    auto resized = mem::realloc(allocation.memory, 8, 32, 16, allocator);
    ASSERT_NE(resized.memory, nullptr);
    ASSERT_EQ(resized.error, mem::Allocator_Error::NONE);
    auto* bytes = static_cast<u8*>(resized.memory);
    for (usize i = 0; i < 8; ++i) ASSERT_EQ(bytes[i], static_cast<u8>(0xAB));
    ASSERT_EQ(ptr_addr(resized.memory) % 16, 0);
    ASSERT_EQ(mem::free(resized.memory, 32, 16, allocator), mem::Allocator_Error::NONE);
}

TEST(Allocator, bump_allocators_realloc_shrink_in_place) {
    mem::Temporary_Allocator_State temporary{256};
    auto temporary_allocator = temporary.get_allocator();
    auto temporary_allocation = mem::alloc(32, 16, temporary_allocator);
    auto temporary_used = temporary.bytes_used();
    auto temporary_resized = mem::realloc(temporary_allocation.memory, 32, 8, 16, temporary_allocator);
    ASSERT_EQ(temporary_resized.memory, temporary_allocation.memory);
    ASSERT_EQ(temporary_resized.error, mem::Allocator_Error::NONE);
    ASSERT_EQ(temporary.bytes_used(), temporary_used);

    mem::Arena_Allocator_State arena{256};
    auto arena_allocator = arena.get_allocator();
    auto arena_allocation = mem::alloc(32, 16, arena_allocator);
    auto arena_used = arena.bytes_used();
    auto arena_resized = mem::realloc(arena_allocation.memory, 32, 8, 16, arena_allocator);
    ASSERT_EQ(arena_resized.memory, arena_allocation.memory);
    ASSERT_EQ(arena_resized.error, mem::Allocator_Error::NONE);
    ASSERT_EQ(arena.bytes_used(), arena_used);
}

TEST(Allocator, hosted_info_reports_allocation_metadata) {
    mem::Hosted_Allocator_State state{};
    auto allocator = state.get_allocator();
    auto allocation = mem::alloc(24, 64, allocator);
    ASSERT_NE(allocation.memory, nullptr);

    auto info = mem::get_info(allocation.memory, allocator);
    ASSERT_EQ(info.value.pointer, allocation.memory);
    ASSERT_EQ(info.value.size, 24);
    ASSERT_EQ(info.value.alignment, 64);
    ASSERT_EQ(mem::free(allocation.memory, 24, 64, allocator), mem::Allocator_Error::NONE);
}

TEST(Allocator, debug_ownership_query_tracks_live_allocations) {
    mem::Hosted_Allocator_State hosted{};
    mem::Debug_Allocator_State debug{hosted.get_allocator()};
    auto allocator = debug.get_allocator();
    auto allocation = mem::alloc(24, 16, allocator);
    ASSERT_TRUE(mem::is_this_yours(allocation.memory, allocator).value);
    ASSERT_EQ(mem::free(allocation.memory, 24, 16, allocator), mem::Allocator_Error::NONE);
    ASSERT_FALSE(mem::is_this_yours(allocation.memory, allocator).value);
}

TEST(Allocator, rejects_invalid_alignment) {
    mem::Hosted_Allocator_State state{};
    auto allocation = mem::alloc(16, 3, state.get_allocator());
    ASSERT_EQ(allocation.memory, nullptr);
    ASSERT_EQ(allocation.error, mem::Allocator_Error::INVALID_ARGUMENT);
}

TEST(Allocator, allocator_states_have_no_interface_base) {
    mem::Temporary_Allocator_State state{256};
    auto allocator = state.get_allocator();
    ASSERT_EQ(allocator.data, &state);
    ASSERT_NE(allocator.proc, nullptr);
}

TEST(Debug_Allocator_State, allows_destruction_when_all_freed) {
    mem::Hosted_Allocator_State hosted{};
    mem::Debug_Allocator_State debug{hosted.get_allocator()};

    auto a = mem::alloc(16, debug.get_allocator());
    auto b = mem::alloc(64, debug.get_allocator());
    ASSERT_NE(a.memory, nullptr);
    ASSERT_NE(b.memory, nullptr);

    ASSERT_EQ(mem::free(a.memory, 16, debug.get_allocator()), mem::Allocator_Error::NONE);
    ASSERT_EQ(mem::free(b.memory, 64, debug.get_allocator()), mem::Allocator_Error::NONE);
}

TEST(Debug_Allocator_State, detects_leaked_allocations) {
    EXPECT_DEATH(
        {
            mem::Hosted_Allocator_State hosted{};
            mem::Debug_Allocator_State debug{hosted.get_allocator()};
            (void)mem::alloc(32, debug.get_allocator());
        },
        "Debug_Allocator_State: leaked allocations"
    );
}

TEST(Debug_Allocator_State, detects_double_frees) {
    EXPECT_DEATH(
        {
            mem::Hosted_Allocator_State hosted{};
            mem::Debug_Allocator_State debug{hosted.get_allocator()};
            auto allocation = mem::alloc(32, debug.get_allocator());
            (void)mem::free(allocation.memory, 32, debug.get_allocator());
            (void)mem::free(allocation.memory, 32, debug.get_allocator());
        },
        "Debug_Allocator_State: double free"
    );
}

TEST(Null_Allocator_State, alloc_is_unreachable) {
    EXPECT_DEATH(
        {
            mem::Null_Allocator_State null{};
            (void)mem::alloc(16, null.get_allocator());
        },
        "Null allocator operation called."
    );
}

TEST(Null_Allocator_State, free_is_unreachable) {
    EXPECT_DEATH(
        {
            mem::Null_Allocator_State null{};
            (void)mem::free(reinterpret_cast<void*>(1), 0, null.get_allocator());
        },
        "Null allocator operation called."
    );
}

TEST(Arena_Allocator_State, alloc_returns_usable_memory) {
    mem::Arena_Allocator_State arena{4096};
    auto allocation = mem::alloc(64, arena.get_allocator());
    ASSERT_NE(allocation.memory, nullptr);
    kstd_memset(allocation.memory, 0xAB, 64);
}

TEST(Arena_Allocator_State, sequential_allocs_bump_forward) {
    mem::Arena_Allocator_State arena{4096};
    auto a = mem::alloc(16, arena.get_allocator());
    auto b = mem::alloc(16, arena.get_allocator());
    ASSERT_NE(a.memory, nullptr);
    ASSERT_NE(b.memory, nullptr);
    ASSERT_LT(a.memory, b.memory);
}

TEST(Arena_Allocator_State, reset_reclaims_memory) {
    mem::Arena_Allocator_State arena{4096};
    usize left_before = arena.bytes_left();
    ASSERT_NE(mem::alloc(256, arena.get_allocator()).memory, nullptr);
    ASSERT_LT(arena.bytes_left(), left_before);
    arena.reset();
    ASSERT_EQ(arena.bytes_left(), left_before);
}

TEST(Arena_Allocator_State, respects_alignment) {
    mem::Arena_Allocator_State arena{4096};
    ASSERT_NE(mem::alloc(1, 64, arena.get_allocator()).memory, nullptr);
    auto allocation = mem::alloc(8, 64, arena.get_allocator());
    ASSERT_NE(allocation.memory, nullptr);
    ASSERT_EQ(ptr_addr(allocation.memory) % 64, 0);
}

TEST(Arena_Allocator_State, returns_nullptr_when_exhausted) {
    mem::Arena_Allocator_State arena{4096};
    auto all = mem::alloc(arena.bytes_left(), arena.get_allocator());
    ASSERT_NE(all.memory, nullptr);
    ASSERT_EQ(arena.bytes_left(), 0);
    ASSERT_EQ(mem::alloc(1, arena.get_allocator()).error, mem::Allocator_Error::OUT_OF_MEMORY);
}

TEST(Arena_Allocator_State, debug_stamps_used_memory_on_reset) {
    mem::Arena_Allocator_State<true> arena{4096};
    auto allocation = mem::alloc(32, arena.get_allocator());
    ASSERT_NE(allocation.memory, nullptr);
    kstd_memset(allocation.memory, 0x11, 32);
    arena.reset();
    auto* bytes = static_cast<u8*>(allocation.memory);
    for (usize i = 0; i < 32; ++i) ASSERT_EQ(bytes[i], static_cast<u8>(0xCC));
}

#endif
