#pragma once

namespace vga {

/* Hardware text mode color constants. */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static force_inline auto vga_entry_color(enum vga_color fg, enum vga_color bg) -> u8 {
    return fg | bg << 4;
}

static force_inline auto vga_entry(u8 uc, u8 color) -> u16 {
    return (u16)uc | (u16)color << 8;
}

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

usize terminal_row;
usize terminal_column;
u8 terminal_color;
u16* terminal_buffer = (u16*)VGA_MEMORY;

auto terminal_initialize() -> void {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (usize y = 0; y < VGA_HEIGHT; y++) {
        for (usize x = 0; x < VGA_WIDTH; x++) {
            const usize index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

auto terminal_setcolor(u8 color) -> void {
    terminal_color = color;
}

auto terminal_putentryat(u8 c, u8 color, usize x, usize y) -> void {
    const usize index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

auto terminal_next_row() -> void {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT) terminal_row = 0;
}

auto terminal_putchar(u8 c) -> void {
    if (c == '\n') {
        terminal_next_row();
    } else {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_next_row();
        }
    }
}

auto terminal_write(const char* data, usize size) -> void {
    for (usize i = 0; i < size; i++) terminal_putchar(data[i]);
}

auto terminal_writestring(const char* data) -> void {
    terminal_write(data, strlen(data));
}

static force_inline auto terminal_count_unsigned(u64 value) -> int {
    int count = 1;
    while (value >= 10) {
        value /= 10;
        ++count;
    }
    return count;
}

static force_inline auto terminal_count_signed(s64 value) -> int {
    if (value < 0) {
        u64 magnitude = (u64)(-(value + 1)) + 1;
        return 1 + terminal_count_unsigned(magnitude);
    }

    return terminal_count_unsigned((u64)value);
}

static force_inline auto terminal_count_hex_compact(u64 value) -> int {
    int count = 1;
    while (value >= 16) {
        value >>= 4;
        ++count;
    }
    return count;
}

static force_inline auto terminal_write_unsigned(u64 value) -> void {
    char buffer[21];
    usize length = 0;

    if (value == 0) {
        terminal_putchar('0');
        return;
    }

    while (value != 0) {
        buffer[length++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (length != 0) {
        terminal_putchar(buffer[--length]);
    }
}

static force_inline auto terminal_write_hex_compact(u64 value, bool uppercase = false) -> void {
    static const char lower_digits[] = "0123456789abcdef";
    static const char upper_digits[] = "0123456789ABCDEF";
    const char* digits = uppercase ? upper_digits : lower_digits;
    char buffer[16];
    usize length = 0;

    if (value == 0) {
        terminal_putchar('0');
        return;
    }

    while (value != 0) {
        buffer[length++] = digits[value & 0x0f];
        value >>= 4;
    }

    while (length != 0) {
        terminal_putchar(buffer[--length]);
    }
}

auto terminal_write_hex(u64 value) -> void {
    static const char digits[] = "0123456789abcdef";
    char buffer[16];

    for (usize i = 0; i < 16; i++) {
        buffer[15 - i] = digits[value & 0x0f];
        value >>= 4;
    }

    terminal_write(buffer, sizeof(buffer));
}

auto terminal_write_pointer(const void* value) -> void {
    terminal_writestring("0x");
    terminal_write_hex_compact((u64)(usize)value);
}

auto terminal_writeint(s64 value) -> void {
    char buffer[21];
    usize length = 0;

    u64 magnitude;
    if (value < 0) {
        terminal_putchar('-');
        magnitude = (u64)(-(value + 1)) + 1;
    } else {
        magnitude = (u64)value;
    }

    if (magnitude == 0) {
        terminal_putchar('0');
        return;
    }

    while (magnitude != 0) {
        buffer[length++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }

    while (length != 0) {
        terminal_putchar(buffer[--length]);
    }
}

static force_inline auto print_remaining(const char* format) -> int {
    int written = 0;

    for (const char* it = format; *it != '\0'; ++it) {
        if (*it == '%' && it[1] == '%') {
            terminal_putchar('%');
            ++written;
            ++it;
            continue;
        }

        terminal_putchar((u8)*it);
        ++written;
    }

    return written;
}

static force_inline auto print_value(bool value) -> int {
    if (value) {
        terminal_writestring("true");
        return 4;
    }

    terminal_writestring("false");
    return 5;
}

static force_inline auto print_value(char value) -> int {
    terminal_putchar((u8)value);
    return 1;
}

static force_inline auto print_value(const char* value) -> int {
    if (value == nullptr) value = "(null)";
    terminal_writestring(value);
    return (int)strlen(value);
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
    terminal_writestring("(null)");
    return 6;
}

static force_inline auto print_value(s64 value) -> int {
    terminal_writeint(value);
    return terminal_count_signed(value);
}

static force_inline auto print_value(u64 value) -> int {
    terminal_write_unsigned(value);
    return terminal_count_unsigned(value);
}

static force_inline auto print_value(f64 value) -> int {
    int written = 0;

    if (value < 0) {
        terminal_putchar('-');
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

    terminal_write_unsigned(whole);
    written += terminal_count_unsigned(whole);

    terminal_putchar('.');
    ++written;

    char buffer[6];
    for (usize i = 0; i < 6; ++i) {
        buffer[5 - i] = (char)('0' + (scaled % 10));
        scaled /= 10;
    }
    terminal_write(buffer, sizeof(buffer));
    written += 6;

    return written;
}

static force_inline auto print_value(const void* value) -> int {
    terminal_write_pointer(value);
    return 2 + terminal_count_hex_compact((u64)(usize)value);
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
            terminal_write(data, size);
            return (int)size;
        } else {
            return print_value((const void*)&value);
        }
    } else {
        return print_value((const void*)&value);
    }
}

auto print(const char* format) -> int {
    return print_remaining(format);
}

template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int {
    int written = 0;

    for (const char* it = format; *it != '\0'; ++it) {
        if (*it != '%') {
            terminal_putchar((u8)*it);
            ++written;
            continue;
        }

        if (it[1] == '%') {
            terminal_putchar('%');
            ++written;
            ++it;
            continue;
        }

        written += print_value(std::forward<T>(value));
        return written + print(it + 1, std::forward<Rest>(rest)...);
    }

    return written;
}

auto println(const char* format) -> int {
    int written = print(format);
    terminal_putchar('\n');
    return written + 1;
}

auto println() -> int {
    terminal_putchar('\n');
    return 1;
}

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    int written = print(format, std::forward<T>(value), std::forward<Rest>(rest)...);
    terminal_putchar('\n');
    return written + 1;
}

}  // namespace vga
