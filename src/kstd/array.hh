#pragma once

#include <new>
#include <type_traits>
#include <utility>
#include <source_location>
#include <iterator>

#include "basic.hh"
#include "cstring.hh"
#include "assert.hh"
#include "allocator.hh"
#include "string.hh"
#include "array_iterator.hh"

template <typename T, usize N>
struct Array_View {
    static constexpr auto size = N;
    static constexpr auto size_in_bytes = sizeof(T) * N;
    T* data;

    auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    constexpr auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    constexpr auto elements()       -> T*       { return data; }
    constexpr auto elements() const -> const T* { return data; }
    constexpr auto empty() const -> bool { return size == 0; }

    force_inline auto slice(usize index, usize count) -> Array_View<T> {
        return Array_View<T>{N, data}.slice(index, count);
    }

    force_inline auto slice(usize index, usize count) const -> Array_View<const T> {
        return Array_View<const T>{N, data}.slice(index, count);
    }

    // Byte views can be reinterpreted as text. Defined on this side (rather
    // than as a string constructor) so string.hh does not need to know about arrays.
    // This header includes it for its own asserts.
    explicit operator string() const requires std::is_same_v<std::remove_const_t<T>, u8> {
        return string(reinterpret_cast<const char*>(data), N);
    }

    ARRAY_ITERATOR()
};

template <typename T>
struct Array_View<T, DYNAMIC_EXTENT> {
    usize size;
    T* data;

    auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    constexpr auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    constexpr auto elements()       -> T*       { return data; }
    constexpr auto elements() const -> const T* { return data; }
    auto empty() const -> bool { return size == 0; }

    force_inline auto slice(usize index, usize count) -> Array_View<T> {
        if (index >= size)
            return {0, data};

        auto remaining = size - index;
        auto length    = count > remaining ? remaining : count;
        return {length, data + index};
    }

    force_inline auto slice(usize index, usize count) const -> Array_View<const T> {
        if (index >= size)
            return {0, data};

        auto remaining = size - index;
        auto length    = count > remaining ? remaining : count;
        return {length, data + index};
    }

    explicit operator string() const requires std::is_same_v<std::remove_const_t<T>, u8> {
        return string(reinterpret_cast<const char*>(data), size);
    }

    ARRAY_ITERATOR()
};

namespace mem {

struct Array_Allocation_Header {
    void* base;
    usize size;
};

template <typename T>
force_inline auto alloc_array_size(usize count, usize alignment = alignof(T)) -> usize {
    if (count == 0) return 0;
    if (count > static_cast<usize>(-1) / sizeof(T)) return 0;

    if (alignment < alignof(T)) alignment = alignof(T);
    if ((alignment & (alignment - 1)) != 0) return 0;

    const usize size = count * sizeof(T);
    if (size > static_cast<usize>(-1) - sizeof(Array_Allocation_Header)) return 0;

    const usize size_with_header = size + sizeof(Array_Allocation_Header);
    if (size_with_header > static_cast<usize>(-1) - (alignment - 1)) return 0;

    return size_with_header + alignment - 1;
}

template <typename T>
force_inline auto alloc_array(
    usize count,
    usize alignment = alignof(T),
    Allocator* allocator = nullptr
) -> Array_View<T> {
    if (alignment < alignof(T)) alignment = alignof(T);

    const usize allocation_size = alloc_array_size<T>(count, alignment);
    if (allocation_size == 0) return {0, nullptr};

    void* base = alloc(allocation_size, alignof(std::max_align_t), allocator);
    if (base == nullptr) return {0, nullptr};

    auto* data = reinterpret_cast<T*>(align_up(
        ptr_addr(base) + sizeof(Array_Allocation_Header),
        static_cast<psize>(alignment)
    ));
    auto* header = reinterpret_cast<Array_Allocation_Header*>(
        reinterpret_cast<u8*>(data) - sizeof(Array_Allocation_Header)
    );
    header->base = base;
    header->size = allocation_size;
    return {count, data};
}

template <typename T>
force_inline auto alloc_array(usize count, Allocator* allocator) -> Array_View<T> {
    return alloc_array<T>(count, alignof(T), allocator);
}

template <typename T>
force_inline auto free_array(void* pointer, Allocator* allocator = nullptr) -> void {
    if (pointer == nullptr) return;

    auto* header = reinterpret_cast<Array_Allocation_Header*>(
        static_cast<u8*>(pointer) - sizeof(Array_Allocation_Header)
    );
    free(header->base, header->size, alignof(std::max_align_t), allocator);
}

template <typename T>
force_inline auto free_array(Array_View<T> array, Allocator* allocator = nullptr) -> void {
    free_array<T>(array.data, allocator);
}

}  // namespace mem

template <typename T, usize N>
struct Static_Array {
    static constexpr auto size = N;
    static constexpr auto size_in_bytes = sizeof(T) * N;
    T data[N];

    auto fill(const T&& value) -> void {
        for (usize i = 0; i < N; ++i)
            data[i] = value;
    }

    constexpr auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    constexpr auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto elements()       -> T*       { return data; }
    auto elements() const -> const T* { return data; }
    constexpr auto empty() const -> bool { return size == 0; }

    auto pop_back() -> T {
        kstd_assert(N > 0, "pop_back on empty Static_Array");
        return std::move(data[N - 1]);
    }

    force_inline auto slice(usize index, usize count) -> Array_View<T> {
        return Array_View<T, N>{data}.slice(index, count);
    }

    force_inline auto slice(usize index, usize count) const -> Array_View<const T> {
        return Array_View<const T, N>{data}.slice(index, count);
    }

    operator Array_View<T, N>() { return Array_View<T, N>{data}; }
    operator Array_View<T>()    { return Array_View<T>{size, data}; }

    ARRAY_ITERATOR()
};

#ifdef UNIT_TESTS_KSTD_ARRAY

TEST(Static_Array, stores_values) {
    Static_Array<int, 3> arr{{1, 2, 3}};

    EXPECT_EQ(arr.size, 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST(Static_Array, elements_returns_data_pointer) {
    Static_Array<int, 2> arr{{10, 20}};

    auto* ptr = arr.elements();

    EXPECT_EQ(ptr[0], 10);
    EXPECT_EQ(ptr[1], 20);
}

TEST(Static_Array, pop_back_returns_last_element) {
    Static_Array<int, 3> arr{{1, 2, 3}};

    EXPECT_EQ(arr.pop_back(), 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST(Static_Array, out_of_bounds_asserts) {
    Static_Array<int, 2> arr{{1, 2}};

    EXPECT_DEATH(arr[2], "index out of bounds");
}

TEST(Static_Array, converts_to_static_array_view) {
    Static_Array<int, 3> arr{{1, 2, 3}};

    Array_View<int, 3> view = arr;

    EXPECT_EQ(view.size, 3);
    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[1], 2);
    EXPECT_EQ(view[2], 3);
    EXPECT_EQ(view.data, arr.elements());
}

TEST(Static_Array, converts_to_array_view) {
    Static_Array<int, 3> arr{{1, 2, 3}};

    Array_View<int> view = arr;

    EXPECT_EQ(view.size, 3);
    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[1], 2);
    EXPECT_EQ(view[2], 3);
    EXPECT_EQ(view.data, arr.elements());
}

TEST(Static_Array, passes_implicitly_to_function_taking_static_array_view) {
    Static_Array<int, 2> arr{{10, 20}};

    auto sum = [](Array_View<int, 2> view) {
        return view[0] + view[1];
    };

    EXPECT_EQ(sum(arr), 30);
}

TEST(Static_Array, passes_implicitly_to_function_taking_array_view) {
    Static_Array<int, 2> arr{{10, 20}};

    auto sum = [](Array_View<int> view) {
        return view[0] + view[1];
    };

    EXPECT_EQ(sum(arr), 30);
}

#endif

//
// Use as a normal Array. The exception is this can't grow, because it's backed by static memory.
//
template <typename T, usize N>
struct Bounded_Array {
    static constexpr auto MAX_SIZE = N;
    static constexpr auto size_in_bytes = sizeof(T) * N;
    usize size = 0;
    // Raw byte storage avoids default-constructing every slot up front.
    // Without this, push_back placement-new would run a second constructor
    // over an already-live object. It is UB for any non-trivial T.
    alignas(T) u8 data[sizeof(T) * N];

    Bounded_Array() = default;

    Bounded_Array(usize initial_size, const T& initial_value) : size(initial_size) {
        kstd_assert(initial_size <= MAX_SIZE, "initial_size exceeds MAX_SIZE");
        for (usize i = 0; i < size; ++i)
            ::new (slot(i)) T(initial_value);
    }

    ~Bounded_Array() {
        for (usize i = 0; i < size; ++i)
            slot(i)->~T();
    }

    Bounded_Array(const Bounded_Array& from) : size(from.size) {
        for (usize i = 0; i < size; ++i)
            ::new (slot(i)) T(*from.slot(i));
    }

    Bounded_Array(Bounded_Array&& from) noexcept : size(from.size) {
        for (usize i = 0; i < size; ++i) {
            ::new (slot(i)) T(std::move(*from.slot(i)));
            from.slot(i)->~T();
        }
        from.size = 0;
    }

    auto operator = (const Bounded_Array& from) -> Bounded_Array& {
        if (this == &from)
            return *this;

        for (usize i = 0; i < size; ++i)
            slot(i)->~T();

        size = from.size;
        for (usize i = 0; i < size; ++i)
            ::new (slot(i)) T(*from.slot(i));

        return *this;
    }

    auto operator = (Bounded_Array&& from) noexcept -> Bounded_Array& {
        if (this == &from)
            return *this;

        for (usize i = 0; i < size; ++i)
            slot(i)->~T();

        size = from.size;
        for (usize i = 0; i < size; ++i) {
            ::new (slot(i)) T(std::move(*from.slot(i)));
            from.slot(i)->~T();
        }
        from.size = 0;

        return *this;
    }

    auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return *slot(index);
    }

    auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return *slot(index);
    }

    auto push_back(T&& element) -> void {
        kstd_assert(size < MAX_SIZE, "push_back on full Bounded_Array");
        ::new (slot(size)) T(std::move(element));
        ++size;
    }

    auto push_back(const T& element) -> void {
        kstd_assert(size < MAX_SIZE, "push_back on full Bounded_Array");
        ::new (slot(size)) T(element);
        ++size;
    }

    auto ensure_space_for(usize new_elements_count) -> void {
        kstd_assert(
            new_elements_count <= MAX_SIZE - size,
            "not enough space in Bounded_Array"
        );
    }

    auto extend(const Array_View<T> from) -> void {
        const usize source_size = from.size;
        ensure_space_for(source_size);
        for (usize i = 0; i < source_size; ++i)
            push_back(from[i]);
    }

    auto pop_back() -> T {
        kstd_assert(size > 0, "pop_back on empty Bounded_Array");
        auto element = std::move(*slot(size - 1));
        slot(size - 1)->~T();
        --size;
        return element;
    }

    auto elements()       -> T*       { return slot(0); }
    auto elements() const -> const T* { return slot(0); }
    auto empty() const -> bool { return size == 0; }

    force_inline auto slice(usize index, usize count) -> Array_View<T> {
        return Array_View<T>{size, slot(0)}.slice(index, count);
    }

    force_inline auto slice(usize index, usize count) const -> Array_View<const T> {
        return Array_View<const T>{size, slot(0)}.slice(index, count);
    }

    operator Array_View<T>() { return Array_View<T>{size, slot(0)}; }

    ARRAY_ITERATOR()

private:
    auto slot(usize i) -> T* {
        return reinterpret_cast<T*>(data + i * sizeof(T));
    }
    auto slot(usize i) const -> const T* {
        return reinterpret_cast<const T*>(data + i * sizeof(T));
    }
};

#ifdef UNIT_TESTS_KSTD_ARRAY

TEST(Bounded_Array, default_is_empty) {
    Bounded_Array<int, 4> arr;

    EXPECT_EQ(arr.size, 0);
}

TEST(Bounded_Array, push_back_grows_size) {
    Bounded_Array<int, 4> arr;

    arr.push_back(1);
    arr.push_back(2);

    EXPECT_EQ(arr.size, 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST(Bounded_Array, push_back_past_max_size_asserts) {
    Bounded_Array<int, 2> arr;

    arr.push_back(1);
    arr.push_back(2);

    EXPECT_DEATH(arr.push_back(3), "push_back on full Bounded_Array");
}

TEST(Bounded_Array, ensure_space_for_rejects_too_many_elements) {
    Bounded_Array<int, 2> arr;
    arr.push_back(1);

    EXPECT_DEATH(arr.ensure_space_for(2), "not enough space in Bounded_Array");
}

TEST(Bounded_Array, extend_appends_another_array) {
    Bounded_Array<int, 4> arr;
    Bounded_Array<int, 2> other;
    arr.push_back(1);
    other.push_back(2);
    other.push_back(3);

    arr.extend(other);

    EXPECT_EQ(arr.size, 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
}

TEST(Bounded_Array, pop_back_returns_last_element_and_shrinks_size) {
    Bounded_Array<int, 3> arr;

    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    EXPECT_EQ(arr.pop_back(), 3);
    EXPECT_EQ(arr.size, 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST(Bounded_Array, converts_to_array_view) {
    Bounded_Array<int, 4> arr;

    arr.push_back(1);
    arr.push_back(2);

    Array_View<int> view = arr;

    EXPECT_EQ(view.size, 2);
    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[1], 2);
    EXPECT_EQ(view.data, arr.elements());
}

TEST(Bounded_Array, view_size_reflects_current_size_not_max_size) {
    Bounded_Array<int, 4> arr;

    arr.push_back(1);

    Array_View<int> view = arr;

    EXPECT_EQ(view.size, 1);
    EXPECT_EQ(arr.MAX_SIZE, 4);
}

TEST(Bounded_Array, passes_implicitly_to_function_taking_array_view) {
    Bounded_Array<int, 4> arr;

    arr.push_back(6);
    arr.push_back(7);

    auto sum = [](Array_View<int> view) {
        int total = 0;
        for (usize i = 0; i < view.size; ++i)
            total += view[i];
        return total;
    };

    EXPECT_EQ(sum(arr), 13);
}

#endif

//
// Growable heap array. Stores the Allocator pointer that owns `data` so free
// and grow always use the same heap. Pass null to capture the current global
// allocator at construction.
//
template <typename T>
struct Array {
    // Allocator first. Member init order follows declaration order.
    mem::Allocator* allocator = nullptr;
    usize           capacity  = 0;
    usize           size      = 0;
    T*              data      = nullptr;

    Array(mem::Allocator* allocator = nullptr)
        : allocator(mem::resolve_allocator(allocator)),
          capacity(1),
          size(0),
          data(allocate_storage(1)) {}

    Array(usize initial_capacity, mem::Allocator* allocator = nullptr)
        : allocator(mem::resolve_allocator(allocator)),
          capacity(initial_capacity == 0 ? 1 : initial_capacity),
          size(0),
          data(allocate_storage(capacity)) {}

    Array(usize initial_size, const T& initial_value, mem::Allocator* allocator = nullptr)
        : allocator(mem::resolve_allocator(allocator)),
          capacity(initial_size == 0 ? 1 : initial_size),
          size(0),
          data(allocate_storage(capacity)) {
        for (usize index = 0; index < initial_size; ++index) {
            ::new (static_cast<void*>(data + index)) T(initial_value);
            ++size;
        }
    }

    ~Array() {
        destroy_elements();
        free_storage(data, capacity);
    }

    Array(const Array& from)
        : allocator(mem::resolve_allocator(from.allocator)),
          capacity(from.capacity == 0 ? 1 : from.capacity),
          size(from.size),
          data(allocate_storage(capacity)) {
        for (usize i = 0; i < size; ++i)
            ::new (data + i) T(from.data[i]);
    }

    auto operator = (const Array& from) -> Array& {
        if (this == &from) return *this;

        destroy_elements();
        // @TODO: Consider not deleting this if it's big enough. Also look at
        //        other constructors and do the same.
        free_storage(data, capacity);

        allocator = mem::resolve_allocator(from.allocator);
        capacity  = from.capacity == 0 ? 1 : from.capacity;
        size      = from.size;
        data      = allocate_storage(capacity);

        for (usize i = 0; i < size; ++i)
            ::new (data + i) T(from.data[i]);

        return *this;
    }

    Array(Array&& from) noexcept
        : allocator(from.allocator),
          capacity(from.capacity),
          size(from.size),
          data(from.data) {
        from.allocator = nullptr;
        from.capacity  = 0;
        from.size      = 0;
        from.data      = nullptr;
    }

    auto operator = (Array&& from) noexcept -> Array& {
        if (this == &from)
            return *this;

        destroy_elements();
        free_storage(data, capacity);

        allocator = from.allocator;
        capacity  = from.capacity;
        size      = from.size;
        data      = from.data;

        from.allocator = nullptr;
        from.capacity  = 0;
        from.size      = 0;
        from.data      = nullptr;

        return *this;
    }

    auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto operator [] (u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto reserve(usize min_capacity) -> void {
        if (min_capacity <= capacity) return;

        ensure_allocator();

        usize new_capacity = capacity == 0 ? 16 : capacity;
        while (new_capacity < min_capacity) new_capacity *= 2;

        T* new_data = allocate_storage(new_capacity);
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (data != nullptr && size > 0)
                kstd_memcpy(new_data, data, sizeof(T) * size);
        } else {
            for (usize i = 0; i < size; ++i) {
                ::new (new_data + i) T(std::move(data[i]));
                data[i].~T();
            }
        }
        free_storage(data, capacity);

        data     = new_data;
        capacity = new_capacity;
    }

    auto ensure_space_for(usize new_elements_count) -> void {
        kstd_assert(new_elements_count <= static_cast<usize>(-1) - size, "Array size overflow");
        reserve(size + new_elements_count);
    }

    auto push_back(T&& element) -> void {
        reserve(size + 1);
        ::new (data + size) T(std::move(element));
        ++size;
    }

    auto push_back(const T& element) -> void {
        reserve(size + 1);
        ::new (data + size) T(element);
        ++size;
    }

    auto extend(const Array_View<T> from) -> void {
        const usize source_size = from.size;
        const bool source_is_self = from.data == data;
        ensure_space_for(source_size);
        for (usize i = 0; i < source_size; ++i)
            push_back(from[i]);
    }

    auto pop_back() -> T {
        kstd_assert(size > 0, "pop_back on empty Array");
        auto element = std::move(data[size - 1]);
        data[size - 1].~T();
        --size;
        return element;
    }

    // Move all elements back by one in O(n) time.
    auto pop_front() -> T {
        kstd_assert(size > 0, "pop_front on empty Array");
        auto element = std::move(data[0]);
        data[0].~T();
        for (usize i = 1; i < size; ++i) {
            ::new (data + i - 1) T(std::move(data[i]));
            data[i].~T();
        }
        --size;
        return element;
    }

    auto clear() {
        destroy_elements();
        size = 0;
    }

    auto elements()       ->       T* { return data; }
    auto elements() const -> const T* { return data; }
    auto empty() const -> bool { return size == 0; }

    force_inline auto slice(usize index, usize count) -> Array_View<T> {
        return Array_View<T>{size, data}.slice(index, count);
    }

    force_inline auto slice(usize index, usize count) const -> Array_View<const T> {
        return Array_View<const T>{size, data}.slice(index, count);
    }

    operator Array_View<T>() { return Array_View<T>{size, data}; }

    ARRAY_ITERATOR()

private:
    auto ensure_allocator() -> void {
        if (allocator == nullptr)
            allocator = mem::resolve_allocator();
    }

    auto allocate_storage(usize count) -> T* {
        ensure_allocator();
        if (count == 0) return nullptr;
        void* memory = allocator->alloc(sizeof(T) * count, alignof(T));
        kstd_assert(memory != nullptr, "Array allocation failed");
        return static_cast<T*>(memory);
    }

    auto free_storage(T* pointer, usize count) -> void {
        if (pointer == nullptr) return;
        ensure_allocator();
        allocator->free(pointer, sizeof(T) * count, alignof(T));
    }

    auto destroy_elements() -> void {
        for (usize i = 0; i < size; ++i)
            data[i].~T();
    }
};


#ifdef UNIT_TESTS_KSTD_ARRAY

TEST(Array, default_is_empty) {
    Array<int> arr;

    EXPECT_EQ(arr.size, 0);
    EXPECT_EQ(arr.capacity, 1);
    EXPECT_NE(arr.allocator, nullptr);
}

TEST(Array, remembers_explicit_allocator) {
    mem::Hosted_Allocator allocator;
    Array<int> arr(&allocator);

    EXPECT_EQ(arr.allocator, &allocator);

    arr.push_back(1);
    arr.push_back(2);

    EXPECT_EQ(arr.size, 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr.allocator, &allocator);
}

TEST(Array, copy_keeps_source_allocator) {
    mem::Hosted_Allocator allocator;
    Array<int> first(&allocator);
    first.push_back(7);

    Array<int> second = first;

    EXPECT_EQ(second.allocator, &allocator);
    EXPECT_EQ(second.size, 1);
    EXPECT_EQ(second[0], 7);
}

TEST(Array, move_transfers_allocator) {
    mem::Hosted_Allocator allocator;
    Array<int> first(&allocator);
    first.push_back(9);

    Array<int> second = std::move(first);

    EXPECT_EQ(second.allocator, &allocator);
    EXPECT_EQ(second[0], 9);
    EXPECT_EQ(first.allocator, nullptr);
    EXPECT_EQ(first.data, nullptr);
}

TEST(Array, push_back_grows_size) {
    Array<int> arr;

    arr.push_back(1);
    arr.push_back(2);

    EXPECT_EQ(arr.size, 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST(Array, push_back_increases_capacity_when_full) {
    Array<int> arr(2);

    arr.push_back(1);
    arr.push_back(2);

    usize pre_capacity = arr.capacity;

    arr.push_back(3);

    EXPECT_EQ(arr.size, 3);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(pre_capacity, 2);
    EXPECT_EQ(arr.capacity, 4);
}

TEST(Array, pop_back_returns_last_element_and_shrinks_size) {
    Array<int> arr;

    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    EXPECT_EQ(arr.pop_back(), 3);
    EXPECT_EQ(arr.size, 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
}

TEST(Array, reserve_increases_capacity) {
    Array<int> arr;

    auto old_capacity = arr.capacity;

    arr.reserve(100);

    EXPECT_TRUE(arr.capacity >= 100);
    EXPECT_TRUE(arr.capacity > old_capacity);
}

TEST(Array, ensure_space_for_reserves_space_for_new_elements) {
    Array<int> arr;
    arr.push_back(1);
    auto old_capacity = arr.capacity;

    arr.ensure_space_for(10);

    EXPECT_TRUE(arr.capacity >= arr.size + 10);
    EXPECT_TRUE(arr.capacity > old_capacity);
    EXPECT_EQ(arr[0], 1);
}

TEST(Array, extend_appends_another_array) {
    Array<int> arr;
    Static_Array other{{2, 3, 4}};
    arr.push_back(1);

    arr.extend(Array_View<const int>{other.size, other.elements()});

    EXPECT_EQ(arr.size, 4);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 3);
    EXPECT_EQ(arr[3], 4);
}

TEST(Array, extend_supports_self_extension) {
    Array<int> arr;
    arr.push_back(1);
    arr.push_back(2);

    arr.extend(Array_View<const int>{arr.size, arr.data});

    EXPECT_EQ(arr.size, 4);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);
    EXPECT_EQ(arr[2], 1);
    EXPECT_EQ(arr[3], 2);
}

TEST(Array, pop_front_removes_first_element) {
    Array<int> arr;

    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    EXPECT_EQ(arr.pop_front(), 1);

    EXPECT_EQ(arr.size, 2);
    EXPECT_EQ(arr[0], 2);
    EXPECT_EQ(arr[1], 3);
}

TEST(Array, clear_removes_all_elements) {
    Array<int> arr;

    arr.push_back(1);
    arr.push_back(2);

    arr.clear();

    EXPECT_EQ(arr.size, 0);
}

TEST(Array, out_of_bounds_asserts) {
    Array<int> arr;

    arr.push_back(42);

    EXPECT_DEATH(arr[1], "index out of bounds");
}

TEST(Array, move_constructor_transfers_ownership) {
    Array<int> first;

    first.push_back(123);

    Array<int> second(std::move(first));

    EXPECT_EQ(second.size, 1);
    EXPECT_EQ(second[0], 123);

    EXPECT_EQ(first.size, 0);
    EXPECT_EQ(first.data, nullptr);
}

TEST(Array, move_assignment_transfers_ownership) {
    Array<int> first;
    Array<int> second;

    first.push_back(55);
    second.push_back(99);

    second = std::move(first);

    EXPECT_EQ(second.size, 1);
    EXPECT_EQ(second[0], 55);

    EXPECT_EQ(first.size, 0);
    EXPECT_EQ(first.data, nullptr);
}

TEST(Array, converts_to_array_view) {
    Array<int> arr;

    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    Array_View<int> view = arr;

    EXPECT_EQ(view.size, 3);
    EXPECT_EQ(view[0], 1);
    EXPECT_EQ(view[1], 2);
    EXPECT_EQ(view[2], 3);
    EXPECT_EQ(view.data, arr.elements());
}

TEST(Array, view_size_matches_size_not_capacity) {
    Array<int> arr(8);

    arr.push_back(1);
    arr.push_back(2);

    Array_View<int> view = arr;

    EXPECT_EQ(view.size, 2);
    EXPECT_TRUE(arr.capacity >= 8);
}

TEST(Array, passes_implicitly_to_function_taking_array_view) {
    Array<int> arr;

    arr.push_back(4);
    arr.push_back(5);

    auto sum = [](Array_View<int> view) {
        int total = 0;
        for (usize i = 0; i < view.size; ++i)
            total += view[i];
        return total;
    };

    EXPECT_EQ(sum(arr), 9);
}

TEST(Array_View, slice_returns_requested_range) {
    Static_Array values{{1, 2, 3, 4}};
    Array_View<int> view = values;

    auto part = view.slice(1, 2);

    EXPECT_EQ(part.size, 2);
    EXPECT_EQ(part[0], 2);
    EXPECT_EQ(part[1], 3);

    part[0] = 9;
    EXPECT_EQ(values[1], 9);
}

TEST(Array_View, slice_clamps_count_and_handles_out_of_bounds_index) {
    Static_Array values{{1, 2, 3}};
    Array_View<int, values.size> view{values.data};

    EXPECT_EQ(view.slice(1, 20).size, 2);
    EXPECT_EQ(view.slice(1, 0).size, 0);
    EXPECT_EQ(view.slice(3, 1).size, 0);
    EXPECT_EQ(view.slice(4, 1).size, 0);
}

TEST(Array_View, const_slice_returns_const_view) {
    Static_Array values{{1, 2}};
    const Array_View<int> view = values;

    auto part = view.slice(0, 1);

    EXPECT_EQ(part.size, 1);
    EXPECT_EQ(part[0], 1);
}

TEST(Static_Array, slice_forwards_to_view) {
    Static_Array<int, 3> arr{{1, 2, 3}};

    auto part = arr.slice(1, 2);

    EXPECT_EQ(part.size, 2);
    EXPECT_EQ(part[0], 2);
    EXPECT_EQ(part[1], 3);
}

TEST(Static_Array, const_slice_returns_const_view) {
    Static_Array<int, 3> arr{{1, 2, 3}};
    const auto& const_arr = arr;

    auto part = const_arr.slice(1, 1);

    EXPECT_EQ(part.size, 1);
    EXPECT_EQ(part[0], 2);
}

TEST(Bounded_Array, slice_forwards_to_view) {
    Bounded_Array<int, 3> arr;
    arr.push_back(1);
    arr.push_back(2);

    auto part = arr.slice(1, 2);

    EXPECT_EQ(part.size, 1);
    EXPECT_EQ(part[0], 2);
}

TEST(Bounded_Array, const_slice_returns_const_view) {
    Bounded_Array<int, 3> arr;
    arr.push_back(1);
    arr.push_back(2);
    const auto& const_arr = arr;

    auto part = const_arr.slice(1, 1);

    EXPECT_EQ(part.size, 1);
    EXPECT_EQ(part[0], 2);
}

TEST(Array, slice_forwards_to_view) {
    Array<int> arr;
    arr.push_back(1);
    arr.push_back(2);

    auto part = arr.slice(1, 2);

    EXPECT_EQ(part.size, 1);
    EXPECT_EQ(part[0], 2);
}

TEST(Array, const_slice_returns_const_view) {
    Array<int> arr;
    arr.push_back(1);
    arr.push_back(2);
    const auto& const_arr = arr;

    auto part = const_arr.slice(1, 1);

    EXPECT_EQ(part.size, 1);
    EXPECT_EQ(part[0], 2);
}

TEST(Allocator, alloc_array_returns_dynamic_array_view) {
    auto array = mem::alloc_array<u64>(3);
    defer(mem::free_array(array));

    ASSERT_NE(array.data, nullptr);
    ASSERT_EQ(array.size, 3u);
    ASSERT_EQ(ptr_addr(array.data) % alignof(u64), 0u);
}

TEST(Allocator, alloc_array_accepts_custom_allocator_without_alignment) {
    mem::Hosted_Allocator hosted{};

    auto array = mem::alloc_array<u64>(3, &hosted);
    defer(mem::free_array(array, &hosted));

    ASSERT_NE(array.data, nullptr);
    ASSERT_EQ(array.size, 3u);
}

TEST(Allocator, free_array_accepts_pointer) {
    auto array = mem::alloc_array<u64>(3);
    defer(mem::free_array<u64>(array.data));

    ASSERT_NE(array.data, nullptr);
    ASSERT_EQ(array.size, 3u);
}

TEST(Allocator, alloc_array_size_uses_type_alignment_by_default) {
    ASSERT_EQ(
        mem::alloc_array_size<u64>(3),
        3u * sizeof(u64) + sizeof(mem::Array_Allocation_Header) + alignof(u64) - 1u
    );
    ASSERT_EQ(mem::alloc_array_size<u64>(3, 1), mem::alloc_array_size<u64>(3));
}

#endif
