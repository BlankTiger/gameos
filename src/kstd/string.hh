#pragma once

#include <iterator>
#include <source_location>
#include <type_traits>
#include <utility>

#include "../basic.hh"
#include "../cstring.hh"
#include "array.hh"
#include "array_iterator.hh"
#include "assert.hh"
#include "string_view.hh"

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
