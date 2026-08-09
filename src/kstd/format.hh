#pragma once

#include <source_location>
#include <tuple>
#include <type_traits>
#include <utility>

#include "basic.hh"
#include "cstring.hh"
#include "enum_name.hh"
#include "string.hh"
#include "assert.hh"

//
// Backend-agnostic formatting utilities.
//
// Backend must provide:
//   auto put_char(char c) -> void;
//   auto new_line() -> void;
//
// Optional (used for bulk literal runs when present):
//   auto append(const char* bytes, usize length) -> void;
//
// put_char/new_line may be static (stateless backends, e.g. serial::Backend)
// or regular instance methods backed by member data (stateful backends,
// e.g. String_Builder). Either way an instance is passed by
// reference into fmt::print/fmt::println; for stateless backends that's just
// a throwaway default-constructed value at the call site.
//
// Format rules:
//   % / %0     next arg (implicit index)
//   %1, %2, ...  1-based arg index; sets next implicit to N+1
//              leading zeros ok (%01 == %1); out-of-range asserts
//   %00        empty insert (%000... too)
//   %%         literal %  (also char 31)
//
// For a custom type to be easily displayed by anything implementing fmt
// implement a `format` method on it. Return type must convert to string.
// Returned string must be owned (sprint / copy_string / to_string);
// print_value free_string's it after writing. Allocator must match free
// (current global if you pass null to free_string / to_string).
//
// struct A {
//     int value;
//
//     auto format() const -> string {
//         return sprint("A(%)", value);
//     }
// }
//
namespace fmt {

template <typename Backend>
static auto write_string(Backend& backend, const char* s) -> int {
    int written = 0;
    for (; *s; ++s) {
        backend.put_char(*s);
        ++written;
    }
    return written;
}

template <typename Backend>
static auto write_unsigned(Backend& backend, u64 value) -> int {
    if (value == 0) {
        backend.put_char('0');
        return 1;
    }
    char buf[20];
    int len = 0;
    while (value > 0) {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (int i = len - 1; i >= 0; --i) {
        backend.put_char(buf[i]);
    }
    return len;
}

template <typename Backend>
static auto write_signed(Backend& backend, s64 value) -> int {
    int written = 0;
    if (value < 0) {
        backend.put_char('-');
        ++written;
        // Handle minimum value overflow.
        if (value == -9223372036854775807LL - 1) {
            written += write_unsigned(backend, (u64)9223372036854775808ULL);
            return written;
        }
        value = -value;
    }
    written += write_unsigned(backend, (u64)value);
    return written;
}

template <typename Backend>
static auto write_hex(Backend& backend, u64 value) -> int {
    if (value == 0) {
        backend.put_char('0');
        return 1;
    }
    char buf[16];
    int len = 0;
    while (value > 0) {
        u8 nibble = (u8)(value & 0xF);
        buf[len++] = nibble < 10 ? (char)('0' + nibble) : (char)('a' + nibble - 10);
        value >>= 4;
    }
    for (int i = len - 1; i >= 0; --i) {
        backend.put_char(buf[i]);
    }
    return len;
}

template <typename Backend>
static auto write_pointer(Backend& backend, const void* value) -> int {
    int written = write_string(backend, "0x");
    written += write_hex(backend, (u64)(usize)value);
    return written;
}

template <typename Backend>
static auto write_float(Backend& backend, f64 value) -> int {
    int written = 0;
    if (value < 0) {
        backend.put_char('-');
        ++written;
        value = -value;
    }
    u64 whole = (u64)value;
    f64 fractional = value - (f64)whole;
    u64 scaled = (u64)(fractional * 1000000.0 + 0.5);
    if (scaled == 1000000) {
        scaled = 0;
        ++whole;
    }
    written += write_unsigned(backend, whole);
    backend.put_char('.');
    ++written;
    char buf[6];
    for (usize i = 0; i < 6; ++i) {
        buf[5 - i] = (char)('0' + (scaled % 10));
        scaled /= 10;
    }
    for (usize i = 0; i < 6; ++i) {
        backend.put_char(buf[i]);
    }
    written += 6;
    return written;
}

// -- print_value overloads --

template <typename Backend>
static force_inline auto print_value(Backend& backend, bool value) -> int {
    return value ? write_string(backend, "true") : write_string(backend, "false");
}

template <typename Backend>
static force_inline auto print_value(Backend& backend, char value) -> int {
    backend.put_char(value);
    return 1;
}

template <typename Backend>
static force_inline auto print_value(Backend& backend, const char* value) -> int {
    if (value == nullptr) value = "(null)";
    return write_string(backend, value);
}

template <typename Backend>
static force_inline auto print_value(Backend& backend, std::nullptr_t) -> int {
    return write_string(backend, "(null)");
}

template <typename Backend>
static force_inline auto print_value(Backend& backend, s64 value) -> int {
    return write_signed(backend, value);
}

template <typename Backend>
static force_inline auto print_value(Backend& backend, u64 value) -> int {
    return write_unsigned(backend, value);
}

template <typename Backend>
static force_inline auto print_value(Backend& backend, f64 value) -> int {
    return write_float(backend, value);
}

template <typename Backend>
static force_inline auto print_value(Backend& backend, const void* value) -> int {
    return write_pointer(backend, value);
}

template <typename Backend>
static force_inline auto print_string(Backend& backend, const string s) -> int {
    for (auto c : s) {
        backend.put_char(c);
    }
    return s.size;
}

template <typename Backend, typename T>
static force_inline auto print_value(Backend& backend, T&& value) -> int {
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, string>) {
        return print_string(backend, value);
    } else if constexpr (requires { value.format(); }) {
        string formatted = value.format();
        defer(free_string(formatted));
        int written = print_string(backend, formatted);
        return written;
    } else if constexpr (std::is_same_v<U, bool>) {
        return print_value(backend, (bool)value);
    } else if constexpr (std::is_same_v<U, char>) {
        return print_value(backend, (char)value);
    } else if constexpr (std::is_null_pointer_v<U>) {
        return print_value(backend, nullptr);
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            return print_value(backend, (s64)value);
        } else {
            return print_value(backend, (u64)value);
        }
    } else if constexpr (std::is_enum_v<U>) {
        string name = enum_to_string(value);
        if (name.size > 0) {
            return print_string(backend, name);
        }
        using Underlying = std::underlying_type_t<U>;
        return print_value(backend, (Underlying)value);
    } else if constexpr (std::is_floating_point_v<U>) {
        return print_value(backend, (f64)value);
    } else if constexpr (std::is_pointer_v<U>) {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
            return print_value(backend, (const char*)value);
        } else {
            return print_value(backend, (const void*)value);
        }
    } else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_extent_t<U>, char>) {
        return print_value(backend, (const char*)value);
    } else if constexpr (requires { value.c_str(); }) {
        return print_value(backend, value.c_str());
    } else if constexpr (requires { value.elements(); value.size; }) {
        // Prefer elements() over raw `data`: Bounded_Array stores elements in a
        // u8 buffer, so walking `data` would print bytes, not typed elements.
        if constexpr (std::is_convertible_v<decltype(value.elements()), const char*>) {
            const char* data_ptr = value.elements();
            usize sz = (usize)value.size;
            for (usize i = 0; i < sz; ++i) { backend.put_char(data_ptr[i]); }
            return (int)sz;
        } else {
            backend.put_char('[');
            int written = 1;
            auto* data_ptr = value.elements();
            usize sz = (usize)value.size;
            for (usize i = 0; i < sz; ++i) {
                if (i > 0) {
                    backend.put_char(',');
                    backend.put_char(' ');
                    written += 2;
                }
                written += print_value(backend, data_ptr[i]);
            }
            backend.put_char(']');
            return written + 1;
        }
    } else if constexpr (requires { value.data; value.size; } && !requires { value.data(); }) {
        if constexpr (std::is_convertible_v<decltype(value.data), const char*>) {
            const char* data_ptr = value.data;
            usize sz = (usize)value.size;
            for (usize i = 0; i < sz; ++i) { backend.put_char(data_ptr[i]); }
            return (int)sz;
        } else {
            backend.put_char('[');
            int written = 1;
            auto* data_ptr = value.data;
            usize sz = (usize)value.size;
            for (usize i = 0; i < sz; ++i) {
                if (i > 0) {
                    backend.put_char(',');
                    backend.put_char(' ');
                    written += 2;
                }
                written += print_value(backend, data_ptr[i]);
            }
            backend.put_char(']');
            return written + 1;
        }
    } else if constexpr (requires { value.data(); value.size; }) {
        if constexpr (std::is_convertible_v<decltype(value.data()), const char*>) {
            const char* data = value.data();
            usize size = (usize)value.size;
            for (usize i = 0; i < size; ++i) {
                backend.put_char(data[i]);
            }
            return (int)size;
        } else {
            return print_value(backend, (const void*)&value);
        }
    } else if constexpr (requires { value.data(); value.size(); }) {
        if constexpr (std::is_convertible_v<decltype(value.data()), const char*>) {
            const char* data = value.data();
            usize size = (usize)value.size();
            for (usize i = 0; i < size; ++i) {
                backend.put_char(data[i]);
            }
            return (int)size;
        } else {
            return print_value(backend, (const void*)&value);
        }
    } else {
        return print_value(backend, (const void*)&value);
    }
}

// -- format string parsing --

template <typename Backend>
force_inline auto write_literal(Backend& backend, const char* data, usize length) -> int {
    if (length == 0) return 0;
    if constexpr (requires { backend.append(data, length); }) {
        backend.append(data, length);
    } else {
        for (usize i = 0; i < length; ++i)
            backend.put_char(data[i]);
    }
    return static_cast<int>(length);
}

force_inline auto is_digit(char c) -> bool {
    return c >= '0' && c <= '9';
}

template <usize I = 0, typename Backend, typename Tuple>
force_inline auto print_arg_at(Backend& backend, Tuple& args, usize index) -> int {
    if constexpr (I < std::tuple_size_v<Tuple>) {
        if (I == index)
            return print_value(backend, std::get<I>(args));
        return print_arg_at<I + 1>(backend, args, index);
    }
    return 0;
}

template <typename Backend, typename Tuple>
auto print_impl(Backend& backend, string format, Tuple& args) -> int {
    constexpr usize arg_count = std::tuple_size_v<Tuple>;

    usize implicit_index_cursor = 0;
    usize cursor                = 0;
    usize printed               = 0;
    int   written               = 0;

    while (cursor < format.size) {
        char c = format.data[cursor];

        if (c != '%') {
            // Byte 31 in the format string becomes a literal '%'.
            if (static_cast<u8>(c) == 31) {
                written += write_literal(backend, format.data + printed, cursor - printed);
                backend.put_char('%');
                written += 1;
                cursor  += 1;
                printed  = cursor;
                continue;
            }
            cursor += 1;
            continue;
        }

        written += write_literal(backend, format.data + printed, cursor - printed);
        cursor += 1;  // skip '%'

        usize value = implicit_index_cursor;

        if (cursor < format.size) {
            char next = format.data[cursor];
            if (next == '%') {
                // %% -> literal %
                backend.put_char('%');
                written += 1;
                cursor  += 1;
                printed  = cursor;
                continue;
            }
            if (is_digit(next)) {
                usize start = cursor;
                usize sum   = 0;
                while (cursor < format.size && is_digit(format.data[cursor])) {
                    sum = sum * 10 + static_cast<usize>(format.data[cursor] - '0');
                    cursor += 1;
                }
                usize digit_count = cursor - start;
                if (sum == 0) {
                    if (digit_count >= 2) {
                        // %00, %000, ... -> empty insert
                        printed = cursor;
                        continue;
                    }
                    // %0 -> same as bare %
                } else {
                    value = sum - 1;  // 1-based -> 0-based
                }
            }
        }

        if (value >= arg_count) {
            kstd_assert(false, "format arg index out of range", std::source_location::current());
            printed = cursor;
            continue;
        }

        written += print_arg_at(backend, args, value);
        implicit_index_cursor = value + 1;
        printed               = cursor;
    }

    written += write_literal(backend, format.data + printed, cursor - printed);
    return written;
}

template <typename Backend, typename... Args>
auto print(Backend& backend, string format, Args&&... args) -> int {
    auto tuple = std::forward_as_tuple(std::forward<Args>(args)...);
    return print_impl(backend, format, tuple);
}

template <typename Backend, typename... Args>
auto print(Backend& backend, const char* format, Args&&... args) -> int {
    return print(backend, string(format), std::forward<Args>(args)...);
}

// Single value, no format string.
template <typename Backend, typename T>
auto print(Backend& backend, T&& value) -> int
    requires(
        !std::is_same_v<std::remove_cvref_t<T>, string> &&
        !std::is_same_v<std::remove_cvref_t<T>, char*> &&
        !std::is_same_v<std::remove_cvref_t<T>, const char*> &&
        !(std::is_array_v<std::remove_cvref_t<T>> &&
          std::is_same_v<std::remove_extent_t<std::remove_cvref_t<T>>, char>)
    )
{
    return print_value(backend, std::forward<T>(value));
}

template <typename Backend>
auto println(Backend& backend) -> int {
    backend.new_line();
    return 1;
}

template <typename Backend, typename... Args>
auto println(Backend& backend, string format, Args&&... args) -> int {
    int written = print(backend, format, std::forward<Args>(args)...);
    backend.new_line();
    return written + 1;
}

template <typename Backend, typename... Args>
auto println(Backend& backend, const char* format, Args&&... args) -> int {
    return println(backend, string(format), std::forward<Args>(args)...);
}

template <typename Backend, typename T>
auto println(Backend& backend, T&& value) -> int
    requires(
        !std::is_same_v<std::remove_cvref_t<T>, string> &&
        !std::is_same_v<std::remove_cvref_t<T>, char*> &&
        !std::is_same_v<std::remove_cvref_t<T>, const char*> &&
        !(std::is_array_v<std::remove_cvref_t<T>> &&
          std::is_same_v<std::remove_extent_t<std::remove_cvref_t<T>>, char>)
    )
{
    int written = print_value(backend, std::forward<T>(value));
    backend.new_line();
    return written + 1;
}

}  // namespace fmt

#ifdef UNIT_TESTS_KSTD_FORMAT

#include "array.hh"

namespace fmt_test {

struct Capture_Backend {
    char  buffer[256] = {};
    usize length      = 0;

    auto put_char(char c) -> void {
        if (length + 1 < sizeof(buffer)) buffer[length++] = c;
    }

    auto new_line() -> void { put_char('\n'); }
};

} // namespace fmt_test

// string exposes `data` as a member variable, which used to make it fall
// through every `value.data()` branch of print_value and get printed as a
// pointer.
TEST(fmt, prints_string_contents_not_its_address) {
    fmt_test::Capture_Backend backend;

    fmt::print(backend, "%", string("hello world", 5));

    EXPECT_STREQ(backend.buffer, "hello");
}

TEST(fmt, prints_string_lvalue) {
    fmt_test::Capture_Backend backend;
    const string value = "abc";

    fmt::print(backend, "[%]", value);

    EXPECT_STREQ(backend.buffer, "[abc]");
}

TEST(fmt, sequential_percent) {
    fmt_test::Capture_Backend backend;
    fmt::print(backend, "%, %!", "hello", "world");
    EXPECT_STREQ(backend.buffer, "hello, world!");
}

TEST(fmt, numbered_args_reorder) {
    fmt_test::Capture_Backend backend;
    fmt::print(backend, "%2 then %1", "first", "second");
    EXPECT_STREQ(backend.buffer, "second then first");
}

TEST(fmt, numbered_then_implicit) {
    fmt_test::Capture_Backend backend;
    // %2 consumes arg1; next bare % uses implicit = 2 -> arg index 2 (third)
    fmt::print(backend, "%2-%", "a", "b", "c");
    EXPECT_STREQ(backend.buffer, "b-c");
}

TEST(fmt, percent_escape_and_empty) {
    fmt_test::Capture_Backend backend;
    fmt::print(backend, "%% %00 done", 1);
    EXPECT_STREQ(backend.buffer, "%  done");
}

TEST(fmt, leading_zero_numbered_arg) {
    fmt_test::Capture_Backend backend;
    fmt::print(backend, "%01-%02", "a", "b");
    EXPECT_STREQ(backend.buffer, "a-b");
}

TEST(fmt, out_of_range_index_asserts) {
    fmt_test::Capture_Backend backend;
    EXPECT_DEATH(fmt::print(backend, "%5 %", "a"), "format arg index out of range");
}

TEST(fmt, char_31_is_literal_percent) {
    fmt_test::Capture_Backend backend;
    char format[] = {'x', static_cast<char>(31), 'y', '\0'};
    fmt::print(backend, format);
    EXPECT_STREQ(backend.buffer, "x%y");
}

TEST(fmt, single_value) {
    fmt_test::Capture_Backend backend;
    fmt::print(backend, 42);
    EXPECT_STREQ(backend.buffer, "42");
}

namespace fmt_test {

struct Owned_Format {
    auto format() const -> string {
        constexpr usize n = 5;
        // Can't include string_builder.hh here..
        auto* data = static_cast<char*>(mem::resolve_allocator(nullptr)->alloc(n, alignof(char)));
        data[0] = 'o';
        data[1] = 'w';
        data[2] = 'n';
        data[3] = 'e';
        data[4] = 'd';
        return string(data, n);
    }
};

}

TEST(fmt, frees_string_from_user_format) {
    mem::Hosted_Allocator hosted{};
    mem::Debug_Allocator  debug{&hosted};
    PUSH_ALLOCATOR(&debug);

    fmt_test::Capture_Backend backend;
    fmt::print(backend, "%", fmt_test::Owned_Format{});
    EXPECT_STREQ(backend.buffer, "owned");
    // Debug_Allocator destructor asserts if format() result leaked.
}

TEST(fmt, prints_static_array) {
    fmt_test::Capture_Backend backend;
    Static_Array<int, 3>      arr{{10, 20, 30}};

    fmt::print(backend, arr);

    EXPECT_STREQ(backend.buffer, "[10, 20, 30]");
}

TEST(fmt, prints_sized_array_view) {
    fmt_test::Capture_Backend backend;
    Static_Array<int, 3>      src{{4, 5, 6}};
    Array_View<int, 3>        view = src;

    fmt::print(backend, view);

    EXPECT_STREQ(backend.buffer, "[4, 5, 6]");
}

TEST(fmt, prints_dynamic_array_view) {
    fmt_test::Capture_Backend backend;
    Static_Array<int, 3>      src{{7, 8, 9}};
    Array_View<int>           view = src;

    fmt::print(backend, view);

    EXPECT_STREQ(backend.buffer, "[7, 8, 9]");
}

TEST(fmt, prints_array) {
    fmt_test::Capture_Backend backend;
    Array<int>                arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    fmt::print(backend, arr);

    EXPECT_STREQ(backend.buffer, "[1, 2, 3]");
}

TEST(fmt, prints_bounded_array) {
    fmt_test::Capture_Backend backend;
    Bounded_Array<int, 4>     arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    fmt::print(backend, arr);

    EXPECT_STREQ(backend.buffer, "[1, 2, 3]");
}

TEST(fmt, prints_empty_dynamic_array_view) {
    fmt_test::Capture_Backend backend;
    Array_View<int>           view{0, nullptr};

    fmt::print(backend, view);

    EXPECT_STREQ(backend.buffer, "[]");
}

TEST(fmt, prints_empty_array) {
    fmt_test::Capture_Backend backend;
    Array<int>                arr;

    fmt::print(backend, arr);

    EXPECT_STREQ(backend.buffer, "[]");
}

TEST(fmt, prints_partially_filled_array) {
    fmt_test::Capture_Backend backend;
    Array<int>                arr(8);
    arr.push_back(10);
    arr.push_back(20);

    fmt::print(backend, arr);

    EXPECT_GE(arr.capacity, 8);
    EXPECT_STREQ(backend.buffer, "[10, 20]");
}

TEST(fmt, prints_empty_bounded_array) {
    fmt_test::Capture_Backend backend;
    Bounded_Array<int, 4>     arr;

    fmt::print(backend, arr);

    EXPECT_STREQ(backend.buffer, "[]");
}

TEST(fmt, prints_partially_filled_bounded_array) {
    fmt_test::Capture_Backend backend;
    Bounded_Array<int, 4>     arr;
    arr.push_back(1);
    arr.push_back(2);

    fmt::print(backend, arr);

    EXPECT_STREQ(backend.buffer, "[1, 2]");
}

#endif
