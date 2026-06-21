#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <source_location>
#include <utility>

#include "int.hh"

namespace kstd {

usize strlen(const char* str) {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

#include "terminal.hh"

void assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()) {
    if (!predicate) {
        term::terminal_writestring(location.file_name());
        term::terminal_writestring(":");
        term::terminal_writeint(location.line());
        term::terminal_writestring(":");
        term::terminal_writeint(location.column());
        term::terminal_writestring(":");
        term::terminal_writestring(" assertion failed");
        if (message) {
            term::terminal_writestring(": ");
            term::terminal_writestring(message);
        }
        term::terminal_writestring("\n");

        for (;;) asm volatile("hlt");
    }
}

inline void debug_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()) {
#ifdef NDEBUG
    return;
#else
    assert(predicate, message, location);
#endif
}

[[noreturn]] static void halt_forever(
    const char* message,
    const std::source_location& location = std::source_location::current()) {
    if (message != nullptr) {
        term::terminal_writestring(message);
        term::terminal_writestring("\n");
    }

    term::terminal_writestring(location.file_name());
    term::terminal_writestring(":");
    term::terminal_writeint(location.line());
    term::terminal_writestring(":");
    term::terminal_writeint(location.column());
    term::terminal_writestring("\n");

    for (;;) {
        asm volatile("hlt");
    }
}

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

    struct iterator {
        T* ptr;
        iterator(T* p)
            : ptr(p) {}
        T& operator*() {
            return *ptr;
        }
        iterator& operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }
    };

    struct const_iterator {
        const T* ptr;
        explicit const_iterator(const T* p)
            : ptr(p) {}
        const T& operator*() const {
            return *ptr;
        }
        const_iterator& operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const const_iterator& other) const {
            return ptr != other.ptr;
        }

        // Implicit conversion from iterator to const_iterator
        explicit const_iterator(iterator it)
            : ptr(it.ptr) {}
    };

    iterator begin() {
        return iterator(data);
    }
    iterator end() {
        return iterator(data + size);
    }

    const_iterator cbegin() const noexcept {
        return const_iterator(data);
    }
    const_iterator cend() const noexcept {
        return const_iterator(data + size);
    }
};

template <typename T, usize N>
struct Static_Array {
    static constexpr auto size = N;
    T data[N];

    T& operator[](u64 index, const std::source_location& location = std::source_location::current()) {
        assert(index < size, "index out of bounds", location);
        return data[index];
    }

    struct iterator {
        T* ptr;
        iterator(T* p)
            : ptr(p) {}
        T& operator*() {
            return *ptr;
        }
        iterator& operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }
    };

    struct const_iterator {
        const T* ptr;
        explicit const_iterator(const T* p)
            : ptr(p) {}
        const T& operator*() const {
            return *ptr;
        }
        const_iterator& operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const const_iterator& other) const {
            return ptr != other.ptr;
        }

        // Implicit conversion from iterator to const_iterator
        explicit const_iterator(iterator it)
            : ptr(it.ptr) {}
    };

    iterator begin() {
        return iterator(data);
    }
    iterator end() {
        return iterator(data + size);
    }

    const_iterator cbegin() const noexcept {
        return const_iterator(data);
    }
    const_iterator cend() const noexcept {
        return const_iterator(data + size);
    }
};

template <typename T>
struct Array {
    usize size;
    T* data;
    usize allocated;

    Array()
        : allocated(1),
          data(new T[allocated]),
          size(0) {}

    explicit Array(usize initial_size)
        : allocated(initial_size),
          data(new T[allocated]),
          size(0) {}

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

    struct iterator {
        T* ptr;
        iterator(T* p)
            : ptr(p) {}
        T& operator*() {
            return *ptr;
        }
        iterator& operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const iterator& other) const {
            return ptr != other.ptr;
        }
    };

    struct const_iterator {
        const T* ptr;
        explicit const_iterator(const T* p)
            : ptr(p) {}
        const T& operator*() const {
            return *ptr;
        }
        const_iterator& operator++() {
            ++ptr;
            return *this;
        }
        bool operator!=(const const_iterator& other) const {
            return ptr != other.ptr;
        }

        // Implicit conversion from iterator to const_iterator
        explicit const_iterator(iterator it)
            : ptr(it.ptr) {}
    };

    iterator begin() {
        return iterator(data);
    }
    iterator end() {
        return iterator(data + size);
    }

    const_iterator cbegin() const noexcept {
        return const_iterator(data);
    }
    const_iterator cend() const noexcept {
        return const_iterator(data + size);
    }
};

#include "memory.hh"

}  // namespace kstd
