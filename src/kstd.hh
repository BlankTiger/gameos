#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <source_location>
#include <type_traits>
#include <utility>

#if defined(__GNUC__) || defined(__clang__)
#define force_inline inline __attribute__((always_inline))
#else
#define force_inline inline
#endif

#include "cstring.hh"
#include "int.hh"

#include "vga.hh"
namespace term = vga::term;

namespace kstd {



constexpr force_inline auto assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()) -> void {
    if (!predicate) {
        if (message) {
            term::println(
                "%:%:%: assertion failed: %", location.file_name(), location.line(), location.column(), message);
        } else {
            term::println("%:%:%: assertion failed", location.file_name(), location.line(), location.column());
        }

        for (;;) asm volatile("hlt");
    }
}

constexpr force_inline auto debug_assert(
    bool predicate,
    const char* message = nullptr,
    const std::source_location& location = std::source_location::current()) -> void {
#ifdef NDEBUG
    return;
#else
    assert(predicate, message, location);
#endif
}

[[noreturn]] static auto halt_forever(
    const char* message,
    const std::source_location& location = std::source_location::current()) -> void {
    if (message != nullptr) {
        term::println("%", message);
    }

    term::println("%:%:%", location.file_name(), location.line(), location.column());

    for (;;) {
        asm volatile("hlt");
    }
}

template <typename Enum, typename... Flags>
    requires(std::is_enum_v<Enum> && (std::is_same_v<Enum, Flags> && ...))
force_inline auto has_flag(Enum value, Flags... flags) -> bool {
    if constexpr (sizeof...(Flags) == 0) {
        return false;
    }

    using Underlying = std::underlying_type_t<Enum>;
    const Underlying mask = (static_cast<Underlying>(flags) | ...);
    return (static_cast<Underlying>(value) & mask) == mask;
}

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

    // @TODO: move iterator definitions for array types into a separate file
    //        cause they are identical.
    struct iterator {
        T* ptr;
        iterator(T* p)
            : ptr(p) {}
        auto operator*() -> T& {
            return *ptr;
        }
        auto operator++() -> iterator& {
            ++ptr;
            return *this;
        }
        auto operator!=(const iterator& other) const -> bool {
            return ptr != other.ptr;
        }
    };

    struct const_iterator {
        const T* ptr;
        explicit const_iterator(const T* p)
            : ptr(p) {}
        auto operator*() const -> const T& {
            return *ptr;
        }
        auto operator++() -> const_iterator& {
            ++ptr;
            return *this;
        }
        auto operator!=(const const_iterator& other) const -> bool {
            return ptr != other.ptr;
        }

        // Implicit conversion from iterator to const_iterator
        explicit const_iterator(iterator it)
            : ptr(it.ptr) {}
    };

    auto begin() -> iterator {
        return iterator(data);
    }
    auto end() -> iterator {
        return iterator(data + size);
    }

    auto begin() const noexcept -> const_iterator {
        return const_iterator(data);
    }
    auto end() const noexcept -> const_iterator {
        return const_iterator(data + size);
    }

    auto cbegin() const noexcept -> const_iterator {
        return const_iterator(data);
    }
    auto cend() const noexcept -> const_iterator {
        return const_iterator(data + size);
    }
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

    struct iterator {
        T* ptr;
        iterator(T* p)
            : ptr(p) {}
        auto operator*() -> T& {
            return *ptr;
        }
        auto operator++() -> iterator& {
            ++ptr;
            return *this;
        }
        auto operator!=(const iterator& other) const -> bool {
            return ptr != other.ptr;
        }
    };

    struct const_iterator {
        const T* ptr;
        explicit const_iterator(const T* p)
            : ptr(p) {}
        auto operator*() const -> const T& {
            return *ptr;
        }
        auto operator++() -> const_iterator& {
            ++ptr;
            return *this;
        }
        auto operator!=(const const_iterator& other) const -> bool {
            return ptr != other.ptr;
        }

        // Implicit conversion from iterator to const_iterator
        explicit const_iterator(iterator it)
            : ptr(it.ptr) {}
    };

    auto begin() -> iterator {
        return iterator(data);
    }
    auto end() -> iterator {
        return iterator(data + size);
    }

    auto cbegin() const noexcept -> const_iterator {
        return const_iterator(data);
    }
    auto cend() const noexcept -> const_iterator {
        return const_iterator(data + size);
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

    struct iterator {
        T* ptr;
        iterator(T* p)
            : ptr(p) {}
        auto operator*() -> T& {
            return *ptr;
        }
        auto operator++() -> iterator& {
            ++ptr;
            return *this;
        }
        auto operator!=(const iterator& other) const -> bool {
            return ptr != other.ptr;
        }
    };

    struct const_iterator {
        const T* ptr;
        explicit const_iterator(const T* p)
            : ptr(p) {}
        auto operator*() const -> const T& {
            return *ptr;
        }
        auto operator++() -> const_iterator& {
            ++ptr;
            return *this;
        }
        auto operator!=(const const_iterator& other) const -> bool {
            return ptr != other.ptr;
        }

        // Implicit conversion from iterator to const_iterator
        explicit const_iterator(iterator it)
            : ptr(it.ptr) {}
    };

    auto begin() -> iterator {
        return iterator(data);
    }
    auto end() -> iterator {
        return iterator(data + size);
    }

    auto cbegin() const noexcept -> const_iterator {
        return const_iterator(data);
    }
    auto cend() const noexcept -> const_iterator {
        return const_iterator(data + size);
    }
};

#include "memory.hh"

}  // namespace kstd

#include "operator_new.hh"
