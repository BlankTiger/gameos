#pragma once

#include <type_traits>
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
        assert(index < size, "index out of bounds", location);
        return *slot(index);
    }

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
        assert(index < size, "index out of bounds", location);
        return *slot(index);
    }

    auto push_back(T&& element) -> void {
        assert(size < MAX_SIZE);
        ::new (slot(size)) T(std::move(element));
        ++size;
    }

    auto push_back(const T& element) -> void {
        assert(size < MAX_SIZE);
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

template <typename T>
struct Array {
    usize allocated;
    usize size;
    T* data;

    Array()
        : allocated(1),
          size(0),
          data(static_cast<T*>(::operator new(sizeof(T) * allocated))) {}

    explicit Array(usize initial_size)
        : allocated(initial_size),
          size(0),
          data(static_cast<T*>(::operator new(sizeof(T) * allocated))) {}

    ~Array() {
        for (usize i = 0; i < size; ++i)
            data[i].~T();
        ::operator delete(data);
    }

    // Owning type: suppress implicit copy/move until properly implemented.
    Array(const Array&) = delete;
    Array& operator=(const Array&) = delete;

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) -> T& {
        assert(index < size, "index out of bounds", location);
        return data[index];
    }

    auto operator[](u64 index, const std::source_location& location = std::source_location::current()) const
        -> const T& {
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

    auto elements()       -> T*       { return data; }
    auto elements() const -> const T* { return data; }

    ARRAY_ITERATOR()
};
