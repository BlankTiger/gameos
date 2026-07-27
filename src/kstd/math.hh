#pragma once

#include <type_traits>

#include "basic.hh"
#include "array.hh"
#include "assert.hh"
#include "string.hh"

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

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T>
struct Vec2 {
    using value_type = T;
    static constexpr auto size = 2;
    union {
        struct { T x, y; };
        T data[size];
    };

    constexpr auto operator[](usize i) -> T& { return data[i]; }
    constexpr auto operator[](usize i) const -> const T& { return data[i]; }
    [[nodiscard]] static constexpr auto zero() -> Vec2 { return Vec2{ T(0), T(0) }; }
    [[nodiscard]] static constexpr auto one()  -> Vec2 { return Vec2{ T(1), T(1) }; }
};

template <Numeric T>
struct Vec3 {
    using value_type = T;
    static constexpr auto size = 3;
    union {
        struct { T x, y, z; };
        T data[size];
    };

    constexpr auto operator[](usize i) -> T& { return data[i]; }
    constexpr auto operator[](usize i) const -> const T& { return data[i]; }
    [[nodiscard]] static constexpr auto zero() -> Vec3 { return Vec3{ T(0), T(0), T(0) }; }
    [[nodiscard]] static constexpr auto one()  -> Vec3 { return Vec3{ T(1), T(1), T(1) }; }
};

template <typename T>
struct is_vec : std::false_type {};

template <typename T>
struct is_vec<Vec2<T>> : std::true_type {};

template <typename T>
struct is_vec<Vec3<T>> : std::true_type {};

template <typename T>
concept IsVector = is_vec<std::remove_cvref_t<T>>::value;

template <IsVector V>
[[nodiscard]] constexpr auto operator+(const V& a, const V& b) -> V {
    V result {};
    for (usize i = 0; i < V::size; ++i) result[i] = a[i] + b[i];
    return result;
}

template <IsVector V>
constexpr auto operator+=(V& a, const V& b) -> V& {
    for (usize i = 0; i < V::size; ++i) a[i] += b[i];
    return a;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator-(const V& a, const V& b) -> V {
    V result {};
    for (usize i = 0; i < V::size; ++i) result[i] = a[i] - b[i];
    return result;
}

template <IsVector V>
constexpr auto operator-=(V& a, const V& b) -> V& {
    for (usize i = 0; i < V::size; ++i) a[i] -= b[i];
    return a;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator*(const V& v, typename V::value_type scalar) -> V {
    V result{};
    for (usize i = 0; i < V::size; ++i) result[i] = v[i] * scalar;
    return result;
}

template <IsVector V>
constexpr auto operator*=(V& a, typename V::value_type scalar) -> V& {
    for (usize i = 0; i < V::size; ++i) a[i] *= scalar;
    return a;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator*(typename V::value_type scalar, const V& v) -> V {
    return v * scalar;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator/(const V& v, typename V::value_type scalar) -> V {
    V result{};
    for (usize i = 0; i < V::size; i++) result[i] = v[i] / scalar;
    return result;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator-(const V& v) -> V {
    V result{};
    for (usize i = 0; i < V::size; i++) result[i] = -v[i];
    return result;
}

template <IsVector V>
constexpr auto operator==(const V& a, const V& b) -> bool {
    for (usize i = 0; i < V::size; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}


template <IsVector V>
constexpr auto operator!=(const V& a, const V& b) -> bool {
    return !(a == b);
}

template <IsVector V>
[[nodiscard]] constexpr auto dot(const V& a, const V& b) -> typename V::value_type {
    typename V::value_type result{};
    for (usize i = 0; i < V::size; i++) result += a[i] * b[i];
    return result;
}

template <IsVector V>
[[nodiscard]] constexpr auto length_sq(const V& v) -> typename V::value_type {
    return dot(v, v);
}

#ifdef UNIT_TESTS

TEST(Vec2, can_create_vec2) {
    Vec2<s32> v{1, 2};

    EXPECT_EQ(v.x, 1);
    EXPECT_EQ(v.y, 2);

    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
}


TEST(Vec3, can_create_vec3) {
    Vec3<s32> v{1, 2, 3};

    EXPECT_EQ(v.x, 1);
    EXPECT_EQ(v.y, 2);
    EXPECT_EQ(v.z, 3);

    EXPECT_EQ(v[0], 1);
    EXPECT_EQ(v[1], 2);
    EXPECT_EQ(v[2], 3);
}

TEST(Vec2, vec2_zero_returns_zero) {
    auto v = Vec2<s32>::zero();

    EXPECT_EQ(v.x, 0);
    EXPECT_EQ(v.y, 0);
}


TEST(Vec3, vec3_zero_returns_zero) {
    auto v = Vec3<s32>::zero();

    EXPECT_EQ(v.x, 0);
    EXPECT_EQ(v.y, 0);
    EXPECT_EQ(v.z, 0);
}

TEST(Vec2, vec2_one_returns_one) {
    auto v = Vec2<s32>::one();

    EXPECT_EQ(v.x, 1);
    EXPECT_EQ(v.y, 1);
}


TEST(Vec3, vec3_one_returns_one) {
    auto v = Vec3<s32>::one();

    EXPECT_EQ(v.x, 1);
    EXPECT_EQ(v.y, 1);
    EXPECT_EQ(v.z, 1);
}

TEST(Vec2, can_add) {
    Vec2<s32> a{1, 2};
    Vec2<s32> b{3, 4};
    Vec2<s32> expected{4, 6};

    EXPECT_EQ(a + b, expected);
}


TEST(Vec3, can_add) {
    Vec3<s32> a{1, 2, 3};
    Vec3<s32> b{4, 5, 6};
    Vec3<s32> expected{5, 7, 9};

    EXPECT_EQ(a + b, expected);
}


TEST(Vec2, can_add_assign) {
    Vec2<s32> a{1, 2};
    Vec2<s32> b{3, 4};

    a += b;

    EXPECT_EQ(a.x, 4);
    EXPECT_EQ(a.y, 6);
}


TEST(Vec3, can_add_assign) {
    Vec3<s32> a{1, 2, 3};
    Vec3<s32> b{3, 4, 5};

    a += b;

    EXPECT_EQ(a.x, 4);
    EXPECT_EQ(a.y, 6);
    EXPECT_EQ(a.z, 8);
}

TEST(Vec2, can_subtract) {
    Vec2<s32> a{5, 7};
    Vec2<s32> b{2, 3};
    Vec2<s32> expected{3, 4};

    EXPECT_EQ(a - b, expected);
}


TEST(Vec3, can_subtract) {
    Vec3<s32> a{5, 7, 9};
    Vec3<s32> b{2, 3, 4};
    Vec3<s32> expected{3, 4, 5};

    EXPECT_EQ(a - b, expected);
}


TEST(Vec2, can_subtract_assign) {
    Vec2<s32> a{5, 7};
    Vec2<s32> b{2, 3};

    a -= b;

    EXPECT_EQ(a.x, 3);
    EXPECT_EQ(a.y, 4);
}

TEST(Vec2, can_multiply_scalar) {
    Vec2<s32> v{2, 3};
    Vec2<s32> expected{6, 9};

    EXPECT_EQ(v * 3, expected);
}


TEST(Vec3, can_multiply_scalar) {
    Vec3<s32> v{2, 3, 4};
    Vec3<s32> expected{4, 6, 8};

    EXPECT_EQ(v * 2, expected);
}


TEST(Vec2, can_multiply_scalar_assign) {
    Vec2<s32> v{2, 3};

    v *= 4;

    EXPECT_EQ(v.x, 8);
    EXPECT_EQ(v.y, 12);
}


TEST(Vec3, can_multiply_scalar_assign) {
    Vec3<s32> v{2, 3, 4};

    v *= 3;

    EXPECT_EQ(v.x, 6);
    EXPECT_EQ(v.y, 9);
    EXPECT_EQ(v.z, 12);
}


TEST(Vec2, scalar_can_be_left_side) {
    Vec2<s32> v{2, 3};
    Vec2<s32> expected{6, 9};

    EXPECT_EQ(3 * v, expected);
}

TEST(Vec2, can_divide_scalar) {
    Vec2<s32> v{8, 12};
    Vec2<s32> expected{2, 3};

    EXPECT_EQ(v / 4, expected);
}


TEST(Vec3, can_divide_scalar) {
    Vec3<s32> v{9, 12, 15};
    Vec3<s32> expected{3, 4, 5};

    EXPECT_EQ(v / 3, expected);
}

TEST(Vec2, unary_minus) {
    Vec2<s32> v{1, -2};
    Vec2<s32> expected{-1, 2};

    EXPECT_EQ(-v, expected);
}


TEST(Vec3, unary_minus) {
    Vec3<s32> v{1, -2, 3};
    Vec3<s32> expected{-1, 2, -3};

    EXPECT_EQ(-v, expected);
}

TEST(Vec2, equality) {
    Vec2<s32> s1{1, 2}, s2{1, 2};
    Vec2<s32> s3{1, 2}, s4{2, 2};

    EXPECT_TRUE(s1 == s2);
    EXPECT_FALSE(s3 == s4);
}


TEST(Vec3, inequality) {
    Vec3<s32> s1{1, 2, 3}, s2{1, 2, 4};
    EXPECT_TRUE(s1 != s2);
}

TEST(Vec2, dot_product) {
    Vec2<s32> a{1,2};
    Vec2<s32> b{3,4};

    EXPECT_EQ(dot(a,b), 11);
}


TEST(Vec3, dot_product) {
    Vec3<s32> a{1,2,3};
    Vec3<s32> b{4,5,6};

    EXPECT_EQ(dot(a,b), 32);
}

TEST(Vec2, length_squared) {
    Vec2<s32> v{3,4};

    EXPECT_EQ(length_sq(v), 25);
}


TEST(Vec3, length_squared) {
    Vec3<s32> v{1,2,2};

    EXPECT_EQ(length_sq(v), 9);
}

#endif

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

    force_inline auto backing_array_index_for(u32 row, u32 col, u32 layer) const -> usize {
        return cells_in_layer * layer + row * cols + col;
    }

    auto format() const -> string {
        string result;
        for (u32 layer_index = 0; layer_index < layers; ++layer_index) {
            for (u32 row_index = 0; row_index < rows; ++row_index) {
                result += "  [";
                for (u32 col_index = 0; col_index < cols; ++col_index) {
                    result += sprint("%", at(row_index, col_index, layer_index));
                    if (col_index != cols - 1) result += ", ";
                }
                result += "]\n";
            }
            result += "\n\n";
        }
        return result;
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

TEST(Grid3_bool, can_move_lower_empty_space_below) {
    Grid3 grid(2, 2, 3);
    grid.set(0, 1, 1, true);
    EXPECT_TRUE(grid.can_move_lower(0, 1, 1));
}

TEST(Grid3_bool, cant_move_lower_on_last_layer) {
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

}  // namespace math
