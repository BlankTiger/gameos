#pragma once

#include <source_location>

#include "basic.hh"
#include "assert.hh"
#include "cstring.hh"

// A non-owning view over existing char memory: a pointer plus a length.
// Never null-terminated by contract (use c_str() on a string if a
// null-terminated buffer is required for a legacy/C API).
struct string_view {
    const char* data = nullptr;
    usize size = 0;

    constexpr string_view() = default;
    constexpr string_view(const char* data, usize size) : data(data), size(size) {}

    // Construction from an Array_View<u8> lives on the Array_View side (see
    // array.hh's `operator string_view()`): array.hh has to include this header
    // to assert, so this header must not need to know about arrays.

    // Intentionally not `explicit`: this lets string_view/string APIs accept plain
    // string literals directly, matching how const char* is used elsewhere
    // in the codebase (e.g. term::print, fmt::print).
    constexpr string_view(const char* cstr) : data(cstr), size(cstr != nullptr ? kstd_strlen(cstr) : 0) {}

    auto operator[](usize index) const -> char {
        kstd_assert(index < size, "string_view index out of bounds");
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

    operator bool() const {
        return data != nullptr;
    }

    auto begin() const -> const char* { return data; }
    auto end() const -> const char* { return data + size; }
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

#endif
