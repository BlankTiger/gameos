#pragma once

#include <new>
#include <type_traits>
#include <utility>
#include <source_location>
#include <iterator>

#include "basic.hh"
#include "cstring.hh"
#include "assert.hh"
#include "string_view.hh"
#include "array_iterator.hh"

inline constexpr usize DYNAMIC_EXTENT = static_cast<usize>(-1);

template <typename T, usize N = DYNAMIC_EXTENT>
struct Array_View {
    static constexpr auto size = N;
    static constexpr auto size_in_bytes = sizeof(T) * N;
    T* data;

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

    // Byte views can be reinterpreted as text. Defined on this side (rather
    // than as a string_view constructor) so string_view.hh never has to know
    // about arrays: it is included by this header for its own asserts.
    explicit operator string_view() const requires std::is_same_v<std::remove_const_t<T>, u8> {
        return string_view(reinterpret_cast<const char*>(data), N);
    }

    ARRAY_ITERATOR()
};

template <typename T>
struct Array_View<T, DYNAMIC_EXTENT> {
    usize size;
    T* data;

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

    explicit operator string_view() const requires std::is_same_v<std::remove_const_t<T>, u8> {
        return string_view(reinterpret_cast<const char*>(data), size);
    }

    ARRAY_ITERATOR()
};

template <typename T, usize N>
struct Static_Array {
    static constexpr auto size = N;
    static constexpr auto size_in_bytes = sizeof(T) * N;
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

    operator Array_View<T, N>() { return Array_View<T, N>{data}; }
    operator Array_View<T>()    { return Array_View<T>{size, data}; }

    ARRAY_ITERATOR()
};

#ifdef UNIT_TESTS

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

TEST(Static_Array, out_of_bounds_asserts) {
    Static_Array<int, 2> arr{{1, 2}};

    EXPECT_DEATH(arr[2], "");
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
    // Without this, push_back's placement-new would run a second constructor
    // over an already-live object - UB for any non-trivial T.
    alignas(T) u8 data[sizeof(T) * N];

    Bounded_Array() = default;

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

    auto operator=(const Bounded_Array& from) -> Bounded_Array& {
        if (this == &from)
            return *this;

        for (usize i = 0; i < size; ++i)
            slot(i)->~T();

        size = from.size;
        for (usize i = 0; i < size; ++i)
            ::new (slot(i)) T(*from.slot(i));

        return *this;
    }

    auto operator=(Bounded_Array&& from) noexcept -> Bounded_Array& {
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

    explicit Array(usize initial_size, const T& initial_value)
        : capacity(initial_size),
          size(0),
          data(static_cast<T*>(::operator new(sizeof(T) * capacity))) {
        for (usize index = 0; index < initial_size; ++index) {
            ::new (static_cast<void*>(data + index)) T(initial_value);
            ++size;
        }
    }

    ~Array() {
        for (usize i = 0; i < size; ++i)
            data[i].~T();
        ::operator delete(data);
    }

    Array(const Array& from)
        : capacity(from.capacity),
          size(from.size),
          data(static_cast<T*>(::operator new(sizeof(T) * capacity))) {
        for (usize i = 0; i < size; ++i)
            ::new (data + i) T(from.data[i]);
    }

    auto operator=(const Array& from) -> Array& {
        if (this == &from) return *this;
        for (usize i = 0; i < size; ++i)
            data[i].~T();
        // @TODO: Consider not deleting this if it's big enough. Also look at
        //        other constructors and do the same.
        ::operator delete(data);

        capacity = from.capacity;
        size     = from.size;
        data     = static_cast<T*>(::operator new(sizeof(T) * from.capacity));

        for (usize i = 0; i < size; ++i)
            ::new (data + i) T(from.data[i]);

        return *this;
    }

    Array(Array&& from) noexcept
        : capacity(from.capacity),
          size(from.size),
          data(from.data) {
        from.capacity = 0;
        from.size     = 0;
        from.data     = nullptr;
    }

    auto operator=(Array&& from) noexcept -> Array& {
        if (this == &from)
            return *this;

        for (usize i = 0; i < size; ++i)
            data[i].~T();
        ::operator delete(data);

        capacity = from.capacity;
        size     = from.size;
        data     = from.data;

        from.capacity = 0;
        from.size     = 0;
        from.data     = nullptr;

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

        T* new_data = static_cast<T*>(::operator new(sizeof(T) * new_capacity));
        if constexpr (std::is_trivially_copyable_v<T>) {
            kstd_memcpy(new_data, data, sizeof(T) * size);
        } else {
            for (usize i = 0; i < size; ++i) {
                ::new (new_data + i) T(std::move(data[i]));
                data[i].~T();
            }
        }
        ::operator delete(data);

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

    operator Array_View<T>() { return Array_View<T>{size, data}; }

    ARRAY_ITERATOR()
};


#ifdef UNIT_TESTS

TEST(Array, default_is_empty) {
    Array<int> arr;

    EXPECT_EQ(arr.size, 0);
    EXPECT_EQ(arr.capacity, 1);
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

TEST(Array, reserve_increases_capacity) {
    Array<int> arr;

    auto old_capacity = arr.capacity;

    arr.reserve(100);

    EXPECT_TRUE(arr.capacity >= 100);
    EXPECT_TRUE(arr.capacity > old_capacity);
}

TEST(Array, pop_front_removes_first_element) {
    Array<int> arr;

    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    arr.pop_front();

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

    EXPECT_DEATH(arr[1], "");
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

#endif
