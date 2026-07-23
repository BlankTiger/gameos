#pragma once

#include <iterator>
#include <source_location>
#include <type_traits>
#include <utility>

#include "../basic.hh"
#include "../cstring.hh"
#include "array_iterator.hh"

// Forward-declared instead of #include "assert.hh": assert.hh pulls in
// serial.hh -> format.hh -> str.hh (for printing string/string_view), which
// would cycle right back here. The real definition is visible wherever this
// header is actually used (kstd.hh includes assert.hh directly).
// Under UNIT_TESTS this header is the entry point on its own, so there's no
// larger aggregate to rely on: pull in the real definition directly instead.
#ifdef UNIT_TESTS
#include "assert.hh"
#else
constexpr force_inline auto kstd_assert(
    bool predicate,
    const char* message,
    const std::source_location& location
) -> void;
#endif

// A non-owning view over existing char memory: a pointer plus a length.
// Never null-terminated by contract (use c_str() on a string if a
// null-terminated buffer is required for a legacy/C API).
struct string_view {
    const char* data = nullptr;
    usize size = 0;

    constexpr string_view() = default;
    constexpr string_view(const char* data, usize size) : data(data), size(size) {}

    // Intentionally not `explicit`: this lets string_view/string APIs accept plain
    // string literals directly, matching how const char* is used elsewhere
    // in the codebase (e.g. term::print, fmt::print).
    constexpr string_view(const char* cstr) : data(cstr), size(cstr != nullptr ? kstd_strlen(cstr) : 0) {}

    auto operator[](usize index) const -> char {
        kstd_assert(index < size, "string_view index out of bounds", std::source_location::current());
        return data[index];
    }

    auto operator==(const string_view& other) const -> bool {
        if (size != other.size) return false;
        for (usize i = 0; i < size; ++i) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }

    auto operator!=(const string_view& other) const -> bool {
        return !(*this == other);
    }

    auto format() const -> string_view {
        return *this;
    }

    auto begin() const -> const char* { return data; }
    auto end() const -> const char* { return data + size; }
};

template <typename T = char>
struct string {
    static_assert(std::is_same_v<T, char>, "string only supports T = char for now");

    T*    data     = nullptr;
    usize size     = 0;
    usize capacity = 0;

    string() = default;

    explicit string(string_view view) {
        append(view);
    }

    string(const T* cstr) : string(string_view(cstr)) {}

    ~string() {
        delete[] data;
    }

    string(const string& other) {
        append(other.view());
    }

    auto operator=(const string& other) -> string& {
        if (this == &other) return *this;
        clear();
        append(other.view());
        return *this;
    }

    string(string&& other) noexcept
        : data(other.data), size(other.size), capacity(other.capacity) {
        other.data     = nullptr;
        other.size     = 0;
        other.capacity = 0;
    }

    auto operator=(string&& other) noexcept -> string& {
        if (this == &other) return *this;
        delete[] data;
        data     = other.data;
        size     = other.size;
        capacity = other.capacity;
        other.data     = nullptr;
        other.size     = 0;
        other.capacity = 0;
        return *this;
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

    auto push_back(T c) -> void {
        reserve(size + 1);
        data[size++] = c;
    }

    auto append(string_view view) -> void {
        reserve(size + view.size);
        kstd_memcpy(data + size, view.data, view.size);
        size += view.size;
    }

    auto operator+=(T c) -> string& {
        push_back(c);
        return *this;
    }

    auto operator+=(string_view view) -> string& {
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
        data[size] = '\0';
        return data;
    }

    auto view() const -> string_view {
        return string_view(data, size);
    }

    auto format() const -> string_view {
        return view();
    }

    auto operator[](usize index) const -> T {
        kstd_assert(index < size, "string index out of bounds", std::source_location::current());
        return data[index];
    }

    auto elements() -> T* { return data; }
    auto elements() const -> const T* { return data; }

    ARRAY_ITERATOR()
};

#ifdef UNIT_TESTS

TEST(string_view, default_is_empty) {
    string_view view;
    EXPECT_EQ(view.size, 0);
    EXPECT_EQ(view.data, nullptr);
}

TEST(string_view, can_be_made_from_a_cstring) {
    string_view view = "hello";
    EXPECT_EQ(view.size, 5);
    EXPECT_EQ(view[0], 'h');
    EXPECT_EQ(view[4], 'o');
}

TEST(string_view, can_be_made_from_pointer_and_length) {
    const char* data = "hello world";
    string_view view(data, 5);
    EXPECT_EQ(view.size, 5);
    EXPECT_EQ(view[4], 'o');
}

TEST(string_view, equality) {
    EXPECT_EQ(string_view("abc"), string_view("abc"));
    EXPECT_NE(string_view("abc"), string_view("abd"));
    EXPECT_NE(string_view("abc"), string_view("ab"));
}

TEST(string_view, index_out_of_bounds_assert) {
    string_view view = "abc";
    EXPECT_DEATH(view[3], "");
}

TEST(string_view, iterator) {
    string_view view = "abc";
    string collected;
    for (char c : view) collected += c;
    EXPECT_EQ(collected.view(), string_view("abc"));
}

TEST(string, default_is_empty) {
    string str;
    EXPECT_EQ(str.data,     nullptr);
    EXPECT_EQ(str.size,     0);
    EXPECT_EQ(str.capacity, 0);
}

TEST(string, can_be_made_from_string_view) {
    string str{string_view("hello")};
    EXPECT_EQ(str.view(), string_view("hello"));
}

TEST(string, can_be_made_from_cstring) {
    string str = "hello";
    EXPECT_EQ(str.view(), string_view("hello"));
}

TEST(string, append_grows_2x) {
    string str;
    EXPECT_EQ(str.capacity, 0);
    str.append("hello ");
    EXPECT_EQ(str.capacity, 16);
    str.append("world");
    EXPECT_EQ(str.capacity, 16);
    EXPECT_EQ(str.view(), string_view("hello world"));
}

TEST(string, push_back_on_single_on_single_chars) {
    string str;
    for (char c : {'a', 'b', 'c'}) str.push_back(c);
    EXPECT_EQ(str.view(), string_view("abc"));
}

TEST(string, operator_plus_means_append) {
    string str;
    str += 'a';
    str += string_view("bc");
    EXPECT_EQ(str.view(), string_view("abc"));
}

TEST(string, clear_resets_size_but_not_capacity) {
    string str = "hello";
    auto capacity_before = str.capacity;
    str.clear();
    EXPECT_EQ(str.size, 0);
    EXPECT_EQ(str.capacity, capacity_before);
}

TEST(string, string_to_cstring) {
    string str = "hi";
    const char* cstr = str.c_str();
    EXPECT_EQ(cstr[0], 'h');
    EXPECT_EQ(cstr[1], 'i');
    EXPECT_EQ(cstr[2], '\0');
    EXPECT_EQ(str.size, 2);
}

TEST(string, copy_constructor) {
    string original = "hello";
    string copy = original;
    copy += '!';
    EXPECT_EQ(original.view(), string_view("hello"));
    EXPECT_EQ(copy.view(),     string_view("hello!"));
}

TEST(string, move_constructor) {
    string original = "hello";
    string moved = std::move(original);
    EXPECT_EQ(moved.view(), string_view("hello"));
    EXPECT_EQ(original.size, 0);
}

TEST(string, index_out_of_bounds_assert) {
    string str = "abc";
    EXPECT_DEATH(str[3], "");
}

#endif
