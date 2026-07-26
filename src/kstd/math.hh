#pragma once

#include "basic.hh"
#include "array.hh"
#include "assert.hh"

namespace math {

force_inline auto abs(s32 x) -> s32 {
    return (x ^ (x >> 31)) - (x >> 31);
}

force_inline auto abs_diff(u32 a, u32 b) {
    return (a > b) ? (a - b) : (b - a);
}

force_inline auto lerp(u8 a, u8 b, int pos, int max) -> u8 {
    return a + ((b - a) * pos) / max;
}

struct Rect {
    u32 x1, x2;
    u32 y1, y2;

    static force_inline auto create(u32 x, u32 y, u32 w, u32 h) -> Rect {
        return {
            .x1 = x,
            .x2 = x + w,
            .y1 = y,
            .y2 = y + h,
        };
    }

    force_inline void clip(u32 screen_width, u32 screen_height) {
        x1 = (x1 >= screen_width)  ? screen_width  - 1 : x1;
        x2 = (x2 >= screen_width)  ? screen_width  - 1 : x2;
        y1 = (y1 >= screen_height) ? screen_height - 1 : y1;
        y2 = (y2 >= screen_height) ? screen_height - 1 : y2;
    }
};

// rows (top to bottom)
// cols (left to right)
// layers (top to bottom)
template <typename T = bool>
struct Grid3 {
    Array<T> backing_array;
    T default_value;
    u32 rows, cols, layers;
    u32 cells_in_layer;

    Grid3(u32 rows, u32 cols, u32 layers, T&& default_value = {})
        : backing_array(Array<T>{rows * cols * layers}),
          default_value(default_value),
          rows(rows),
          cols(cols),
          layers(layers),
          cells_in_layer(rows * cols) {
        for (u32 index = 0; index < rows * cols * layers; ++index) {
            backing_array.push_back(default_value);
        }
    }

    auto set(u32 row, u32 col, u32 layer, T&& value) -> void {
        const auto index = backing_array_index_for(row, col, layer);
        backing_array[index] = value;
    }

    auto clear(u32 row, u32 col, u32 layer) -> void {
        const auto index = backing_array_index_for(row, col, layer);
        backing_array[index] = default_value;
    }

    auto move(u32 from_row, u32 from_col, u32 from_layer, u32 to_row, u32 to_col, u32 to_layer) -> void {
        const auto from_index = backing_array_index_for(from_row, from_col, from_layer);
        const auto to_index   = backing_array_index_for(to_row,   to_col,   to_layer);
        backing_array[to_index]   = std::move(backing_array[from_index]);
        backing_array[from_index] = default_value;
    }

    auto layer_filled_with(u32 layer, T&& value) const -> bool {
        for (u32 index = layer * cells_in_layer; index < (layer + 1) * cells_in_layer; ++index) {
            if (backing_array[index] != value) return false;
        }
        return true;
    }

    force_inline auto at(u32 row, u32 col, u32 layer) const -> T {
        const auto index = backing_array_index_for(row, col, layer);
        return backing_array[index];
    }

    force_inline auto backing_array_index_for(u32 row, u32 col, u32 layer) const -> usize {
        return cells_in_layer * layer + row * cols + col;
    }
};

#ifdef UNIT_TESTS

TEST(Grid3_bool, can_set_a_custom_default_value) {
    {
        Grid3 grid(3, 3, 3);
        EXPECT_EQ(grid.at(0, 0, 0), bool{});
        EXPECT_EQ(grid.at(0, 0, 0), false);
    }

    {
        Grid3 grid(3, 3, 3, false);
        EXPECT_EQ(grid.at(0, 0, 0), bool{});
        EXPECT_EQ(grid.at(0, 0, 0), false);
    }

    {
        Grid3 grid(3, 3, 3, true);
        EXPECT_EQ(grid.at(0, 0, 0), true);
    }
}

TEST(Grid3_bool, can_mark_points_as_occupied) {
    Grid3 grid(3, 3, 3);
    EXPECT_EQ(grid.backing_array.size, 27);

    grid.set(1, 1, 1, true);
    EXPECT_TRUE(grid.at(1, 1, 1));
}

TEST(Grid3_bool, can_free_points) {
    Grid3 grid(3, 3, 3);
    grid.set(1, 1, 1, true);
    EXPECT_TRUE(grid.at(1, 1, 1));

    grid.clear(1, 1, 1);
    EXPECT_FALSE(grid.at(1, 1, 1));
}

TEST(Grid3_bool, can_move_points) {
    Grid3 grid(3, 3, 3);
    grid.set(1, 1, 1, true);
    EXPECT_TRUE(grid.at(1, 1, 1));

    grid.move(1, 1, 1, 2, 2, 2);
    EXPECT_FALSE(grid.at(1, 1, 1));
    EXPECT_TRUE(grid.at(2, 2, 2));
}

TEST(Grid3_bool, can_check_for_a_layer_filled_with_value) {
    Grid3 grid(2, 2, 2);
    EXPECT_TRUE(grid.layer_filled_with(0, false));

    grid.set(0, 0, 0, true);
    grid.set(0, 1, 0, true);
    EXPECT_FALSE(grid.layer_filled_with(0, true));
    EXPECT_FALSE(grid.layer_filled_with(0, false));

    grid.set(1, 0, 0, true);
    grid.set(1, 1, 0, true);
    EXPECT_TRUE(grid.layer_filled_with(0, true));
}

#endif

}  // namespace math
