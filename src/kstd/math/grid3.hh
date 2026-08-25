#pragma once

#include <utility>

#include "kstd/array.hh"
#include "kstd/string_builder.hh"

// layers (top to bottom)
// rows   (top to bottom)
// cols   (left to right)
template <typename T = bool>
struct Grid3 {
    Array<T> backing_array;

    T default_value;
    u32 rows, cols, layers;
    u32 cells_in_layer;

    Grid3(u32 rows, u32 cols, u32 layers, const T& default_value = {})
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

    force_inline auto set(u32 row, u32 col, u32 layer, const T& value) -> void {
        const auto index = backing_array_index_for(row, col, layer);
        backing_array[index] = value;
    }

    force_inline auto clear(u32 row, u32 col, u32 layer) -> void {
        const auto index = backing_array_index_for(row, col, layer);
        backing_array[index] = default_value;
    }

    template <typename Skip_Predicate>
    auto clear_layer(u32 layer, Skip_Predicate skip) -> void {
        for (u32 row = 0; row < rows; ++row) {
            for (u32 col = 0; col < cols; ++col) {
                if (skip(row, col, layer)) continue;
                clear(row, col, layer);
            }
        }
    }

    auto clear_layer(u32 layer) -> void {
        clear_layer(layer, [](u32, u32, u32) { return false; });
    }

    force_inline auto move(u32 from_row, u32 from_col, u32 from_layer, u32 to_row, u32 to_col, u32 to_layer) -> void {
        const auto from_index = backing_array_index_for(from_row, from_col, from_layer);
        const auto to_index   = backing_array_index_for(to_row,   to_col,   to_layer);
        backing_array[to_index]   = std::move(backing_array[from_index]);
        backing_array[from_index] = default_value;
    }

    auto move_layer(u32 from_layer, u32 to_layer) -> void {
        for (u32 row = 0; row < rows; ++row) {
            for (u32 col = 0; col < cols; ++col) {
                move(row, col, from_layer, row, col, to_layer);
            }
        }
    }

    force_inline auto copy(u32 from_row, u32 from_col, u32 from_layer, u32 to_row, u32 to_col, u32 to_layer) -> void {
        const auto from_index = backing_array_index_for(from_row, from_col, from_layer);
        const auto to_index   = backing_array_index_for(to_row,   to_col,   to_layer);
        backing_array[to_index] = backing_array[from_index];
    }

    template <typename Skip_Predicate>
    auto copy_layer(u32 from_layer, u32 to_layer, Skip_Predicate skip) -> void {
        for (u32 row = 0; row < rows; ++row) {
            for (u32 col = 0; col < cols; ++col) {
                if (skip(row, col, from_layer)) continue;
                copy(row, col, from_layer, row, col, to_layer);
            }
        }
    }

    auto copy_layer(u32 from_layer, u32 to_layer) -> void {
        copy_layer(from_layer, to_layer, [](u32, u32, u32) { return false; });
    }

    auto layer_filled_with(u32 layer, const T& value) const -> bool {
        for (u32 index = layer * cells_in_layer; index < (layer + 1) * cells_in_layer; ++index) {
            if (backing_array[index] != value) return false;
        }
        return true;
    }

    // Check if the space down a layer contains a value that is different from the default_value.
    // value_lower == default_value -> can move lower
    auto can_move_lower(u32 row, u32 col, u32 layer) const -> bool {
        if (layer + 1 >= layers) return false;

        const auto& value_lower = at(row, col, layer + 1);
        return value_lower == default_value;
    }

    force_inline auto at(u32 row, u32 col, u32 layer) const -> T {
        const auto index = backing_array_index_for(row, col, layer);
        return backing_array[index];
    }

    force_inline auto backing_array_index_for(u32 row, u32 col, u32 layer) const -> s64 {
        return cells_in_layer * layer + row * cols + col;
    }

    auto format() const -> string {
        String_Builder builder;
        for (u32 layer_index = 0; layer_index < layers; ++layer_index) {
            for (u32 row_index = 0; row_index < rows; ++row_index) {
                builder.append("  [");
                for (u32 col_index = 0; col_index < cols; ++col_index) {
                    builder.print(at(row_index, col_index, layer_index));
                    if (col_index != cols - 1) builder.append(", ");
                }
                builder.append("]\n");
            }
            builder.append("\n\n");
        }
        return builder.to_string();
    }
};

#ifdef UNIT_TESTS_KSTD_MATH_GRID3

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

TEST(Grid3_bool, can_move_lower_empty_space_below) {
    Grid3 grid(2, 2, 3);
    grid.set(0, 1, 1, true);
    EXPECT_TRUE(grid.can_move_lower(0, 1, 1));
}

TEST(Grid3_bool, cant_move_lower_on_last_layer) {
    Grid3 grid(2, 2, 3);
    grid.set(0, 1, 2, true);
    EXPECT_FALSE(grid.can_move_lower(0, 1, 2));
}

TEST(Grid3_bool, trying_to_go_lower_than_last_layer_returns_false) {
    Grid3 grid(2, 2, 3);
    grid.set(0, 1, 2, true);
    EXPECT_FALSE(grid.can_move_lower(0, 1, 3));
}

TEST(Grid3_bool, cant_move_lower_when_something_is_blocking_lower) {
    Grid3 grid(2, 2, 3);
    grid.set(0, 1, 1, true);
    grid.set(0, 1, 2, true);
    EXPECT_FALSE(grid.can_move_lower(0, 1, 1));
}

#endif
