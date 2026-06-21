#pragma once
namespace term {

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

static inline auto vga_entry_color(enum vga_color fg, enum vga_color bg) -> u8 {
    return fg | bg << 4;
}

static inline auto vga_entry(u8 uc, u8 color) -> u16 {
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

auto terminal_write_hex(u64 value) -> void {
    static const char digits[] = "0123456789abcdef";
    char buffer[16];

    for (usize i = 0; i < 16; i++) {
        buffer[15 - i] = digits[value & 0x0f];
        value >>= 4;
    }

    terminal_write(buffer, sizeof(buffer));
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

}  // namespace term
