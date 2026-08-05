#pragma once

#include <cstddef>
#include <new>

#include "basic.hh"
#include "assert.hh"
#include "cstring.hh"
#include "pointer_utils.hh"
#include "synchronization.hh"

template <typename T, usize N>
struct Static_Array;

namespace mem {

struct Allocator {
    virtual auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* = 0;
    virtual auto free(void* pointer, usize size, usize alignment = alignof(std::max_align_t)) -> void = 0;
    virtual ~Allocator() = default;
};

force_inline auto resolve_allocator(Allocator* allocator = nullptr) -> Allocator*;

template <typename T>
force_inline auto align_up(T value, T alignment) -> T {
    return (value + alignment - 1) & ~(alignment - 1);
}

template <typename T>
force_inline auto align_down(T value, T alignment) -> T {
    return value & ~(alignment - 1);
}

//
// Per-frame bump allocator. free() is a no-op. Call reset() once per frame (or
// scope) to reclaim memory. Does not own its backing buffer.
//
struct Temporary_Allocator final : Allocator {
    u8* base    = nullptr;
    u8* current = nullptr;
    u8* end     = nullptr;

    auto init(void* memory, usize size) -> void {
        kstd_assert(memory != nullptr);
        kstd_assert(size > 0);
        base    = static_cast<u8*>(memory);
        current = base;
        end     = base + size;
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

    auto mark() const -> u8* {
        return current;
    }

    auto rewind(const u8* mark_point) -> void {
        kstd_assert(mark_point >= base && mark_point <= end);
        current = const_cast<u8*>(mark_point);
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;
        if (size == 0) size = 1;

        auto aligned = reinterpret_cast<u8*>(align_up(ptr_addr(current), static_cast<usize>(alignment)));
        auto* next   = aligned + size;
        if (next > end) return nullptr;

        current = next;
        return aligned;
    }

    auto free(void*, usize, usize = alignof(std::max_align_t)) -> void override {}
};

inline Temporary_Allocator temporary_allocator{};

force_inline auto talloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* {
    void* memory = temporary_allocator.alloc(size, alignment);
    kstd_assert(memory != nullptr, "temporary allocator exhausted");
    return memory;
}

//
// Buddy heap over page-aligned physical regions. Call clear(), then
// add_region() for each usable range, before alloc.
//
struct Buddy_Allocator final : Allocator {
    struct Free_Block {
        Free_Block* next;
    };

    struct Allocation_Header {
        u64 block_base;
        u8  order;
        u8  reserved[7];
    };

    static constexpr usize MIN_ORDER = 12;  // 4 KiB pages.
    static constexpr usize MAX_ORDER = 63;
    static constexpr usize PAGE_SIZE = 1ull << MIN_ORDER;

    Free_Block* free_lists[MAX_ORDER + 1]{};
    // It's the main global allocator, so it has to have a lock on `alloc` and `free`.
    synchronization::Spinlock lock;

    static auto floor_pow2(u64 n) -> u64 {
        if (n == 0) return 0;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n & ~(n >> 1);
    }

    static auto block_size_for_order(usize order) -> usize {
        return 1ull << order;
    }

    static auto order_for_block_size(u64 block_size) -> usize {
        usize order   = MIN_ORDER;
        u64   current = PAGE_SIZE;
        while (current < block_size && order < MAX_ORDER) {
            current <<= 1;
            order++;
        }
        return order;
    }

    static auto next_block_size(u64 required_size) -> u64 {
        u64 block_size = PAGE_SIZE;
        while (block_size < required_size && block_size < (1ull << MAX_ORDER)) {
            block_size <<= 1;
        }
        return block_size;
    }

    auto clear() -> void {
        for (usize i = 0; i <= MAX_ORDER; ++i) free_lists[i] = nullptr;
    }

    auto push_free_block(u64 base, usize order) -> void {
        auto* block = reinterpret_cast<Free_Block*>(base);
        block->next = free_lists[order];
        free_lists[order] = block;
    }

    auto remove_free_block(usize order, u64 base) -> bool {
        auto** link = &free_lists[order];
        while (*link != nullptr) {
            if (ptr_addr(*link) == base) {
                *link = (*link)->next;
                return true;
            }
            link = &((*link)->next);
        }
        return false;
    }

    auto add_region(u64 base, u64 size) -> void {
        const u64 start = align_up(base, static_cast<u64>(PAGE_SIZE));
        const u64 end   = align_down(base + size, static_cast<u64>(PAGE_SIZE));
        if (start >= end) return;

        u64 current = start;
        while (current < end) {
            u64 block_size = floor_pow2(end - current);
            while (block_size > PAGE_SIZE && (current & (block_size - 1)) != 0) {
                block_size >>= 1;
            }

            if (block_size < PAGE_SIZE) block_size = PAGE_SIZE;

            const usize order = order_for_block_size(block_size);
            push_free_block(current, order);
            current += block_size;
        }
    }

    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment == 0) alignment = 1;
        if (size == 0) size = 1;

        const u64 required_size = static_cast<u64>(size) + static_cast<u64>(alignment) + sizeof(Allocation_Header);
        if (required_size < size) return nullptr;

        auto scoped_lock = lock.scoped_irq_lock();

        const u64 target_block_size = next_block_size(required_size);
        if (target_block_size < PAGE_SIZE) return nullptr;

        const usize target_order = order_for_block_size(target_block_size);

        usize order = target_order;
        while (order <= MAX_ORDER && free_lists[order] == nullptr) order++;
        if (order > MAX_ORDER) return nullptr;

        u64 block_base = ptr_addr(free_lists[order]);
        free_lists[order] = free_lists[order]->next;

        while (order > target_order) {
            order--;
            const u64 split_size = block_size_for_order(order);
            push_free_block(block_base + split_size, order);
        }

        const u64 block_size = block_size_for_order(order);
        const u64 user_ptr   = align_up(block_base + sizeof(Allocation_Header), static_cast<u64>(alignment));
        if (user_ptr + size > block_base + block_size) {
            push_free_block(block_base, order);
            return nullptr;
        }

        auto* header = reinterpret_cast<Allocation_Header*>(user_ptr - sizeof(Allocation_Header));
        *header = Allocation_Header{};
        header->block_base = block_base;
        header->order      = static_cast<u8>(order);
        return reinterpret_cast<void*>(user_ptr);
    }

    auto free(void* pointer, usize, usize alignment = alignof(std::max_align_t)) -> void override {
        (void)alignment;
        if (pointer == nullptr) return;

        auto scoped_lock = lock.scoped_irq_lock();

        Allocation_Header header{};
        kstd_memcpy(&header, reinterpret_cast<void*>(ptr_addr(pointer) - sizeof(Allocation_Header)), sizeof(header));

        u64   block_base = header.block_base;
        usize order      = header.order;

        while (order < MAX_ORDER) {
            const u64 block_size = block_size_for_order(order);
            const u64 buddy_base = block_base ^ block_size;
            if (!remove_free_block(order, buddy_base)) break;

            if (buddy_base < block_base) block_base = buddy_base;
            order++;
        }

        push_free_block(block_base, order);
    }
};

constexpr usize DEFAULT_ARENA_RESERVE_SIZE = 1 * 1024 * 1024;
constexpr usize DEFAULT_ARENA_PAGE_SIZE    = 4096;

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
          allocated(align_up(reserve, DEFAULT_ARENA_PAGE_SIZE)),
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
// Thin leak-checking wrapper. Forwards alloc/free to a backing allocator and
// asserts in the destructor that every allocation was freed.
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

    Debug_Allocator() = default;

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

        unreachable("Debug_Allocator: free of untracked pointer");
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

#if HOSTED

// Hosted unit tests have no buddy or arena. Wrap the C++ heap so Array
// and other types can still exercise the Allocator pointer path.
struct Hosted_Allocator final : Allocator {
    auto alloc(usize size, usize alignment = alignof(std::max_align_t)) -> void* override {
        if (alignment < alignof(std::max_align_t)) alignment = alignof(std::max_align_t);
        if (size == 0) size = 1;
        void* pointer = ::operator new(size, std::align_val_t{alignment});
        return pointer;
    }

    auto free(void* pointer, usize, usize alignment = alignof(std::max_align_t)) -> void override {
        if (pointer == nullptr) return;
        if (alignment < alignof(std::max_align_t)) alignment = alignof(std::max_align_t);
        ::operator delete(pointer, std::align_val_t{alignment});
    }
};

#endif

namespace hidden {
    inline Buddy_Allocator buddy{};

#if HOSTED
    inline Hosted_Allocator hosted_allocator{};

#if UNIT_TEST
    inline Debug_Allocator debug_allocator{&hosted_allocator};
#endif

    // Enough for unit tests that exercise tprint and tcopy without the kernel buddy.
    constexpr usize HOSTED_TEMPORARY_ALLOCATOR_SIZE = 256 * 1024;
    alignas(16) inline u8 hosted_temporary_allocator_buffer[HOSTED_TEMPORARY_ALLOCATOR_SIZE];

    struct Hosted_Allocator_Init {
        Hosted_Allocator_Init() {
            temporary_allocator.init(
                hosted_temporary_allocator_buffer,
                HOSTED_TEMPORARY_ALLOCATOR_SIZE
            );
        }
    };

    inline Hosted_Allocator_Init hosted_allocator_init{};
#endif

#if HOSTED
#if UNIT_TEST
    inline Allocator* current_global_allocator = &debug_allocator;
#else
    inline Allocator* current_global_allocator = &hosted_allocator;
#endif
#else
    inline Allocator* current_global_allocator = &buddy;
#endif
}

inline Allocator* default_global_allocator = hidden::current_global_allocator;

force_inline auto set_global_allocator(Allocator* allocator) -> void {
    hidden::current_global_allocator = allocator;
}

force_inline auto resolve_allocator(Allocator* allocator) -> Allocator* {
    return allocator != nullptr ? allocator : hidden::current_global_allocator;
}

struct Push_Allocator {
    Allocator* previous_allocator;

    Push_Allocator(Allocator* new_allocator) : previous_allocator(hidden::current_global_allocator) {
        set_global_allocator(new_allocator);
    }
    ~Push_Allocator() { set_global_allocator(previous_allocator); }
};

}  // namespace mem

// Named RAII so destructor runs at scope exit.
#define PUSH_ALLOCATOR(allocator) mem::Push_Allocator DEFER_UNIQ(_push_allocator_)(allocator)

#ifdef UNIT_TESTS_KSTD_ALLOCATOR

#include "array.hh"

TEST(Debug_Allocator, allows_destruction_when_all_freed) {
    mem::Hosted_Allocator hosted{};
    mem::Debug_Allocator  debug{&hosted};

    void* a = debug.alloc(16);
    void* b = debug.alloc(64);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    debug.free(a, 16);
    debug.free(b, 64);
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
