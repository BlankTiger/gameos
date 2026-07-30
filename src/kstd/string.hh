#pragma once

#include <source_location>
#include <type_traits>

#include "basic.hh"
#include "cstring.hh"

//
// Forward declare instead of including array.hh.
// array.hh includes assert.hh, then serial.hh, format.hh, string.hh.
// Supply the default here, not in array.hh definition.
// Redefining the same default there would be illegal.
// Array_View<T, N> is a view of exactly N elements.
// Array_View<T> with N=DYNAMIC_EXTENT carries its own runtime size.
//
template <typename T, usize N = DYNAMIC_EXTENT>
struct Array_View;

struct string;

//
// Forward declare instead of including assert.hh.
// assert.hh includes serial.hh, format.hh, string.hh.
// That would create a cycle back to this header.
// Forward declare unconditionally, not only in the UNIT_TESTS-less branch.
// The first header that reaches this one in the cycle needs kstd_assert
// visible for operator[] before the other header include returns.
//
constexpr force_inline auto kstd_assert(bool predicate, const char* message, const std::source_location& location) -> void;

#ifdef UNIT_TESTS
#include "assert.hh"
#endif

//
// string: {data, size}, non-owning. Never frees in destructor.
// Heap bytes: free_string(s [, allocator]). Prefer defer(free_string(s)).
// Temp bytes: use before temporary_allocator.reset().
// Builder / sprint / tprint: string_builder.hh
//
struct string {
    char* data = nullptr;
    usize size = 0;

    constexpr string() = default;
    constexpr string(char* data, usize size) : data(data), size(size) {}
    constexpr string(const char* data, usize size)
        : data(const_cast<char*>(data)), size(size) {}

    // Construction from an Array_View<u8> lives on the Array_View side (see
    // array.hh's `operator string()`): array.hh has to include this header
    // to assert, so this header must not need to know about arrays.

    // Intentionally not `explicit`: accept string literals like const char*.
    constexpr string(const char* cstr)
        : data(const_cast<char*>(cstr)), size(cstr != nullptr ? kstd_strlen(cstr) : 0) {}

    auto operator [] (usize index) const -> char {
        kstd_assert(index < size, "string index out of bounds", std::source_location::current());
        return data[index];
    }

    auto operator == (const string& other) const -> bool {
        if (size != other.size) return false;
        for (usize i = 0; i < size; ++i) {
            if (data[i] != other.data[i]) return false;
        }
        return true;
    }

    auto operator != (const string& other) const -> bool {
        return !(*this == other);
    }

    auto operator == (const char* other) const -> bool {
        return *this == string(other);
    }

    auto operator != (const char* other) const -> bool {
        return !(*this == other);
    }

    auto format() const -> string {
        return *this;
    }

    operator bool() const {
        return data != nullptr && size > 0;
    }

    auto begin() const -> const char* { return data; }
    auto end() const -> const char* { return data + size; }
};

#ifdef UNIT_TESTS

TEST(string, default_is_empty) {
    string s;
    EXPECT_EQ(s.size, 0);
    EXPECT_EQ(s.data, nullptr);
}

TEST(string, can_be_made_from_a_cstring) {
    string s = "hello";
    EXPECT_EQ(s.size, 5);
    EXPECT_EQ(s[0], 'h');
    EXPECT_EQ(s[4], 'o');
}

TEST(string, can_be_made_from_pointer_and_length) {
    const char* data = "hello world";
    string s(data, 5);
    EXPECT_EQ(s.size, 5);
    EXPECT_EQ(s[4], 'o');
}

TEST(string, equality) {
    EXPECT_EQ(string("abc"), string("abc"));
    EXPECT_NE(string("abc"), string("abd"));
    EXPECT_NE(string("abc"), string("ab"));
}

TEST(string, equality_with_cstring) {
    EXPECT_EQ(string("abc"), "abc");
    EXPECT_NE(string("abc"), "abd");
    EXPECT_NE(string("abc"), "ab");
}

TEST(string, index_out_of_bounds_assert) {
    string s = "abc";
    EXPECT_DEATH(s[3], "");
}

TEST(string, iterator) {
    string s = "abc";
    char buf[4] = {};
    usize i = 0;
    for (char c : s) buf[i++] = c;
    EXPECT_STREQ(buf, "abc");
}

TEST(string, bool_is_content_presence) {
    EXPECT_FALSE(static_cast<bool>(string{}));
    EXPECT_FALSE(static_cast<bool>(string(static_cast<const char*>(nullptr))));
    EXPECT_FALSE(static_cast<bool>(string("x", 0)));
    EXPECT_TRUE(static_cast<bool>(string("x")));
}

#endif
