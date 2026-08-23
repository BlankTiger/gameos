#pragma once

#include <cstddef>
#include <new>

#include "kstd/allocator_types.hh"
#include "kstd/context.hh"
#include "kstd/basic.hh"
#include "kstd/assert.hh"
#include "kstd/cstring.hh"
#include "kstd/context.hh"
#include "kstd/math.hh"
#include "kstd/pointer_utils.hh"

template <typename T, usize N>
struct Static_Array;

namespace mem {

constexpr usize TEMPORARY_STORAGE_SIZE = 16 * 1024;


force_inline auto resolve_allocator(Allocator allocator) -> Allocator;

force_inline constexpr auto result(void* memory, Allocator_Error error = Allocator_Error::NONE) -> Allocator_Result {
    return { .memory = memory, .error = error };
}

template <typename T>
force_inline constexpr auto query_result(T&& value, Allocator_Query_Error error = Allocator_Query_Error::NONE) -> Allocator_Query_Error<T> {
    return { .value = value, .error = error };
}

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

    if (alignment == 0 || (!math::is_power_of_two(alignment) || alignment > static_cast<usize>(S64_MAX))
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    allocator = resolve_allocator(allocator);
    return call_allocator(allocator, Allocator_Mode::ALLOCATE, static_cast<s64>(size), static_cast<s64>(alignment), 0, nullptr);
}

force_inline auto free(void* pointer, usize size = 0, Allocator allocator = {}) -> Allocator_Error {
    if (size > static_cast<usize>(S64_MAX))
        return Allocator_Error::INVALID_ARGUMENT;

    allocator = resolve_allocator(allocator);
    auto result = call_allocator(allocator, Allocator_Mode::FREE, 0, 0, static_cast<s64>(size), pointer);
    return result.error;
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

    using enum Allocator_Ownership;
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
    static constexpr auto FEATURES = FAST_BUMP_ALLOCATOR | PER_FRAME_TEMPORARY_STORAGE;

    void*     base{};
    void*     current{};
    void*     end{};
    Allocator backing_allocator{};

    Temporary_Allocator_State() = default;

    explicit Temporary_Allocator_State(usize size) : Temporary_Allocator_State(context.allocator, size) {}

    Temporary_Allocator_State(void* memory, usize size)
        : base(memory),
          current(base),
          end(base + size) {
        kstd_assert(base != nullptr);
        kstd_assert(size > 0);
    }

    Temporary_Allocator_State(Allocator backing, usize size) : backing_allocator(backing) {
        kstd_assert(backing_allocator.valid());
        kstd_assert(size > 0);

        auto allocation = alloc(size, alignof(std::max_align_t), backing_allocator);
        kstd_assert(allocation.memory != nullptr);
        kstd_assert(allocation.error == Allocator_Error::NONE);

        base    = allocation.memory;
        current = base;
        end     = base + size;
    }

    // @NOTE(blanktiger): Couldn't figure out a way to avoid ownership issues with those two implemented.
    Temporary_Allocator_State(const Temporary_Allocator_State&) = delete;
    auto operator = (const Temporary_Allocator_State&) -> Temporary_Allocator_State& = delete;

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
        current = const_cast<void*>(mark_point);
    }

    static auto proc(Allocator_Mode mode, s64 size, s64 alignment, s64, void* old_memory, void* temporary_allocator_state) -> Allocator_Result {
        auto* state = static_cast<Temporary_Allocator_State*>(temporary_allocator_state);

        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (state->base == nullptr)
                    return result(nullptr, Allocator_Error::USE_OF_UNINITIALIZED_ALLOCATOR);

                if (size < 0 || alignment <= 0 || (alignment & (alignment - 1)) != 0)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                auto* aligned = reinterpret_cast<void*>(align_up(ptr_addr(state->current), static_cast<usize>(alignment)));
                auto* next = aligned + size;
                if (next < aligned || next > state->end)
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                state->current = next;
                return result(aligned);
            } break;

            case Allocator_Mode::FREE: {
                return result(nullptr);
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = static_cast<Allocator_Features*>(old_memory);
                if (features != nullptr) *features = FEATURES;
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

inline Temporary_Allocator_State temporary_allocator{};

force_inline auto talloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* {
    auto allocation = alloc(size, alignment, context.temporary_allocator);
    kstd_assert(allocation.memory != nullptr, "Temporary allocator exhausted.");
    return allocation.memory;
}

constexpr usize DEFAULT_ARENA_RESERVE_SIZE = 1 * 1024 * 1024;

template <bool DEBUG = false>
struct Arena_Allocator final : Allocator {
    Allocator* backing_allocator;
    usize      allocated;

    // @TODO: unique_ptr?
    u8* memory_base;
    u8* current_point;
    u8* address_limit;

    Arena_Allocator(usize reserve = DEFAULT_ARENA_RESERVE_SIZE, Allocator* backing_allocator = nullptr)
        : backing_allocator(mem::resolve_allocator(backing_allocator)),
          allocated(align_up(reserve, mem::PAGE_SIZE)),
          memory_base(static_cast<u8*>(this->backing_allocator->alloc(allocated))),
          current_point(memory_base),
          address_limit(memory_base + allocated) {
        kstd_assert(memory_base != nullptr);
    }

    // Does not own buffer. Destructor will not free it.
    Arena_Allocator(void* memory, usize size)
        : backing_allocator(nullptr),
          allocated(size),
          memory_base(static_cast<u8*>(memory)),
          current_point(memory_base),
          address_limit(memory_base + allocated) {
        kstd_assert(memory != nullptr);
        kstd_assert(size > 0);
    }

    // Does not own buffer. Destructor will not free it.
    template <usize N>
    Arena_Allocator(Static_Array<u8, N>& buffer)
        : Arena_Allocator(buffer.data, N) {}

    ~Arena_Allocator() {
        reset();
        if (backing_allocator != nullptr) {
            backing_allocator->free(memory_base, allocated);
        }
    }

    auto resize_and_dont_copy_old_memory(usize reserve) {
        allocated = align_up(reserve, mem::PAGE_SIZE);
        memory_base = static_cast<u8*>(backing_allocator->alloc(allocated));
        kstd_assert(memory_base != nullptr);

        current_point = memory_base;
        address_limit = memory_base + allocated;
    }

    auto reset() -> void {
        if constexpr (DEBUG) {
            const auto STAMP = 0xCC;
            kstd_memset(memory_base, STAMP, current_point - memory_base);
        }
        current_point = memory_base;
    }

    auto bytes_left() -> usize {
        return address_limit - current_point;
    }

    auto bytes_used() -> usize {
        return current_point - memory_base;
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;

        auto* aligned_point = reinterpret_cast<u8*>(
            align_up(ptr_addr(current_point), static_cast<usize>(alignment))
        );
        auto* new_point = aligned_point + size;

        if (new_point > address_limit) return nullptr;
        current_point = new_point;

        return static_cast<void*>(aligned_point);
    }

    auto free(void*, usize, usize = alignof(std::max_align_t)) -> void override {}
};

//
// Thin leak-checking wrapper. Forwards alloc/free to a backing allocator.
// Currently checks for:
// - leaked allocations,
// - double frees.
//
struct Debug_Allocator final : Allocator {
    struct Allocation_Record {
        void*              pointer;
        usize              size;
        usize              alignment;
        Allocation_Record* next;
    };

    Allocator*         backing    = nullptr;
    Allocation_Record* live_head  = nullptr;
    usize              live_count = 0;

    Debug_Allocator() : Debug_Allocator(mem::resolve_allocator()) {}

    explicit Debug_Allocator(Allocator* backing_allocator)
        : backing(backing_allocator),
          live_head(nullptr),
          live_count(0) {
        kstd_assert(backing_allocator != nullptr);
    }

    ~Debug_Allocator() {
        kstd_assert(live_count == 0, "Debug_Allocator: leaked allocations");
        kstd_assert(live_head == nullptr, "Debug_Allocator: leaked allocations");
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        void* pointer = backing->alloc(size, alignment);
        if (pointer == nullptr) return nullptr;

        auto* record = static_cast<Allocation_Record*>(
            backing->alloc(sizeof(Allocation_Record), alignof(Allocation_Record))
        );
        kstd_assert(record != nullptr, "Debug_Allocator: failed to allocate tracking record");

        record->pointer   = pointer;
        record->size      = size;
        record->alignment = alignment;
        record->next      = live_head;
        live_head         = record;
        live_count++;
        return pointer;
    }

    auto free(void* pointer, usize size, usize alignment = alignof(std::max_align_t)) -> void override {
        if (pointer == nullptr) return;

        Allocation_Record** link = &live_head;
        while (*link != nullptr) {
            Allocation_Record* record = *link;
            if (record->pointer == pointer) {
                if (size != 0) {
                    kstd_assert(record->size == size, "Debug_Allocator: free size mismatch");
                }
                kstd_assert(record->alignment == alignment, "Debug_Allocator: free alignment mismatch");

                *link = record->next;
                live_count--;

                backing->free(pointer, record->size, record->alignment);
                backing->free(record, sizeof(Allocation_Record), alignof(Allocation_Record));
                return;
            }
            link = &record->next;
        }

        unreachable("Debug_Allocator: double free");
    }
};

struct Null_Allocator final : Allocator {
    auto alloc(usize, usize = alignof(std::max_align_t)) -> void* override {
        unreachable("Null_Allocator alloc called.");
        return nullptr;
    }

    auto free(void*, usize, usize = alignof(std::max_align_t)) -> void override {
        unreachable("Null_Allocator free called.");
    }
};

inline Null_Allocator null_allocator{};




inline Null_Allocator_State null_allocator_state{};
inline Allocator null_allocator = null_allocator_state.get_allocator();

force_inline auto set_allocator(Allocator allocator) -> void {
    context.allocator = allocator.valid() ? allocator : null_allocator;
}

force_inline auto set_temporary_allocator(Temporary_Allocator_State* new_storage) -> void {
    context.temporary_allocator = new_storage->get_allocator();
    context.temporary_storage   = new_storage;
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

}  // namespace mem

#define PUSH_ALLOCATOR(allocator) mem::Push_Allocator DEFER_UNIQ(_push_allocator_)(allocator)
#define PUSH_TEMPORARY_ALLOCATOR(allocator) mem::Push_Temporary_Allocator DEFER_UNIQ(_push_temporary_allocator_)(allocator)

// @TODO(blanktiger): Macro for automatically getting the temp mark and deferring the unwind to it.

#if OS == GAMEOS
#include "gameos/allocator.hh"
#elif OS == LINUX
#include "linux/allocator.hh"
#else
#error "Unsupported OS"
#endif

#ifdef UNIT_TESTS_KSTD_ALLOCATOR

#include "array.hh"

TEST(Debug_Allocator, allows_destruction_when_all_freed) {
    mem::Hosted_Allocator hosted{};
    mem::Debug_Allocator  debug{&hosted};

    void* a = debug.alloc(16);
    defer(debug.free(a, 16));

    void* b = debug.alloc(64);
    defer(debug.free(b, 64));

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
}

// @TODO(blanktiger): Implement is_this_yours on allocators, currently this tests nothing.
TEST(Allocator, convenience_alloc_and_free_use_current_allocator) {
    mem::Hosted_Allocator hosted{};
    PUSH_ALLOCATOR(&hosted);

    void* pointer = mem::alloc(16, alignof(u64));
    defer(mem::free(pointer, 16, alignof(u64)));

    ASSERT_NE(pointer, nullptr);
}

TEST(Allocator, convenience_alloc_and_free_accept_explicit_allocator) {
    mem::Hosted_Allocator hosted{};

    void* pointer = mem::alloc(16, alignof(u64), &hosted);
    defer(mem::free(pointer, 16, alignof(u64), &hosted));

    ASSERT_NE(pointer, nullptr);
}

TEST(Debug_Allocator, detects_leaked_allocations) {
    EXPECT_DEATH(
        {
            mem::Hosted_Allocator hosted{};
            mem::Debug_Allocator  debug{&hosted};
            (void)debug.alloc(32);
        },
        "Debug_Allocator: leaked allocations"
    );
}

TEST(Debug_Allocator, detects_double_frees) {
    EXPECT_DEATH(
        {
            mem::Hosted_Allocator hosted{};
            mem::Debug_Allocator  debug{&hosted};
            auto size = 32;
            auto* mem = debug.alloc(size);
            debug.free(mem, size);
            debug.free(mem, size);
        },
        "Debug_Allocator: double free"
    );
}

TEST(Null_Allocator, alloc_is_unreachable) {
    EXPECT_DEATH(
        {
            mem::Null_Allocator null{};
            (void)null.alloc(16);
        },
        "Null_Allocator alloc called."
    );
}

TEST(Null_Allocator, free_is_unreachable) {
    EXPECT_DEATH(
        {
            mem::Null_Allocator null{};
            null.free(nullptr, 0);
        },
        "Null_Allocator free called."
    );
}

TEST(Arena_Allocator, alloc_returns_usable_memory) {
    mem::Arena_Allocator arena{4096};

    void* p = arena.alloc(64);
    ASSERT_NE(p, nullptr);
    kstd_memset(p, 0xAB, 64);
}

TEST(Arena_Allocator, sequential_allocs_bump_forward) {
    mem::Arena_Allocator arena{4096};

    auto* a = static_cast<u8*>(arena.alloc(16));
    auto* b = static_cast<u8*>(arena.alloc(16));
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_LT(a, b);
}

TEST(Arena_Allocator, free_is_noop) {
    mem::Arena_Allocator arena{4096};

    void* p          = arena.alloc(128);
    usize left_after = arena.bytes_left();
    arena.free(p, 128);
    ASSERT_EQ(arena.bytes_left(), left_after);
}

TEST(Arena_Allocator, reset_reclaims_memory) {
    mem::Arena_Allocator arena{4096};

    usize left_before = arena.bytes_left();
    ASSERT_NE(arena.alloc(256), nullptr);
    ASSERT_LT(arena.bytes_left(), left_before);

    arena.reset();
    ASSERT_EQ(arena.bytes_left(), left_before);
}

TEST(Arena_Allocator, respects_alignment) {
    mem::Arena_Allocator arena{4096};

    ASSERT_NE(arena.alloc(1), nullptr);
    void* p = arena.alloc(8, 64);
    ASSERT_NE(p, nullptr);
    ASSERT_EQ(ptr_addr(p) % 64, 0u);
}

TEST(Arena_Allocator, returns_nullptr_when_exhausted) {
    mem::Arena_Allocator arena{4096};

    void* all = arena.alloc(arena.bytes_left());
    ASSERT_NE(all, nullptr);
    ASSERT_EQ(arena.bytes_left(), 0u);
    ASSERT_EQ(arena.alloc(1), nullptr);
}

TEST(Arena_Allocator, debug_stamps_used_memory_on_reset) {
    mem::Arena_Allocator<true> arena{4096};

    auto* p = static_cast<u8*>(arena.alloc(32));
    ASSERT_NE(p, nullptr);
    kstd_memset(p, 0x11, 32);

    arena.reset();
    for (usize i = 0; i < 32; ++i) {
        ASSERT_EQ(p[i], static_cast<u8>(0xCC));
    }
}

TEST(Arena_Allocator, uses_static_buffer) {
    alignas(16) u8 buffer[256];
    mem::Arena_Allocator arena{buffer, sizeof(buffer)};

    ASSERT_EQ(arena.bytes_left(), 256u);
    void* p = arena.alloc(64);
    ASSERT_NE(p, nullptr);
    ASSERT_GE(static_cast<u8*>(p), buffer);
    ASSERT_LT(static_cast<u8*>(p), buffer + sizeof(buffer));
}

TEST(Arena_Allocator, static_buffer_returns_nullptr_when_exhausted) {
    alignas(16) u8 buffer[256];
    mem::Arena_Allocator arena{buffer, sizeof(buffer)};

    void* all = arena.alloc(arena.bytes_left());
    ASSERT_NE(all, nullptr);
    ASSERT_EQ(arena.bytes_left(), 0u);
    ASSERT_EQ(arena.alloc(1), nullptr);
}

TEST(Arena_Allocator, uses_static_array) {
    Static_Array<u8, 256> buffer{};
    mem::Arena_Allocator arena{buffer};

    ASSERT_EQ(arena.bytes_left(), 256u);
    void* p = arena.alloc(64);
    ASSERT_NE(p, nullptr);
    ASSERT_GE(static_cast<u8*>(p), buffer.data);
    ASSERT_LT(static_cast<u8*>(p), buffer.data + buffer.size);
}

TEST(Arena_Allocator, static_array_returns_nullptr_when_exhausted) {
    Static_Array<u8, 256> buffer{};
    mem::Arena_Allocator arena{buffer};

    void* all = arena.alloc(arena.bytes_left());
    ASSERT_NE(all, nullptr);
    ASSERT_EQ(arena.bytes_left(), 0u);
    ASSERT_EQ(arena.alloc(1), nullptr);
}

#endif
