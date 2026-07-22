#pragma once

#include <type_traits>
#include <utility>

#include "../basic.hh"
#include "../cstring.hh"
#include "string.hh"

//
// Backend-agnostic formatting utilities.
//
// Backend must provide:
//   static auto put_char(char c) -> void;
//   static auto new_line() -> void;
//

//
// For a custom type to be easily displayed by anything implementing fmt
// implement a `format` method on it:
//
// struct A {
//     const char* message;
//
//     auto format() -> String_View {
//         return String_View(message);
//     }
// }
//
namespace fmt {

template <typename Backend>
static auto write_string(const char* s) -> int {
    int written = 0;
    for (; *s; ++s) {
        Backend::put_char(*s);
        ++written;
    }
    return written;
}

template <typename Backend>
static auto write_unsigned(u64 value) -> int {
    if (value == 0) {
        Backend::put_char('0');
        return 1;
    }
    char buf[20];
    int len = 0;
    while (value > 0) {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (int i = len - 1; i >= 0; --i) {
        Backend::put_char(buf[i]);
    }
    return len;
}

template <typename Backend>
static auto write_signed(s64 value) -> int {
    int written = 0;
    if (value < 0) {
        Backend::put_char('-');
        ++written;
        // Handle minimum value overflow.
        if (value == -9223372036854775807LL - 1) {
            written += write_unsigned<Backend>((u64)9223372036854775808ULL);
            return written;
        }
        value = -value;
    }
    written += write_unsigned<Backend>((u64)value);
    return written;
}

template <typename Backend>
static auto write_hex(u64 value) -> int {
    if (value == 0) {
        Backend::put_char('0');
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
        Backend::put_char(buf[i]);
    }
    return len;
}

template <typename Backend>
static auto write_pointer(const void* value) -> int {
    int written = write_string<Backend>("0x");
    written += write_hex<Backend>((u64)(usize)value);
    return written;
}

template <typename Backend>
static auto write_float(f64 value) -> int {
    int written = 0;
    if (value < 0) {
        Backend::put_char('-');
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
    written += write_unsigned<Backend>(whole);
    Backend::put_char('.');
    ++written;
    char buf[6];
    for (usize i = 0; i < 6; ++i) {
        buf[5 - i] = (char)('0' + (scaled % 10));
        scaled /= 10;
    }
    for (usize i = 0; i < 6; ++i) {
        Backend::put_char(buf[i]);
    }
    written += 6;
    return written;
}

// -- print_value overloads --

template <typename Backend>
static force_inline auto print_value(bool value) -> int {
    return value ? write_string<Backend>("true") : write_string<Backend>("false");
}

template <typename Backend>
static force_inline auto print_value(char value) -> int {
    Backend::put_char(value);
    return 1;
}

template <typename Backend>
static force_inline auto print_value(const char* value) -> int {
    if (value == nullptr) value = "(null)";
    return write_string<Backend>(value);
}

template <typename Backend, usize N>
static force_inline auto print_value(const char (&value)[N]) -> int {
    return print_value<Backend>((const char*)value);
}

template <typename Backend, usize N>
static force_inline auto print_value(char (&value)[N]) -> int {
    return print_value<Backend>((const char*)value);
}

template <typename Backend>
static force_inline auto print_value(std::nullptr_t) -> int {
    return write_string<Backend>("(null)");
}

template <typename Backend>
static force_inline auto print_value(s64 value) -> int {
    return write_signed<Backend>(value);
}

template <typename Backend>
static force_inline auto print_value(u64 value) -> int {
    return write_unsigned<Backend>(value);
}

template <typename Backend>
static force_inline auto print_value(f64 value) -> int {
    return write_float<Backend>(value);
}

template <typename Backend>
static force_inline auto print_value(const void* value) -> int {
    return write_pointer<Backend>(value);
}

template <typename Backend>
static force_inline auto print_string_view(const String_View s) -> int {
    for (auto c : s) {
        Backend::put_char(c);
    }
    return s.size;
}

template <typename Backend, typename T>
static force_inline auto print_value(T&& value) -> int {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { value.format(); }) {
        return print_string_view<Backend>(value.format());
    } else if constexpr (std::is_same_v<U, bool>) {
        return print_value<Backend>((bool)value);
    } else if constexpr (std::is_same_v<U, char>) {
        return print_value<Backend>((char)value);
    } else if constexpr (std::is_null_pointer_v<U>) {
        return print_value<Backend>(nullptr);
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            return print_value<Backend>((s64)value);
        } else {
            return print_value<Backend>((u64)value);
        }
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        return print_value<Backend>((Underlying)value);
    } else if constexpr (std::is_floating_point_v<U>) {
        return print_value<Backend>((f64)value);
    } else if constexpr (std::is_pointer_v<U>) {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
            return print_value<Backend>((const char*)value);
        } else {
            return print_value<Backend>((const void*)value);
        }
    } else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_extent_t<U>, char>) {
        return print_value<Backend>((const char*)value);
    } else if constexpr (requires { value.c_str(); }) {
        return print_value<Backend>(value.c_str());
    } else if constexpr (requires { value.data(); value.size; }) {
        if constexpr (std::is_convertible_v<decltype(value.data()), const char*>) {
            const char* data = value.data();
            usize size = (usize)value.size;
            for (usize i = 0; i < size; ++i) {
                Backend::put_char(data[i]);
            }
            return (int)size;
        } else {
            return print_value<Backend>((const void*)&value);
        }
    } else if constexpr (requires { value.data(); value.size(); }) {
        if constexpr (std::is_convertible_v<decltype(value.data()), const char*>) {
            const char* data = value.data();
            usize size = (usize)value.size();
            for (usize i = 0; i < size; ++i) {
                Backend::put_char(data[i]);
            }
            return (int)size;
        } else {
            return print_value<Backend>((const void*)&value);
        }
    } else {
        return print_value<Backend>((const void*)&value);
    }
}

// -- format string parsing --

template <typename Backend>
auto print(const char* format) -> int {
    int written = 0;
    for (const char* it = format; *it != '\0'; ++it) {
        if (*it == '%' && it[1] == '%') {
            Backend::put_char('%');
            ++written;
            ++it;
        } else {
            Backend::put_char(*it);
            ++written;
        }
    }
    return written;
}

template <typename Backend, typename T>
auto print(T&& value) -> int {
    return print_value<Backend>(std::forward<T>(value));
}

template <typename Backend, typename T>
auto println(T&& value) -> int {
    int written = print_value<Backend>(std::forward<T>(value));
    Backend::new_line();
    return written + 1;
}

template <typename Backend, typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int {
    int written = 0;

    for (const char* it = format; *it != '\0'; ++it) {
        if (*it != '%') {
            Backend::put_char(*it);
            ++written;
            continue;
        }

        if (it[1] == '%') {
            Backend::put_char('%');
            ++written;
            ++it;
            continue;
        }

        written += print_value<Backend>(std::forward<T>(value));
        return written + print<Backend>(it + 1, std::forward<Rest>(rest)...);
    }

    return written;
}

template <typename Backend>
auto println() -> int {
    Backend::new_line();
    return 1;
}

template <typename Backend>
auto println(const char* format) -> int {
    int written = print<Backend>(format);
    Backend::new_line();
    return written + 1;
}

template <typename Backend, typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    int written = print<Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
    Backend::new_line();
    return written + 1;
}

}  // namespace fmt
