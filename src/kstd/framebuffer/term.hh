#pragma once

namespace fb {
namespace term {

constexpr u32 DEFAULT_PADDING = 10;

struct Terminal_State {
    u32 current_col;
    u32 current_row;
};

static Terminal_State __state;

static auto next_line() -> void {
    __state.current_row += 1;
    __state.current_col = 0;
}

static auto max_cols() -> u32 {
    return (__current_frame.width - 2 * DEFAULT_PADDING) / font::GLYPH_WIDTH;
}

struct Backend {
    static auto put_char(char c) -> void {
        if (c == '\n') {
            next_line();
            return;
        }
        if (__state.current_col >= max_cols()) {
            next_line();
        }
        u32 x = DEFAULT_PADDING + __state.current_col * font::GLYPH_WIDTH;
        u32 y = DEFAULT_PADDING + __state.current_row * font::GLYPH_HEIGHT;
        fb::draw_char(x, y, c, WHITE, TRANSPARENT);
        __state.current_col++;
    }

    static auto new_line() -> void {
        next_line();
    }
};

// -- public API --

[[nodiscard]] auto initialize(const mem::Multiboot2_Info* mbi) -> bool {
    __state = {0, 0};
    return fb::initialize(mbi);
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
}  // namespace fb
