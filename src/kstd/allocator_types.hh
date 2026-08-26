#pragma once

#include "basic.hh"

namespace mem {

// @TODO(blanktiger): Give @enum_to_string for every new enum.
//
// @TODO(blanktiger): Write documentation on how to implement each (how to interpret each of the values in the proc).
enum struct Allocator_Mode : u8 {
    ALLOCATE,
    FREE,
    RESIZE,
    FEATURES,
    IS_THIS_YOURS,
    INFO,
};

// @TODO(blanktiger): Make preprocessor directive that will generate operators
// for an enum_flag struct. Also make it verify that values don't collide.
enum struct Allocator_Features : u32 {
    NONE                        = 0b000000000000,

    FREE                        = 0b000000000001, // Allocator actually frees memory.
    RESIZE_SHRINK_NO_OP         = 0b000000000010, // Resize to smaller or equal size preserves pointer and storage.
    ACTUALLY_RESIZE             = 0b000000000100, // Allocator implements native RESIZE. Otherwise falls back to alloc+copy+free.
    THREADSAFE                  = 0b000000001000, // All allocator operations may be called concurrently.
    IS_THIS_YOURS               = 0b000000010000, // Allocator supports ownership queries for pointers.
    INFO                        = 0b000000100000, // Allocator supports allocation metadata queries.
    FAST_BUMP_ALLOCATOR         = 0b000001000000, // Allocator is optimized for monotonic bump allocation.
    GENERAL_HEAP_ALLOCATOR      = 0b000010000000, // Allocator is intended for general-purpose allocations.
    PER_FRAME_TEMPORARY_STORAGE = 0b000100000000, // Allocator is intended for temporary per-frame allocations.
    DEBUG_ALLOCATOR             = 0b001000000000, // Allocator provides allocation diagnostics.
};

constexpr auto operator | (Allocator_Features left, Allocator_Features right) -> Allocator_Features {
    auto value = cast(u32)left | cast(u32)right;
    return cast(Allocator_Features)value;
}

constexpr auto operator & (Allocator_Features left, Allocator_Features right) -> Allocator_Features {
    auto value = cast(u32)left & cast(u32)right;
    return cast(Allocator_Features)value;
}

enum struct Allocator_Error : u8 {
    NONE,
    OUT_OF_MEMORY,
    USE_OF_UNINITIALIZED_ALLOCATOR,
    INVALID_POINTER,
    INVALID_ARGUMENT,
    MODE_NOT_IMPLEMENTED,
};

struct Allocator_Result {
    void*           memory{};
    Allocator_Error error = Allocator_Error::NONE;
};

enum struct Allocator_Query_Error : u8 {
    NONE,
    QUERY_NOT_IMPLEMENTED,
};

template <typename T>
struct Allocator_Query_Result {
    T                     value{};
    Allocator_Query_Error error = Allocator_Query_Error::NONE;
};

struct Allocator_Info {
    void* pointer{}; // Pointer passed to get_info.
    ssize requested_size{};
    ssize requested_alignment{};
};

using Allocator_Proc = Allocator_Result (*)(Allocator_Mode mode, ssize size, ssize alignment, ssize old_size, void* old_memory, void* allocator_data);

// @TODO(blanktiger): Note in documentation that every allocators get_allocator that includes pointer to state must obviously outlive everything that uses the returned Allocator
struct Allocator {
    Allocator_Proc proc{};
    void*          data{};

    auto operator == (const Allocator&) const -> bool = default;
    auto valid() const -> bool { return proc != nullptr; }
};

}  // namespace mem
