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

struct Gfx_Backend {
    [[nodiscard]] static auto initialize(const mem::Multiboot2_Info*) -> bool {
        return true;
    }

    static auto set_pixel(u32, u32, Color) -> void {}
    static auto width() -> u32 { return 0; }
    static auto height() -> u32 { return 0; }
};

struct Backend {
    static auto put_char(char c) -> void {
        terminal_putchar((u8)c);
    }

    static auto new_line() -> void {
        terminal_putchar('\n');
    }
};

namespace term {

[[nodiscard]] auto initialize(const mem::Multiboot2_Info* mbi) -> bool {
    (void)mbi;
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (usize y = 0; y < VGA_HEIGHT; y++) {
        for (usize x = 0; x < VGA_WIDTH; x++) {
            const usize index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }

    return true;
}

auto print(const char* format) -> int {
    return fmt::print<Backend>(format);
}

template <typename T, typename... Rest>
auto print(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::print<Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

auto println() -> int {
    return fmt::println<Backend>();
}

auto println(const char* format) -> int {
    return fmt::println<Backend>(format);
}

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::println<Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

}  // namespace term

}  // namespace vga
