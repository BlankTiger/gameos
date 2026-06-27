#pragma once

namespace fb {
namespace term {

constexpr u32 DEFAULT_PADDING = 10;

struct Terminal_State {
    u32 current_col;
    u32 current_row;
    // Padding from screen edges in pixels.
    u32 padding;
};

static Terminal_State __state;

static auto next_line() -> void {
    __state.current_row += 1;
    __state.current_col = 0;
}

static auto max_cols() -> u32 {
    return (__current_frame.width - 2 * __state.padding) / font::GLYPH_WIDTH;
}

static auto put_char(char c) -> void {
    if (c == '\n') {
        next_line();
        return;
    }
    if (__state.current_col >= max_cols()) {
        next_line();
    }
    u32 x = __state.padding + __state.current_col * font::GLYPH_WIDTH;
    u32 y = __state.padding + __state.current_row * font::GLYPH_HEIGHT;
    fb::draw_char(x, y, c, WHITE, TRANSPARENT);
    __state.current_col++;
}

static auto write_string(const char* s) -> int {
    int written = 0;
    for (; *s; ++s) {
        put_char(*s);
        ++written;
    }
    return written;
}

static auto write_unsigned(u64 value) -> int {
    if (value == 0) {
        put_char('0');
        return 1;
    }
    char buf[20];
    int len = 0;
    while (value > 0) {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }
    for (int i = len - 1; i >= 0; --i) {
        put_char(buf[i]);
    }
    return len;
}

static auto write_signed(s64 value) -> int {
    int written = 0;
    if (value < 0) {
        put_char('-');
        ++written;
        // Handle minimum value overflow.
        if (value == -9223372036854775807LL - 1) {
            written += write_unsigned((u64)9223372036854775808ULL);
            return written;
        }
        value = -value;
    }
    written += write_unsigned((u64)value);
    return written;
}

static auto write_hex(u64 value) -> int {
    if (value == 0) {
        put_char('0');
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
        put_char(buf[i]);
    }
    return len;
}

static auto write_pointer(const void* value) -> int {
    int written = write_string("0x");
    written += write_hex((u64)(usize)value);
    return written;
}

static auto write_float(f64 value) -> int {
    int written = 0;
    if (value < 0) {
        put_char('-');
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
    written += write_unsigned(whole);
    put_char('.');
    ++written;
    char buf[6];
    for (usize i = 0; i < 6; ++i) {
        buf[5 - i] = (char)('0' + (scaled % 10));
        scaled /= 10;
    }
    for (usize i = 0; i < 6; ++i) {
        put_char(buf[i]);
    }
    written += 6;
    return written;
}

// -- print_value overloads --

static force_inline auto print_value(bool value) -> int {
    return value ? write_string("true") : write_string("false");
}

static force_inline auto print_value(char value) -> int {
    put_char(value);
    return 1;
}

static force_inline auto print_value(const char* value) -> int {
    if (value == nullptr) value = "(null)";
    return write_string(value);
}

template <usize N>
static force_inline auto print_value(const char (&value)[N]) -> int {
    return print_value((const char*)value);
}

template <usize N>
static force_inline auto print_value(char (&value)[N]) -> int {
    return print_value((const char*)value);
}

static force_inline auto print_value(std::nullptr_t) -> int {
    return write_string("(null)");
}

static force_inline auto print_value(s64 value) -> int {
    return write_signed(value);
}

static force_inline auto print_value(u64 value) -> int {
    return write_unsigned(value);
}

static force_inline auto print_value(f64 value) -> int {
    return write_float(value);
}

static force_inline auto print_value(const void* value) -> int {
    return write_pointer(value);
}

template <typename T>
static force_inline auto print_value(T&& value) -> int {
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, bool>) {
        return print_value((bool)value);
    } else if constexpr (std::is_same_v<U, char>) {
        return print_value((char)value);
    } else if constexpr (std::is_null_pointer_v<U>) {
        return print_value(nullptr);
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            return print_value((s64)value);
        } else {
            return print_value((u64)value);
        }
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        return print_value((Underlying)value);
    } else if constexpr (std::is_floating_point_v<U>) {
        return print_value((f64)value);
    } else if constexpr (std::is_pointer_v<U>) {
        if constexpr (std::is_same_v<std::remove_cv_t<std::remove_pointer_t<U>>, char>) {
            return print_value((const char*)value);
        } else {
            return print_value((const void*)value);
        }
    } else if constexpr (std::is_array_v<U> && std::is_same_v<std::remove_extent_t<U>, char>) {
        return print_value((const char*)value);
    } else if constexpr (requires(const U& object) { object.c_str(); }) {
        return print_value(value.c_str());
    } else if constexpr (requires(const U& object) {
                             object.data();
                             object.size();
                         }) {
        if constexpr (std::is_convertible_v<decltype(value.data()), const char*>) {
            const char* data = value.data();
            usize size = (usize)value.size();
            for (usize i = 0; i < size; ++i) {
                put_char(data[i]);
            }
            return (int)size;
        } else {
            return print_value((const void*)&value);
        }
    } else {
        return print_value((const void*)&value);
    }
}

// -- public API --

[[nodiscard]] auto initialize(const mem::Multiboot2_Info* mbi) -> bool {
    __state = {0, 0, DEFAULT_PADDING};
    return fb::initialize(mbi);
}

auto print(const char* format) -> int {
    int written = 0;
    for (const char* it = format; *it != '\0'; ++it) {
        if (*it == '%' && it[1] == '%') {
            put_char('%');
            ++written;
            ++it;
        } else {
            put_char(*it);
            ++written;
        }
    }
    return written;
}

template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int {
    int written = 0;

    for (const char* it = format; *it != '\0'; ++it) {
        if (*it != '%') {
            put_char(*it);
            ++written;
            continue;
        }

        if (it[1] == '%') {
            put_char('%');
            ++written;
            ++it;
            continue;
        }

        written += print_value(std::forward<T>(value));
        return written + print(it + 1, std::forward<Rest>(rest)...);
    }

    return written;
}

auto println() -> int {
    next_line();
    return 1;
}

auto println(const char* format) -> int {
    int written = print(format);
    next_line();
    return written + 1;
}

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    int written = print(format, std::forward<T>(value), std::forward<Rest>(rest)...);
    next_line();
    return written + 1;
}

}  // namespace term
}  // namespace fb
