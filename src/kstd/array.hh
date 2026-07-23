#pragma once

#include <new>
#include <type_traits>
#include <utility>
#include <source_location>
#include <iterator>

#include "../basic.hh"
#include "assert.hh"
#include "array_iterator.hh"

template <typename T, usize N>
struct Static_Array {
    static constexpr auto size = N;
    T data[N];

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    constexpr auto operator[](u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto elements()       -> T*       { return data; }
    auto elements() const -> const T* { return data; }

    ARRAY_ITERATOR()
};

//
// Use as a normal Array. The exception is this can't grow, because it's backed by static memory.
//
template <typename T, usize N>
struct Bounded_Array {
    static constexpr auto MAX_SIZE = N;
    usize size = 0;
    // Raw byte storage avoids default-constructing every slot up front.
    // Without this, push_back's placement-new would run a second constructor
    // over an already-live object - UB for any non-trivial T.
    alignas(T) u8 data[sizeof(T) * N];

    Bounded_Array() = default;

    ~Bounded_Array() {
        for (usize i = 0; i < size; ++i)
            slot(i)->~T();
    }

    // Bounded_Array owns its elements, so copies/moves need element-wise
    // construction.  Delete them until actually needed to avoid silent bugs.
    Bounded_Array(const Bounded_Array&) = delete;
    Bounded_Array& operator=(const Bounded_Array&) = delete;

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return *slot(index);
    }

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return *slot(index);
    }

    auto push_back(T&& element) -> void {
        kstd_assert(size < MAX_SIZE);
        ::new (slot(size)) T(std::move(element));
        ++size;
    }

    auto push_back(const T& element) -> void {
        kstd_assert(size < MAX_SIZE);
        ::new (slot(size)) T(element);
        ++size;
    }

    auto elements()       -> T*       { return slot(0); }
    auto elements() const -> const T* { return slot(0); }

    ARRAY_ITERATOR()

private:
    auto slot(usize i) -> T* {
        return reinterpret_cast<T*>(data + i * sizeof(T));
    }
    auto slot(usize i) const -> const T* {
        return reinterpret_cast<const T*>(data + i * sizeof(T));
    }
};

#ifdef UNIT_TESTS

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
    EXPECT_DEATH(arr.push_back(3), "");
}

#endif

template <typename T>
struct Array {
    usize capacity;
    usize size;
    T* data;

    Array()
        : capacity(1),
          size(0),
          data(static_cast<T*>(::operator new(sizeof(T) * capacity))) {}

    explicit Array(usize initial_size)
        : capacity(initial_size),
          size(0),
          data(static_cast<T*>(::operator new(sizeof(T) * capacity))) {}

    ~Array() {
        for (usize i = 0; i < size; ++i)
            data[i].~T();
        ::operator delete(data);
    }

    // Owning type: suppress implicit copy until properly implemented.
    Array(const Array&) = delete;

    Array(Array&& from) noexcept
        : capacity(from.capacity),
          size(from.size),
          data(from.data) {
        from.capacity = 0;
        from.size      = 0;
        from.data      = nullptr;
    }

    auto operator=(Array&& from) noexcept -> Array& {
        if (this == &from)
            return *this;

        for (usize i = 0; i < size; ++i)
            data[i].~T();
        ::operator delete(data);

        capacity = from.capacity;
        size      = from.size;
        data      = from.data;

        from.capacity = 0;
        from.size      = 0;
        from.data      = nullptr;

        return *this;
    }

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        kstd_assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto reserve(usize min_capacity) -> void {
        if (min_capacity <= capacity) return;

        usize new_capacity = capacity == 0 ? 16 : capacity;
        while (new_capacity < min_capacity) new_capacity *= 2;

        T* new_data = new T[new_capacity];
        if (data != nullptr) kstd_memcpy(new_data, data, size);
        delete[] data;
        data = new_data;
        capacity = new_capacity;
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

    // O(n) move of all elements back by one.
    auto pop_front() -> void {
        kstd_assert(size > 0, "pop_front on empty Array");
        data[0].~T();
        for (usize i = 1; i < size; ++i) {
            ::new (data + i - 1) T(std::move(data[i]));
            data[i].~T();
        }
        --size;
    }

    auto clear() {
        for (usize i = 0; i < size; ++i) {
            data[i].~T();
        }
        size = 0;
    }

    auto elements()       -> T*       { return data; }
    auto elements() const -> const T* { return data; }

    ARRAY_ITERATOR()
};
