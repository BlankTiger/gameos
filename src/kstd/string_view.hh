#pragma once

#include <source_location>

#include "basic.hh"
#include "cstring.hh"

//
// Forward-declared instead of #include "array.hh": array.hh pulls in assert.hh
// -> serial.hh -> format.hh -> string_view.hh (for printing string_view)
//
template <typename T, usize N>
struct Static_Array;

//
// Forward-declared instead of #include "assert.hh": assert.hh pulls in
// serial.hh -> format.hh -> string_view.hh (for printing string_view), which
// would cycle right back here.
//
// Under UNIT_TESTS this header is the entry point on its own.
//
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

    template <usize N>
    explicit constexpr string_view(const Static_Array<u8, N>& bytes)
        : data(reinterpret_cast<const char*>(bytes.data)), size(N) {}

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
