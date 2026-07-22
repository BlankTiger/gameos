#pragma once

#include <iterator>
#include <source_location>
#include <type_traits>
#include <utility>

#include "../basic.hh"
#include "../cstring.hh"
#include "array_iterator.hh"

// Forward-declared instead of #include "assert.hh": assert.hh pulls in
// serial.hh -> format.hh -> str.hh (for printing String/String_View), which
// would cycle right back here. The real definition is visible wherever this
// header is actually used (kstd.hh includes assert.hh directly).
constexpr force_inline auto assert(
    bool predicate,
    const char* message,
    const std::source_location& location
) -> void;

// A non-owning view over existing char memory: a pointer plus a length.
// Never null-terminated by contract (use c_str() on a String if a
// null-terminated buffer is required for a legacy/C API).
struct String_View {
    const char* data = nullptr;
    usize size = 0;

    constexpr String_View() = default;
    constexpr String_View(const char* data, usize size) : data(data), size(size) {}

    // Intentionally not `explicit`: this lets String_View/String APIs accept plain
    // string literals directly, matching how const char* is used elsewhere
    // in the codebase (e.g. term::print, fmt::print).
    constexpr String_View(const char* cstr) : data(cstr), size(cstr != nullptr ? strlen(cstr) : 0) {}

    auto operator[](usize index) const -> char {
        assert(index < size, "String_View index out of bounds", std::source_location::current());
        return data[index];
    }

    auto operator==(const String_View& other) const -> bool {
        if (size != other.size) return false;
        for (usize i = 0; i < size; ++i) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }

    auto operator!=(const String_View& other) const -> bool {
        return !(*this == other);
    }

    auto format() const -> String_View {
        return *this;
    }

    auto begin() const -> const char* { return data; }
    auto end() const -> const char* { return data + size; }
};

template <typename T = char>
struct String {
    static_assert(std::is_same_v<T, char>, "String only supports T = char for now");

    T*    buffer   = nullptr;
    usize size     = 0;
    usize capacity = 0;

    String() = default;

    explicit String(String_View view) {
        append(view);
    }

    String(const T* cstr) : String(String_View(cstr)) {}

    ~String() {
        delete[] buffer;
    }

    String(const String& other) {
        append(other.view());
    }

    auto operator=(const String& other) -> String& {
        if (this == &other) return *this;
        clear();
        append(other.view());
        return *this;
    }

    String(String&& other) noexcept
        : buffer(other.buffer), size(other.size), capacity(other.capacity) {
        other.buffer   = nullptr;
        other.size     = 0;
        other.capacity = 0;
    }

    auto operator=(String&& other) noexcept -> String& {
        if (this == &other) return *this;
        delete[] buffer;
        buffer   = other.buffer;
        size     = other.size;
        capacity = other.capacity;
        other.buffer   = nullptr;
        other.size     = 0;
        other.capacity = 0;
        return *this;
    }

    auto reserve(usize min_capacity) -> void {
        if (min_capacity <= capacity) return;

        usize new_capacity = capacity == 0 ? 16 : capacity;
        while (new_capacity < min_capacity) new_capacity *= 2;

        T* new_buffer = new T[new_capacity];
        if (buffer != nullptr) memcpy(new_buffer, buffer, size);
        delete[] buffer;
        buffer = new_buffer;
        capacity = new_capacity;
    }

    auto push_back(T c) -> void {
        reserve(size + 1);
        buffer[size++] = c;
    }

    auto append(String_View view) -> void {
        reserve(size + view.size);
        memcpy(buffer + size, view.data, view.size);
        size += view.size;
    }

    auto operator+=(T c) -> String& {
        push_back(c);
        return *this;
    }

    auto operator+=(String_View view) -> String& {
        append(view);
        return *this;
    }

    auto clear() -> void {
        size = 0;
    }

    // Ensures a trailing '\0' without counting it towards size, and returns
    // the buffer for use with APIs expecting a null-terminated C string.
    auto c_str() -> const T* {
        reserve(size + 1);
        buffer[size] = '\0';
        return buffer;
    }

    auto view() const -> String_View {
        return String_View(buffer, size);
    }

    auto format() const -> String_View {
        return view();
    }

    auto operator[](usize index) const -> T {
        assert(index < size, "String index out of bounds", std::source_location::current());
        return buffer[index];
    }

    auto elements() -> T* { return buffer; }
    auto elements() const -> const T* { return buffer; }

    ARRAY_ITERATOR()
};
