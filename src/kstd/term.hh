#pragma once

#include "basic.hh"
#include "gfx.hh"

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
    return (gfx::width() - 2 * DEFAULT_PADDING) / font::GLYPH_WIDTH;
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
        gfx::draw_char_immediate(x, y, c, gfx::WHITE, gfx::TRANSPARENT);
        __state.current_col++;
    }

    static auto new_line() -> void {
        next_line();
    }
};

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

template <typename T>
auto print(T&& value) -> int {
    return fmt::print<Backend>(std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    return fmt::println<Backend>(std::forward<T>(value));
}

auto println(const char* format) -> int {
    return fmt::println<Backend>(format);
}

template <typename T, typename... Rest>
auto println(const char* format, T&& value, Rest&&... rest) -> int {
    return fmt::println<Backend>(format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

[[nodiscard]] auto initialize() -> bool {
    __state = {0, 0};
    auto is_initialized = gfx::is_initialized();
    if (is_initialized) {
        halt_set_print(+[](const string_view file, u32 line, u32 col, const string_view message) {
            print("%:%:%", file, line, col);
            if (message != nullptr) println(": %", message);
        });
    }
    return is_initialized;
}

}  // namespace term
