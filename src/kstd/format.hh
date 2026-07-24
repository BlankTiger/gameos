#pragma once

#include <type_traits>
#include <utility>

#include "basic.hh"
#include "cstring.hh"
#include "enum_name.hh"
#include "string_view.hh"

//
// Backend-agnostic formatting utilities.
//
// Backend must provide:
//   auto put_char(char c) -> void;
//   auto new_line() -> void;
//
// put_char/new_line may be static (stateless backends, e.g. serial::Backend)
// or regular instance methods backed by member data (stateful backends,
// e.g. sprint's target string). Either way an instance is passed by
// reference into fmt::print/fmt::println; for stateless backends that's just
// a throwaway default-constructed value at the call site.
//

//
// For a custom type to be easily displayed by anything implementing fmt
// implement a `format` method on it:
//
// struct A {
//     const char* message;
//
//     auto format() -> string_view {
//         return string_view(message);
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
static force_inline auto print_string_view(Backend& backend, const string_view s) -> int {
    for (auto c : s) {
        backend.put_char(c);
    }
    return s.size;
}

template <typename Backend, typename T>
static force_inline auto print_value(Backend& backend, T&& value) -> int {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { value.format(); }) {
        return print_string_view(backend, value.format());
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
        string_view name = enum_name(value);
        if (name.size > 0) {
            return print_string_view(backend, name);
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
auto print(Backend& backend, const char* format) -> int {
    int written = 0;
    for (const char* it = format; *it != '\0'; ++it) {
        if (*it == '%' && it[1] == '%') {
            backend.put_char('%');
            ++written;
            ++it;
        } else {
            backend.put_char(*it);
            ++written;
        }
    }
    return written;
}

template <typename Backend, typename T>
auto print(Backend& backend, T&& value) -> int {
    return print_value(backend, std::forward<T>(value));
}

template <typename Backend, typename T>
auto println(Backend& backend, T&& value) -> int {
    int written = print_value(backend, std::forward<T>(value));
    backend.new_line();
    return written + 1;
}

template <typename Backend, typename T, typename... Rest>
auto print(Backend& backend, const char* format, T&& value, Rest&&... rest) -> int {
    int written = 0;

    for (const char* it = format; *it != '\0'; ++it) {
        if (*it != '%') {
            backend.put_char(*it);
            ++written;
            continue;
        }

        if (it[1] == '%') {
            backend.put_char('%');
            ++written;
            ++it;
            continue;
        }

        written += print_value(backend, std::forward<T>(value));
        return written + print(backend, it + 1, std::forward<Rest>(rest)...);
    }

    return written;
}

template <typename Backend>
auto println(Backend& backend) -> int {
    backend.new_line();
    return 1;
}

template <typename Backend>
auto println(Backend& backend, const char* format) -> int {
    int written = print(backend, format);
    backend.new_line();
    return written + 1;
}

template <typename Backend, typename T, typename... Rest>
auto println(Backend& backend, const char* format, T&& value, Rest&&... rest) -> int {
    int written = print(backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
    backend.new_line();
    return written + 1;
}

}  // namespace fmt
