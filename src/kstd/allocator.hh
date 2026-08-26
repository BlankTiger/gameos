#pragma once

#include <cstddef>
#include <new>
#include <type_traits>

#include "allocator_types.hh"
#include "kstd/allocator_types.hh"
#include "kstd/context.hh"
#include "kstd/basic.hh"
#include "kstd/assert.hh"
#include "kstd/cstring.hh"
#include "kstd/enum_flags.hh"
#include "kstd/math.hh"
#include "kstd/pointer_utils.hh"
#include "kstd/synchronization.hh"

template <typename T, ssize N>
struct Static_Array;

namespace mem {

constexpr auto TEMPORARY_STORAGE_SIZE = 16 * 1024;


force_inline auto resolve_allocator(Allocator allocator) -> Allocator;

force_inline constexpr auto result(void* memory, Allocator_Error error = Allocator_Error::NONE) -> Allocator_Result {
    return { .memory = memory, .error = error };
}

template <typename T>
force_inline constexpr auto query_result(T&& value, Allocator_Query_Error error = Allocator_Query_Error::NONE) -> Allocator_Query_Result<std::remove_cvref_t<T>> {
    return { .value = value, .error = error };
}

force_inline auto get_features(Allocator allocator) -> Allocator_Features;

force_inline auto call_allocator(Allocator allocator, Allocator_Mode mode, ssize size, ssize alignment, ssize old_size, void* old_memory) -> Allocator_Result {
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

force_inline auto alloc(ssize size, ssize alignment = MAX_ALIGN, Allocator allocator = {}) -> Allocator_Result {
    if (size < 0)
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    if (alignment <= 0 || !math::is_power_of_two(alignment))
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    allocator = resolve_allocator(allocator);
    return call_allocator(allocator, Allocator_Mode::ALLOCATE, size, alignment, 0, nullptr);
}

force_inline auto alloc(ssize size, Allocator allocator) -> Allocator_Result {
    return alloc(size, MAX_ALIGN, allocator);
}

force_inline auto free(void* pointer, ssize size, ssize alignment, Allocator allocator = {}) -> Allocator_Error {
    if (size < 0 || alignment < 0)
        return Allocator_Error::INVALID_ARGUMENT;

    allocator = resolve_allocator(allocator);
    auto result = call_allocator(allocator, Allocator_Mode::FREE, 0, alignment, size, pointer);
    return result.error;
}

force_inline auto free(void* pointer, ssize size = 0, Allocator allocator = {}) -> Allocator_Error {
    return free(pointer, size, 0, allocator);
}

//
// The assumption is that you want to (AND HAVE TO) use the same allocator you
// used for the original allocation.
//
force_inline auto realloc(void* pointer, ssize old_size, ssize new_size, ssize alignment, Allocator allocator = {}) -> Allocator_Result {
    if (old_size < 0 || new_size < 0 || alignment < 0)
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    if (pointer == nullptr && new_size == 0) return result(nullptr);

    allocator = resolve_allocator(allocator);
    auto direct = call_allocator(allocator, Allocator_Mode::RESIZE, new_size, alignment, old_size, pointer);
    if (direct.error != Allocator_Error::MODE_NOT_IMPLEMENTED)
        return direct;

    if (pointer != nullptr && old_size == 0)
        return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

    const auto features = get_features(allocator);
    const bool can_free = has_flag(features, Allocator_Features::FREE);

    if (new_size == 0) {
        if (!can_free)
            return direct;
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

    if (!can_free)
        return allocation;

    auto error = free(pointer, old_size, alignment, allocator);
    if (error != Allocator_Error::NONE) {
        // @TODO(blanktiger): Transform Allocator_Error into enum_flags an then
        // OR the flags here. Think of how to make it intuitive, cause then you
        // can't just switch on the error to check what happened.
        cast(void)free(allocation.memory, new_size, alignment, allocator);
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
force_inline constexpr auto align_up(T value, T alignment) -> T {
    return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T>
force_inline constexpr auto align_down(T value, T alignment) -> T {
    return value & ~(alignment - 1);
}

struct Temporary_Allocator_Overflow_Page {
    Temporary_Allocator_Overflow_Page* next{};
    ssize                              size{};
};

struct Temporary_Allocator_Mark {
    Temporary_Allocator_Overflow_Page* top_overflow_page{};
    ssize                              current_page_bytes_occupied{};
    ssize                              total_bytes_occupied{};
};

struct Temporary_Allocator_State {
    using enum Allocator_Features;
    static constexpr auto FEATURES = RESIZE_SHRINK_NO_OP | ACTUALLY_RESIZE | FAST_BUMP_ALLOCATOR | PER_FRAME_TEMPORARY_STORAGE;
    static constexpr ssize OVERFLOW_PAGE_HEADER_SIZE = align_up(size_of(Temporary_Allocator_Overflow_Page), MAX_ALIGN);

    u8*                                original_data{};
    ssize                              original_size{};
    u8*                                data{};
    ssize                              size{};
    ssize                              current_page_bytes_occupied{};
    ssize                              total_bytes_occupied{};
    ssize                              high_water_mark{};
    Temporary_Allocator_Overflow_Page* overflow_pages{};
    Allocator                          backing_allocator{};

    Temporary_Allocator_State() = default;

    explicit Temporary_Allocator_State(ssize size) : Temporary_Allocator_State(context.allocator, size) {}

    Temporary_Allocator_State(void* memory, ssize size)
        : original_data(cast(u8*)memory),
          original_size(size),
          data(original_data),
          size(size) {
        kstd_assert(original_data != nullptr);
        kstd_assert(size > 0);
    }

    Temporary_Allocator_State(Allocator backing, ssize size) : backing_allocator(backing) {
        kstd_assert(backing_allocator.valid());
        kstd_assert(size > 0);

        auto allocation = alloc(size, backing_allocator);
        kstd_assert(allocation.memory != nullptr);
        kstd_assert(allocation.error == Allocator_Error::NONE);

        original_data = cast(u8*)allocation.memory;
        original_size = size;
        data          = original_data;
        this->size    = size;
    }

    // @NOTE(blanktiger): Couldn't figure out a way to avoid ownership issues with those two implemented.
    Temporary_Allocator_State(const Temporary_Allocator_State&) = delete;
    auto operator = (const Temporary_Allocator_State&) -> Temporary_Allocator_State& = delete;

    Temporary_Allocator_State(Temporary_Allocator_State&& from) noexcept
        : original_data(from.original_data),
          original_size(from.original_size),
          data(from.data),
          size(from.size),
          current_page_bytes_occupied(from.current_page_bytes_occupied),
          total_bytes_occupied(from.total_bytes_occupied),
          high_water_mark(from.high_water_mark),
          overflow_pages(from.overflow_pages),
          backing_allocator(from.backing_allocator) {
        from.original_data               = nullptr;
        from.original_size               = 0;
        from.data                        = nullptr;
        from.size                        = 0;
        from.current_page_bytes_occupied = 0;
        from.total_bytes_occupied        = 0;
        from.high_water_mark             = 0;
        from.overflow_pages              = nullptr;
        from.backing_allocator           = {};
    }

    auto operator = (Temporary_Allocator_State&&) -> Temporary_Allocator_State& = delete;

    ~Temporary_Allocator_State() {
        free_overflow_pages();
        if (backing_allocator.valid() && original_data != nullptr) {
            auto error = mem::free(original_data, original_size, backing_allocator);
            kstd_debug_assert(error == Allocator_Error::NONE);
        }
    }

    auto get_allocator() -> Allocator {
        return { .proc = proc, .data = this };
    }

    auto reset() -> void {
        free_overflow_pages();
        data                         = original_data;
        size                         = original_size;
        current_page_bytes_occupied  = 0;
        total_bytes_occupied         = 0;
        high_water_mark              = 0;
    }

    auto bytes_used() const -> ssize {
        return total_bytes_occupied;
    }

    auto bytes_left() const -> ssize {
        return size - current_page_bytes_occupied;
    }

    auto mark() const -> Temporary_Allocator_Mark {
        return { overflow_pages, current_page_bytes_occupied, total_bytes_occupied };
    }

    auto rewind(Temporary_Allocator_Mark mark_point) -> void {
        free_overflow_pages(mark_point.top_overflow_page);
        current_page_bytes_occupied = mark_point.current_page_bytes_occupied;
        total_bytes_occupied        = mark_point.total_bytes_occupied;
    }

    force_inline auto add_new_overflow_page(ssize minimum_size, ssize alignment) -> Allocator_Result {
        auto default_page_size = original_size > OVERFLOW_PAGE_HEADER_SIZE
            ? original_size - OVERFLOW_PAGE_HEADER_SIZE
            : 0;
        auto page_size = default_page_size > minimum_size + alignment ? default_page_size : minimum_size + alignment;
        auto page_bytes = align_up(OVERFLOW_PAGE_HEADER_SIZE + page_size, MAX_ALIGN);
        auto page_allocation = alloc(page_bytes, MAX_ALIGN, backing_allocator);
        if (page_allocation.memory == nullptr)
            return page_allocation;

        auto* page = cast(Temporary_Allocator_Overflow_Page*)page_allocation.memory;
        page->next = overflow_pages;
        page->size = page_size;
        overflow_pages = page;
        data = cast(u8*)page + OVERFLOW_PAGE_HEADER_SIZE;
        size = page_size;
        current_page_bytes_occupied = 0;
        return result(page);
    }

    force_inline auto update_high_water_mark() -> void {
        if (total_bytes_occupied > high_water_mark)
            high_water_mark = total_bytes_occupied;
    }

    force_inline auto allocate_from_current_page(ssize requested_size, ssize alignment) -> Allocator_Result {
        auto* aligned = cast(u8*)(align_up(ptr_addr(data + current_page_bytes_occupied), cast(psize)alignment));
        auto* next    = aligned + requested_size;
        auto allocation_did_not_overflow = next >= aligned;
        auto allocation_fits_in_page     = next <= data + size;
        if (!allocation_did_not_overflow || !allocation_fits_in_page)
            return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

        total_bytes_occupied += cast(ssize)(next - (data + current_page_bytes_occupied));
        current_page_bytes_occupied = cast(ssize)(next - data);
        update_high_water_mark();
        return result(aligned);
    }

    static auto proc(Allocator_Mode mode, ssize size, ssize alignment, ssize old_size, void* old_memory, void* temporary_allocator_state) -> Allocator_Result {
        auto* state = cast(Temporary_Allocator_State*)temporary_allocator_state;

        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (state->data == nullptr)
                    return result(nullptr, Allocator_Error::USE_OF_UNINITIALIZED_ALLOCATOR);

                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0) return result(nullptr);

                auto allocation = state->allocate_from_current_page(size, alignment);
                if (allocation.memory != nullptr)
                    return allocation;

                if (!state->backing_allocator.valid())
                    return allocation;

                auto page_allocation = state->add_new_overflow_page(size, alignment);
                if (page_allocation.memory == nullptr)
                    return page_allocation;

                return state->allocate_from_current_page(size, alignment);
            } break;

            case Allocator_Mode::RESIZE: {
                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0 || size <= old_size)
                    return result(size == 0 ? nullptr : old_memory);

                const auto old_address     = ptr_addr(old_memory);
                const auto current_address = ptr_addr(state->data) + state->current_page_bytes_occupied;
                if (old_memory != nullptr && old_address <= current_address && old_address + old_size == current_address) {
                    const auto next_address  = old_address + size;
                    const auto limit_address = ptr_addr(state->data) + state->size;
                    if (next_address >= old_address && next_address <= limit_address) {
                        state->total_bytes_occupied += size - old_size;
                        state->current_page_bytes_occupied = cast(ssize)(next_address - ptr_addr(state->data));
                        state->update_high_water_mark();
                        return result(old_memory);
                    }
                }

                if (old_memory == nullptr)
                    return proc(Allocator_Mode::ALLOCATE, size, alignment, 0, nullptr, temporary_allocator_state);

                return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = cast(Allocator_Features*)old_memory;
                if (features != nullptr) *features = FEATURES;
                else                     return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                return result(nullptr);
            } break;

            case Allocator_Mode::IS_THIS_YOURS: {
                auto address = ptr_addr(old_memory);
                if (address >= ptr_addr(state->original_data) && address < ptr_addr(state->original_data + state->original_size))
                    return result(old_memory);
                for (auto* page = state->overflow_pages; page != nullptr; page = page->next) {
                    auto* page_data = cast(u8*)page + OVERFLOW_PAGE_HEADER_SIZE;
                    if (address >= ptr_addr(page_data) && address < ptr_addr(page_data + page->size))
                        return result(old_memory);
                }
                return result(nullptr);
            } break;

            default: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
        }

        unreachable();
    }

    // nullptr means free every overflow page.
    auto free_overflow_pages(Temporary_Allocator_Overflow_Page* target = nullptr) -> void {
        while (overflow_pages != target) {
            kstd_assert(overflow_pages != nullptr, "Temporary allocator mark belongs to another allocator.");
            auto* page = overflow_pages;
            overflow_pages = page->next;
            auto error = mem::free(page, OVERFLOW_PAGE_HEADER_SIZE + page->size, backing_allocator);
            kstd_debug_assert(error == Allocator_Error::NONE);
        }

        if (overflow_pages == nullptr) {
            data = original_data;
            size = original_size;
        } else {
            data = cast(u8*)overflow_pages + OVERFLOW_PAGE_HEADER_SIZE;
            size = overflow_pages->size;
        }
    }
};

inline Temporary_Allocator_State temporary_allocator_state{};

// @Important: Don't use the global temporary_allocator_state directly in the
// functions below. They are meant to work with the one set in the context.

    force_inline auto talloc(ssize size, ssize alignment = MAX_ALIGN) -> void* {
    auto allocation = alloc(size, alignment, context.temporary_allocator);
    kstd_assert(allocation.memory != nullptr, "Temporary allocator exhausted.");
    return allocation.memory;
}

force_inline auto reset_temporary_allocator() -> void {
    context.temporary_state->reset();
}

force_inline auto temporary_allocator_mark() -> Temporary_Allocator_Mark {
    return context.temporary_state->mark();
}

force_inline auto temporary_allocator_rewind(Temporary_Allocator_Mark mark) -> void {
    context.temporary_state->rewind(mark);
}

constexpr ssize DEFAULT_ARENA_RESERVE_SIZE = 1 * 1024 * 1024;

template <bool DEBUG = false>
struct Arena_Allocator_State {
    using enum Allocator_Features;
    static constexpr auto FEATURES = RESIZE_SHRINK_NO_OP | ACTUALLY_RESIZE | FAST_BUMP_ALLOCATOR | IS_THIS_YOURS;

    Allocator backing_allocator{};
    ssize     allocated{};

    u8* memory_base   = nullptr;
    u8* current_point = nullptr;
    u8* address_limit = nullptr;

    Arena_Allocator_State(ssize reserve = DEFAULT_ARENA_RESERVE_SIZE, Allocator backing_allocator = {})
        : backing_allocator(mem::resolve_allocator(backing_allocator)),
          allocated(align_up(reserve, mem::PAGE_SSIZE)),
          memory_base(nullptr),
          current_point(nullptr),
          address_limit(nullptr) {
        auto allocation = alloc(allocated, backing_allocator);
        kstd_assert(allocation.memory != nullptr);
        kstd_assert(allocation.error == Allocator_Error::NONE);

        memory_base   = cast(u8*)allocation.memory;
        current_point = memory_base;
        address_limit = memory_base + allocated;
    }

    // Does not own buffer. Destructor will not free it.
    Arena_Allocator_State(void* memory, ssize size)
        : backing_allocator({}),
          allocated(size),
          memory_base(cast(u8*)memory),
          current_point(memory_base),
          address_limit(memory_base + allocated) {
        kstd_assert(memory != nullptr);
        kstd_assert(size > 0);
    }

    // Does not own buffer. Destructor will not free it.
    template <ssize N>
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

    auto resize_and_dont_copy_old_memory(ssize reserve) -> void {
        reserve = align_up(reserve, mem::PAGE_SSIZE);
        auto allocation = alloc(reserve, backing_allocator);
        kstd_assert(allocation.memory != nullptr);

        auto error = mem::free(memory_base, allocated, backing_allocator);
        kstd_debug_assert(error == Allocator_Error::NONE);

        allocated     = reserve;
        memory_base   = cast(u8*)allocation.memory;
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

    auto bytes_left() const -> ssize {
        return cast(ssize)(address_limit - current_point);
    }

    auto bytes_used() const -> ssize {
        return cast(ssize)(current_point - memory_base);
    }

    static auto proc(Allocator_Mode mode, ssize size, ssize alignment, ssize old_size, void* old_memory, void* arena_state) -> Allocator_Result {
        auto* state = cast(Arena_Allocator_State*)arena_state;

        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                if (state->memory_base == nullptr)
                    return result(nullptr, Allocator_Error::USE_OF_UNINITIALIZED_ALLOCATOR);

                if (size < 0 || alignment <= 0 || !math::is_power_of_two(alignment))
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                if (size == 0)
                    return result(nullptr);

                auto* aligned = cast(u8*)(align_up(ptr_addr(state->current_point), cast(psize)alignment));
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

                const auto old_address     = ptr_addr(old_memory);
                const auto current_address = ptr_addr(state->current_point);
                if (old_memory != nullptr && old_address <= current_address && old_address + old_size == current_address) {
                    const auto next_address  = old_address + size;
                    const auto limit_address = ptr_addr(state->address_limit);
                    if (next_address >= old_address && next_address <= limit_address) {
                        state->current_point = cast(u8*)next_address;
                        return result(old_memory);
                    }
                }

                if (old_memory == nullptr)
                    return proc(Allocator_Mode::ALLOCATE, size, alignment, 0, nullptr, arena_state);

                return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
            } break;

            case Allocator_Mode::FEATURES: {
                auto* features = cast(Allocator_Features*)old_memory;
                if (features != nullptr) *features = FEATURES;
                else                     return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                return result(nullptr);
            } break;

            case Allocator_Mode::IS_THIS_YOURS: {
                auto address = ptr_addr(old_memory);
                return result(address >= ptr_addr(state->memory_base) && address < ptr_addr(state->address_limit) ? old_memory : nullptr);
            } break;

            default: return result(nullptr, Allocator_Error::MODE_NOT_IMPLEMENTED);
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
        ssize              size;
        ssize              alignment;
        Allocation_Record* next;
    };

    Allocator                 backing{};
    synchronization::Spinlock guard;
    bool                      synchronized{};
    Allocation_Record*        live_head  = nullptr;
    ssize                     live_count = 0;

    explicit Debug_Allocator_State(Allocator backing_allocator)
        : backing(backing_allocator),
          synchronized(backing_allocator.valid() && has_flag(get_features(backing_allocator), THREADSAFE)),
          live_head(nullptr),
          live_count(0) {
        kstd_assert(backing.valid());
    }

    ~Debug_Allocator_State() {
        kstd_assert(live_count == 0,       "Debug_Allocator_State: Leaked allocations.");
        kstd_assert(live_head  == nullptr, "Debug_Allocator_State: Leaked allocations.");
    }

    auto get_allocator() -> Allocator { return {.proc = proc, .data = this}; }

    static auto dispatch(Debug_Allocator_State* state, Allocator_Mode mode, ssize size, ssize alignment, ssize old_size, void* old_memory) -> Allocator_Result {
        switch (mode) {
            case Allocator_Mode::ALLOCATE: {
                auto allocation = call_allocator(state->backing, mode, size, alignment, old_size, old_memory);
                if (allocation.error != Allocator_Error::NONE || allocation.memory == nullptr)
                    return allocation;

                // @TODO(blanktiger): Merge this with the original allocation.
                auto record_allocation = alloc(size_of(Allocation_Record), align_of(Allocation_Record), state->backing);
                if (record_allocation.memory == nullptr) {
                    // @TODO(blanktiger): Merge errors once they are enum_flags.
                    auto error = free(allocation.memory, size, alignment, state->backing);
                    cast(void)error;
                    return result(nullptr, Allocator_Error::OUT_OF_MEMORY);
                }

                new(record_allocation.memory) Allocation_Record {
                    .pointer = allocation.memory,
                    .size = size,
                    .alignment = alignment,
                    .next = state->live_head
                };

                state->live_head = cast(Allocation_Record*)record_allocation.memory;
                state->live_count++;
                return allocation;
            } break;

            case Allocator_Mode::RESIZE: {
                auto allocation = mem::realloc(old_memory, old_size, size, alignment, state->backing);
                if (allocation.error != Allocator_Error::NONE)
                    return allocation;

                Allocation_Record* record = state->live_head;
                while (record != nullptr && record->pointer != old_memory)
                    record = record->next;

                if (old_memory == nullptr) {
                    if (allocation.memory == nullptr)
                        return allocation;

                    auto record_allocation = alloc(size_of(Allocation_Record), align_of(Allocation_Record), state->backing);
                    if (record_allocation.memory == nullptr)
                        return result(nullptr, Allocator_Error::OUT_OF_MEMORY);

                    new(record_allocation.memory) Allocation_Record {
                        .pointer   = allocation.memory,
                        .size      = size,
                        .alignment = alignment,
                        .next      = state->live_head
                    };
                    state->live_head = cast(Allocation_Record*)record_allocation.memory;
                    state->live_count++;
                    return allocation;
                }

                kstd_assert(record != nullptr, "Debug_Allocator_State: Resize of unknown pointer.");
                if (size == 0) {
                    Allocation_Record** link = &state->live_head;
                    while (*link != record)
                        link = &(*link)->next;

                    *link = record->next;
                    state->live_count--;
                    cast(void)free(record, size_of(Allocation_Record), align_of(Allocation_Record), state->backing);
                    return allocation;
                }

                record->pointer   = allocation.memory;
                record->size      = size;
                record->alignment = alignment;
                return allocation;
            } break;

            case Allocator_Mode::FREE: {
                if (old_memory == nullptr) return result(nullptr);

                Allocation_Record** link = &state->live_head;
                while (*link != nullptr) {
                    auto* record = *link;
                    if (record->pointer == old_memory) {
                        if (old_size != 0)
                            kstd_assert(record->size == old_size, "Debug allocator free size mismatch");

                        *link = record->next;
                        state->live_count--;

                        auto free_result        = free(record->pointer, record->size, record->alignment, state->backing);
                        auto record_free_result = free(record, size_of(Allocation_Record), align_of(Allocation_Record), state->backing);
                        cast(void)record_free_result;
                        return result(nullptr, free_result);
                    }

                    link = &record->next;
                }

                unreachable("Debug_Allocator_State: Double free.");
            } break;

            case Allocator_Mode::IS_THIS_YOURS: {
                for (auto* record = state->live_head; record != nullptr; record = record->next)
                    if (record->pointer == old_memory)
                        return result(old_memory);
                return result(nullptr);
            } break;

            case Allocator_Mode::INFO: {
                auto* info = cast(Allocator_Info*)old_memory;
                if (info == nullptr)
                    return result(nullptr, Allocator_Error::INVALID_ARGUMENT);

                for (auto* record = state->live_head; record != nullptr; record = record->next) {
                    if (record->pointer == info->pointer) {
                        info->requested_size      = cast(ssize)record->size;
                        info->requested_alignment = cast(ssize)record->alignment;
                        return result(nullptr);
                    }
                }

                return result(nullptr, Allocator_Error::INVALID_POINTER);
            } break;

            default: unreachable();
        }

        unreachable();
    }

    static auto proc(Allocator_Mode mode, ssize size, ssize alignment, ssize old_size, void* old_memory, void* debug_state) -> Allocator_Result {
        auto* state = cast(Debug_Allocator_State*)debug_state;

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
                auto* features = cast(Allocator_Features*)old_memory;
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

    static auto proc(Allocator_Mode mode, ssize size, ssize, ssize, void* old_memory, void*) -> Allocator_Result {
        switch (mode) {
            case Allocator_Mode::FEATURES: {
                auto* features = cast(Allocator_Features*)old_memory;
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

        unreachable("Null_Allocator: Operation called.");
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
    Temporary_Allocator_Mark mark;

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
        u32   calls          = 0;
        ssize free_alignment = 0;

        static auto proc(mem::Allocator_Mode mode, ssize, ssize alignment, ssize, void*, void* data) -> mem::Allocator_Result {
            auto* state = cast(State*)data;
            if (mode == mem::Allocator_Mode::ALLOCATE) state->calls++;
            if (mode == mem::Allocator_Mode::FREE)     state->free_alignment = alignment;
            return {
                .memory = state,
                .error = mem::Allocator_Error::NONE
            };
        }

        auto get_allocator() -> mem::Allocator {
            return { .proc = proc, .data = this };
        }
    } state;

    PUSH_ALLOCATOR(state.get_allocator());
    auto allocation = mem::alloc(16, align_of(u64));

    ASSERT_EQ(allocation.memory, &state);
    ASSERT_EQ(state.calls, 1);
    ASSERT_EQ(mem::free(&state, 16, 64), mem::Allocator_Error::NONE);
    ASSERT_EQ(state.free_alignment, 64);
    ASSERT_EQ(mem::free(&state, 16), mem::Allocator_Error::NONE);
    ASSERT_EQ(state.free_alignment, 0);
}

TEST(Allocator, features_are_queried_through_dispatch) {
    mem::Temporary_Allocator_State state{256};
    auto features = mem::get_features(state.get_allocator());
    ASSERT_TRUE(has_flag(features, mem::Allocator_Features::FAST_BUMP_ALLOCATOR));
    ASSERT_TRUE(has_flag(features, mem::Allocator_Features::PER_FRAME_TEMPORARY_STORAGE));
}

TEST(Allocator, unimplemented_query_result) {
    struct State {
        static auto proc(mem::Allocator_Mode, ssize, ssize, ssize, void*, void*) -> mem::Allocator_Result {
            return mem::result(nullptr, mem::Allocator_Error::MODE_NOT_IMPLEMENTED);
        }

        auto get_allocator() -> mem::Allocator {
            return { .proc = proc, .data = this };
        }
    } state;

    auto allocator = state.get_allocator();

    auto info = mem::get_info(nullptr, allocator);
    ASSERT_EQ(info.error, mem::Allocator_Query_Error::QUERY_NOT_IMPLEMENTED);
}

TEST(Allocator, realloc_moves_and_preserves_memory) {
    mem::Hosted_Allocator_State state{};
    PUSH_ALLOCATOR(state.get_allocator());

    auto allocation = mem::alloc(8, 16);
    ASSERT_NE(allocation.memory, nullptr);

    kstd_memset(allocation.memory, 0xAB, 8);
    auto resized = mem::realloc(allocation.memory, 8, 32, 16);

    ASSERT_NE(resized.memory, nullptr);
    ASSERT_EQ(resized.error, mem::Allocator_Error::NONE);
    auto* bytes = cast(u8*)resized.memory;
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ(bytes[i], cast(u8)(0xAB));
    ASSERT_EQ(ptr_addr(resized.memory) % 16, 0);
    ASSERT_EQ(mem::free(resized.memory, 32, 16), mem::Allocator_Error::NONE);
}

TEST(Allocator, bump_allocators_realloc_tail_growth_in_place) {
    // Can't have freestanding methods defined in gtests.. MEH.
    struct A {
        static auto verify(mem::Allocator allocator) -> void {
            auto allocation = mem::alloc(8, 16, allocator);
            auto resized    = mem::realloc(allocation.memory, 8, 32, 16, allocator);

            ASSERT_EQ(resized.memory, allocation.memory);
            ASSERT_EQ(resized.error, mem::Allocator_Error::NONE);
        }
    };

    mem::Temporary_Allocator_State temporary{256};
    A::verify(temporary.get_allocator());
    ASSERT_EQ(temporary.bytes_used(), 32);

    mem::Arena_Allocator_State arena{256};
    A::verify(arena.get_allocator());
    ASSERT_EQ(arena.bytes_used(), 32);
}

TEST(Allocator, bump_allocators_move_non_tail_allocations) {
    mem::Arena_Allocator_State arena{256};
    auto first  = mem::alloc(8, 16, arena.get_allocator());
    auto second = mem::alloc(8, 16, arena.get_allocator());
    kstd_memset(first.memory, 0xAB, 8);

    auto resized = mem::realloc(first.memory, 8, 32, 16, arena.get_allocator());
    ASSERT_EQ(resized.error, mem::Allocator_Error::NONE);
    ASSERT_NE(resized.memory, first.memory);
    auto* bytes = cast(u8*)resized.memory;
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ(bytes[i], cast(u8)(0xAB));
    cast(void)second;
}

TEST(Allocator, bump_allocators_realloc_shrink_in_place) {
    {
        mem::Temporary_Allocator_State temporary{256};
        PUSH_ALLOCATOR(temporary.get_allocator());

        auto temporary_allocation = mem::alloc(32, 16);
        auto temporary_used = temporary.bytes_used();
        auto temporary_resized = mem::realloc(temporary_allocation.memory, 32, 8, 16);
        ASSERT_EQ(temporary_resized.memory, temporary_allocation.memory);
        ASSERT_EQ(temporary_resized.error, mem::Allocator_Error::NONE);
        ASSERT_EQ(temporary.bytes_used(), temporary_used);
    }

    {
        mem::Arena_Allocator_State arena{256};
        PUSH_ALLOCATOR(arena.get_allocator());

        auto arena_features = mem::get_features(arena.get_allocator());
        ASSERT_TRUE(has_flag(arena_features, mem::Allocator_Features::RESIZE_SHRINK_NO_OP));
        ASSERT_FALSE(has_flag(arena_features, mem::Allocator_Features::FREE));

        auto arena_allocation = mem::alloc(32, 16);
        auto arena_used = arena.bytes_used();
        auto arena_resized = mem::realloc(arena_allocation.memory, 32, 8, 16);
        ASSERT_EQ(arena_resized.memory, arena_allocation.memory);
        ASSERT_EQ(arena_resized.error, mem::Allocator_Error::NONE);
        ASSERT_EQ(arena.bytes_used(), arena_used);
    }
}

TEST(Allocator, hosted_info_reports_allocation_metadata) {
    mem::Hosted_Allocator_State state{};
    PUSH_ALLOCATOR(state.get_allocator());

    auto allocation = mem::alloc(24, 64);
    ASSERT_NE(allocation.memory, nullptr);

    auto info = mem::get_info(allocation.memory, state.get_allocator());
    ASSERT_EQ(info.value.pointer, allocation.memory);
    ASSERT_EQ(info.value.requested_size,      24);
    ASSERT_EQ(info.value.requested_alignment, 64);
    ASSERT_EQ(mem::free(allocation.memory, 24, 64), mem::Allocator_Error::NONE);
}

TEST(Allocator, debug_ownership_query_tracks_live_allocations) {
    mem::Hosted_Allocator_State hosted{};
    mem::Debug_Allocator_State debug{hosted.get_allocator()};
    auto allocator = debug.get_allocator();

    auto allocation = mem::alloc(24, 16, allocator);
    ASSERT_TRUE(mem::is_this_yours(allocation.memory, allocator).value);

    auto error = mem::free(allocation.memory, 24, 16, allocator);
    ASSERT_EQ(error, mem::Allocator_Error::NONE);
    ASSERT_FALSE(mem::is_this_yours(allocation.memory, allocator).value);
}

TEST(Allocator, rejects_invalid_alignment) {
    mem::Hosted_Allocator_State state{};
    PUSH_ALLOCATOR(state.get_allocator());

    auto allocation = mem::alloc(16, 3);

    ASSERT_EQ(allocation.memory, nullptr);
    ASSERT_EQ(allocation.error, mem::Allocator_Error::INVALID_ARGUMENT);
}

TEST(Allocator, temporary_allocator_get_allocator_interface) {
    mem::Temporary_Allocator_State state{256};
    auto allocator = state.get_allocator();
    ASSERT_EQ(allocator.data, &state);
    ASSERT_NE(allocator.proc, nullptr);
}

TEST(Temporary_Allocator_State, grows_with_embedded_overflow_pages) {
    mem::Hosted_Allocator_State hosted{};
    mem::Temporary_Allocator_State state{hosted.get_allocator(), 32};

    auto first = mem::alloc(24, 8, state.get_allocator());
    auto second = mem::alloc(24, 8, state.get_allocator());

    ASSERT_NE(first.memory, nullptr);
    ASSERT_NE(second.memory, nullptr);
    ASSERT_NE(first.memory, second.memory);
    ASSERT_NE(state.overflow_pages, nullptr);
    ASSERT_EQ(state.bytes_used(), 48);
    ASSERT_EQ(state.high_water_mark, 48);
}

TEST(Temporary_Allocator_State, rewind_releases_newer_overflow_pages) {
    mem::Hosted_Allocator_State hosted{};
    mem::Temporary_Allocator_State state{hosted.get_allocator(), 32};

    auto mark = state.mark();
    auto first = mem::alloc(40, 8, state.get_allocator());
    auto second_mark = state.mark();
    auto second = mem::alloc(40, 8, state.get_allocator());

    state.rewind(second_mark);
    ASSERT_NE(first.memory, nullptr);
    ASSERT_NE(second.memory, nullptr);
    ASSERT_NE(state.overflow_pages, nullptr);
    ASSERT_EQ(state.bytes_used(), second_mark.total_bytes_occupied);

    state.rewind(mark);
    ASSERT_EQ(state.overflow_pages, nullptr);
    ASSERT_EQ(state.bytes_used(), 0);
}

TEST(Temporary_Allocator_State, fixed_buffer_does_not_grow) {
    Static_Array<u8, 32> buffer{};
    mem::Temporary_Allocator_State state{buffer.data, 32};

    auto allocation = mem::alloc(33, 8, state.get_allocator());

    ASSERT_EQ(allocation.memory, nullptr);
    ASSERT_EQ(allocation.error, mem::Allocator_Error::OUT_OF_MEMORY);
    ASSERT_EQ(state.overflow_pages, nullptr);
}

TEST(Debug_Allocator_State, allows_destruction_when_all_freed) {
    mem::Hosted_Allocator_State hosted{};
    mem::Debug_Allocator_State debug{hosted.get_allocator()};
    PUSH_ALLOCATOR(debug.get_allocator());

    auto a = mem::alloc(16);
    auto b = mem::alloc(64);
    ASSERT_NE(a.memory, nullptr);
    ASSERT_NE(b.memory, nullptr);

    ASSERT_EQ(mem::free(a.memory, 16), mem::Allocator_Error::NONE);
    ASSERT_EQ(mem::free(b.memory, 64), mem::Allocator_Error::NONE);
}

TEST(Debug_Allocator_State, detects_leaked_allocations) {
    EXPECT_DEATH(
        {
            mem::Hosted_Allocator_State hosted{};
            mem::Debug_Allocator_State debug{hosted.get_allocator()};
            cast(void)mem::alloc(32, debug.get_allocator());
        },
        "Debug_Allocator_State: Leaked allocations."
    );
}

TEST(Debug_Allocator_State, detects_double_frees) {
    EXPECT_DEATH(
        {
            mem::Hosted_Allocator_State hosted{};
            mem::Debug_Allocator_State debug{hosted.get_allocator()};
            auto allocation = mem::alloc(32, debug.get_allocator());
            cast(void)mem::free(allocation.memory, 32, debug.get_allocator());
            cast(void)mem::free(allocation.memory, 32, debug.get_allocator());
        },
        "Debug_Allocator_State: Double free."
    );
}

// @TODO(blanktiger): Test and figure out what should happen when you wrap
// something that doesn't allow freeing like an arena that just resets
// everything at once. Do we do special handling here?

TEST(Debug_Allocator_State, tracks_resize_in_place) {
    mem::Temporary_Allocator_State backing{256};
    mem::Debug_Allocator_State debug{backing.get_allocator()};
    PUSH_ALLOCATOR(debug.get_allocator());

    auto allocation = mem::alloc(32, 16);
    ASSERT_NE(allocation.memory, nullptr);
    kstd_memset(allocation.memory, 0xAB, 32);

    auto resized = mem::realloc(allocation.memory, 32, 8, 16);
    ASSERT_EQ(resized.memory, allocation.memory);
    ASSERT_EQ(resized.error,  mem::Allocator_Error::NONE);

    auto info = mem::get_info(resized.memory, debug.get_allocator());
    ASSERT_EQ(info.error, mem::Allocator_Query_Error::NONE);
    ASSERT_EQ(info.value.requested_size,      8);
    ASSERT_EQ(info.value.requested_alignment, 16);
    // Temporary_Allocator_State doesn't implement FREE, but Debug_Allocator tracks the call.
    ASSERT_EQ(mem::free(resized.memory, 8, 16), mem::Allocator_Error::MODE_NOT_IMPLEMENTED);
}

TEST(Debug_Allocator_State, tracks_resize_to_new_pointer) {
    mem::Temporary_Allocator_State backing{256};
    mem::Debug_Allocator_State debug{backing.get_allocator()};
    PUSH_ALLOCATOR(debug.get_allocator());

    auto allocation = mem::alloc(8, 16);
    ASSERT_NE(allocation.memory, nullptr);
    kstd_memset(allocation.memory, 0xAB, 8);

    auto resized = mem::realloc(allocation.memory, 8, 32, 16);
    ASSERT_NE(resized.memory, allocation.memory);
    ASSERT_EQ(resized.error, mem::Allocator_Error::NONE);

    auto* bytes = cast(u8*)resized.memory;
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ(bytes[i], cast(u8)(0xAB));

    auto info = mem::get_info(resized.memory, debug.get_allocator());
    ASSERT_EQ(info.error, mem::Allocator_Query_Error::NONE);
    ASSERT_EQ(info.value.requested_size,      32);
    ASSERT_EQ(info.value.requested_alignment, 16);
    // Temporary_Allocator_State doesn't implement FREE, but Debug_Allocator tracks the call.
    ASSERT_EQ(mem::free(resized.memory, 32, 16), mem::Allocator_Error::MODE_NOT_IMPLEMENTED);
}

TEST(Debug_Allocator_State, resize_of_unknown_pointer_asserts) {
    EXPECT_DEATH(
        {
            mem::Temporary_Allocator_State backing{256};
            mem::Debug_Allocator_State debug{backing.get_allocator()};
            auto backing_allocation = mem::alloc(16, 16, backing.get_allocator());
            cast(void)mem::realloc(backing_allocation.memory, 16, 8, 16, debug.get_allocator());
        },
        "Debug_Allocator_State: Resize of unknown pointer."
    );
}

TEST(Null_Allocator_State, alloc_is_unreachable) {
    EXPECT_DEATH(
        {
            mem::Null_Allocator_State null{};
            cast(void)mem::alloc(16, null.get_allocator());
        },
        "Null_Allocator: Operation called."
    );
}

TEST(Null_Allocator_State, free_is_unreachable) {
    EXPECT_DEATH(
        {
            mem::Null_Allocator_State null{};
            cast(void)mem::free(cast(void*)1, 0, null.get_allocator());
        },
        "Null_Allocator: Operation called."
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
    auto left_before = arena.bytes_left();
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
    auto* bytes = cast(u8*)allocation.memory;
    for (int i = 0; i < 32; ++i) ASSERT_EQ(bytes[i], cast(u8)(0xCC));
}

#endif
