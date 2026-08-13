#pragma once

#include "kstd/assert.hh"
#include "kstd/basic.hh"
#include "kstd/format.hh"

#include "gameos/gfx.hh"

namespace term {

constexpr u32 DEFAULT_PADDING = 10;

struct Terminal_State {
    u32 current_col;
    u32 current_row;
};

namespace hidden {
    inline Terminal_State state;
}

static auto next_line() -> void {
    using namespace hidden;

    state.current_row += 1;
    state.current_col = 0;
}

static auto max_cols() -> u32 {
    return (gfx::width() - 2 * DEFAULT_PADDING) / font::GLYPH_WIDTH;
}

struct Backend {
    static auto put_char(char c) -> void {
        using namespace hidden;

        if (c == '\n') {
            next_line();
            return;
        }

        if (state.current_col >= max_cols()) {
            next_line();
        }

        u32 x = DEFAULT_PADDING + state.current_col * font::GLYPH_WIDTH;
        u32 y = DEFAULT_PADDING + state.current_row * font::GLYPH_HEIGHT;
        gfx::draw_char_immediate(x, y, c, gfx::WHITE, gfx::TRANSPARENT);
        state.current_col++;
    }

    static auto new_line() -> void {
        next_line();
    }
};

namespace hidden {
    inline Backend term_backend;
}

auto print(string format) -> int {
    return fmt::print(hidden::term_backend, format);
}

template <typename T, typename... Rest>
auto print(string format, T&& value, Rest&&... rest) -> int {
    return fmt::print(hidden::term_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

auto println() -> int {
    return fmt::println(hidden::term_backend);
}

template <typename T>
auto print(T&& value) -> int {
    return fmt::print(hidden::term_backend, std::forward<T>(value));
}

template <typename T>
auto println(T&& value) -> int {
    return fmt::println(hidden::term_backend, std::forward<T>(value));
}

auto println(string format) -> int {
    return fmt::println(hidden::term_backend, format);
}

template <typename T, typename... Rest>
auto println(string format, T&& value, Rest&&... rest) -> int {
    return fmt::println(hidden::term_backend, format, std::forward<T>(value), std::forward<Rest>(rest)...);
}

[[nodiscard]] auto initialize() -> bool {
    hidden::state = {0, 0};
    auto is_initialized = gfx::is_initialized();
    if (is_initialized) {
        halt::add_printer(Backend::put_char);
    }
    return is_initialized;
}

}  // namespace term
