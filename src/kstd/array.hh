#pragma once

#include <utility>

#include "array_iterator.hh"

template <typename T, usize N>
struct Static_Array {
    static constexpr auto size = N;
    T data[N];

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        assert(index < size, "index out of bounds", location);
        return data[index];
    }

    constexpr auto operator[](u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        assert(index < size, "index out of bounds", location);
        return data[index];
    }

    ARRAY_ITERATOR()
};

//
// Use as a normal Array. The exception is this can't grow, because it's backed by static memory.
//
template <typename T, usize N>
struct Bounded_Array {
    static constexpr auto MAX_SIZE = N;
    usize size = 0;
    T data[N];

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto push_back(T&& element) -> void {
        assert(size < MAX_SIZE);
        ::new (data + size) T(std::move(element));
        ++size;
    }

    auto push_back(const T& element) -> void {
        assert(size < MAX_SIZE);
        ::new (data + size) T(element);
        ++size;
    }

    ARRAY_ITERATOR()
};

template <typename T>
struct Array {
    usize allocated;
    usize size;
    T* data;

    Array()
        : allocated(1),
          size(0),
          data(new T[allocated]) {}

    explicit Array(usize initial_size)
        : allocated(initial_size),
          size(0),
          data(new T[allocated]) {}

    ~Array() {
        delete[] data;
    }

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto push_back(T&& element) -> void {
        assert(size < allocated, "Implement resizing.");
        ::new (data + size) T(std::move(element));
        ++size;
    }

    auto push_back(const T& element) -> void {
        assert(size < allocated, "Implement resizing.");
        ::new (data + size) T(element);
        ++size;
    }

    ARRAY_ITERATOR()
};
