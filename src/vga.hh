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

auto vprintf(const char* format, va_list args) -> int {
    int written = 0;

    for (const char* it = format; *it != '\0'; ++it) {
        if (*it != '%') {
            terminal_putchar((u8)*it);
            ++written;
            continue;
        }

        ++it;
        if (*it == '\0') break;

        switch (*it) {
            case '%':
                terminal_putchar('%');
                ++written;
                break;
            case 'c':
                terminal_putchar((u8)va_arg(args, int));
                ++written;
                break;
            case 's': {
                const char* str = va_arg(args, const char*);
                if (str == nullptr) str = "(null)";
                terminal_writestring(str);
                written += (int)strlen(str);
                break;
            }
            case 'd':
            case 'i': {
                int value = va_arg(args, int);
                terminal_writeint((s64)value);
                written += terminal_count_signed((s64)value);
            } break;
            case 'u': {
                unsigned int value = va_arg(args, unsigned int);
                terminal_write_unsigned((u64)value);
                written += terminal_count_unsigned((u64)value);
            } break;
            case 'x': {
                unsigned int value = va_arg(args, unsigned int);
                terminal_write_hex_compact((u64)value);
                written += terminal_count_hex_compact((u64)value);
            } break;
            case 'X': {
                unsigned int value = va_arg(args, unsigned int);
                terminal_write_hex_compact((u64)value, true);
                written += terminal_count_hex_compact((u64)value);
            } break;
            case 'p': {
                void* value = va_arg(args, void*);
                terminal_write_pointer(value);
                written += 2 + terminal_count_hex_compact((u64)(usize)value);
            } break;
            case 'l': {
                ++it;
                if (*it == '\0') break;

                if (*it == 'l') {
                    ++it;
                    if (*it == '\0') break;

                    switch (*it) {
                        case 'd':
                        case 'i': {
                            long long value = va_arg(args, long long);
                            terminal_writeint((s64)value);
                            written += terminal_count_signed((s64)value);
                        } break;
                        case 'u': {
                            unsigned long long value = va_arg(args, unsigned long long);
                            terminal_write_unsigned((u64)value);
                            written += terminal_count_unsigned((u64)value);
                        } break;
                        case 'x': {
                            unsigned long long value = va_arg(args, unsigned long long);
                            terminal_write_hex_compact((u64)value);
                            written += terminal_count_hex_compact((u64)value);
                        } break;
                        case 'X': {
                            unsigned long long value = va_arg(args, unsigned long long);
                            terminal_write_hex_compact((u64)value, true);
                            written += terminal_count_hex_compact((u64)value);
                        } break;
                        default:
                            terminal_putchar('%');
                            terminal_putchar('l');
                            terminal_putchar('l');
                            terminal_putchar((u8)*it);
                            written += 4;
                            break;
                    }
                } else {
                    switch (*it) {
                        case 'd':
                        case 'i': {
                            long value = va_arg(args, long);
                            terminal_writeint((s64)value);
                            written += terminal_count_signed((s64)value);
                        } break;
                        case 'u': {
                            unsigned long value = va_arg(args, unsigned long);
                            terminal_write_unsigned((u64)value);
                            written += terminal_count_unsigned((u64)value);
                        } break;
                        case 'x': {
                            unsigned long value = va_arg(args, unsigned long);
                            terminal_write_hex_compact((u64)value);
                            written += terminal_count_hex_compact((u64)value);
                        } break;
                        case 'X': {
                            unsigned long value = va_arg(args, unsigned long);
                            terminal_write_hex_compact((u64)value, true);
                            written += terminal_count_hex_compact((u64)value);
                        } break;
                        default:
                            terminal_putchar('%');
                            terminal_putchar('l');
                            terminal_putchar((u8)*it);
                            written += 3;
                            break;
                    }
                }
                break;
            }
            default:
                terminal_putchar('%');
                terminal_putchar((u8)*it);
                written += 2;
                break;
        }
    }

    return written;
}

auto printf(const char* format, ...) -> int {
    va_list args;
    va_start(args, format);
    int written = vprintf(format, args);
    va_end(args);
    return written;
}

}  // namespace vga
