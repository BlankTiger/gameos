#pragma once

#include <algorithm>
#include <tuple>
#include <type_traits>

#include "basic.hh"
#include "array.hh"
#include "assert.hh"
#include "string.hh"

namespace math {

force_inline auto floor(f32 x) -> s32 {
    s32 i = static_cast<s32>(x);
    return x < static_cast<f32>(i) ? i - 1 : i;
}

force_inline auto ceil(f32 x) -> s32 {
    s32 i = static_cast<s32>(x);
    return x > static_cast<f32>(i) ? i + 1 : i;
}

force_inline auto abs(s32 x) -> s32 {
    return (x ^ (x >> 31)) - (x >> 31);
}

force_inline auto abs_diff(u32 a, u32 b) {
    return (a > b) ? (a - b) : (b - a);
}

force_inline auto lerp(u8 a, u8 b, int pos, int max) -> u8 {
    return a + ((b - a) * pos) / max;
}

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T>
struct Vector2 {
    using value_type = T;
    static constexpr auto size = 2;
    union {
        struct { T x, y; };
        T data[size];
    };

    constexpr auto operator [] (usize i) -> T& { return data[i]; }
    constexpr auto operator [] (usize i) const -> const T& { return data[i]; }
    [[nodiscard]] static constexpr auto zero() -> Vector2 { return Vector2{ T(0), T(0) }; }
    [[nodiscard]] static constexpr auto one()  -> Vector2 { return Vector2{ T(1), T(1) }; }
};

template <Numeric T>
struct Vector3 {
    using value_type = T;
    static constexpr auto size = 3;
    union {
        struct { T x, y, z; };
        T data[size];
    };

    constexpr auto operator [] (usize i) -> T& { return data[i]; }
    constexpr auto operator [] (usize i) const -> const T& { return data[i]; }
    [[nodiscard]] static constexpr auto zero() -> Vector3 { return Vector3{ T(0), T(0), T(0) }; }
    [[nodiscard]] static constexpr auto one()  -> Vector3 { return Vector3{ T(1), T(1), T(1) }; }
};

template <Numeric T>
struct Vector4 {
    using value_type = T;
    static constexpr auto size = 4;
    union {
        struct { T x, y, z, w; };
        T data[size];
    };

    constexpr Vector4() {}
    constexpr Vector4(T x, T y, T z, T w): x(x), y(y), z(z), w(w) {}
    constexpr Vector4(const Vector3<T>& v, T w) : x(v.x), y(v.y), z(v.z), w(w) {}

    constexpr auto operator [] (usize i) -> T& { return data[i]; }
    constexpr auto operator [] (usize i) const -> const T& { return data[i]; }
    explicit constexpr operator Vector3<T>() const { return Vector3<T>{ x, y, z }; }
    [[nodiscard]] static constexpr auto zero() -> Vector4 { return Vector4{ T(0), T(0), T(0), T(0) }; }
    [[nodiscard]] static constexpr auto one() -> Vector4 { return Vector4{ T(1), T(1), T(1), T(1) }; }
};

template <typename T>
struct is_vec : std::false_type {};

template <typename T>
struct is_vec<Vector2<T>> : std::true_type {};

template <typename T>
struct is_vec<Vector3<T>> : std::true_type {};

template <typename T>
struct is_vec<Vector4<T>> : std::true_type {};

template <typename T>
concept IsVector = is_vec<std::remove_cvref_t<T>>::value;

template<usize I, IsVector V>
constexpr auto get(V& v) -> typename V::value_type& {
    static_assert(I < V::size);
    return v.data[I];
}

template<usize I, IsVector V>
constexpr auto get(const V& v) -> const typename V::value_type& {
    static_assert(I < V::size);
    return v.data[I];
}

} // namespace math

// Have to define it here, because tests use it
// Alternatively move tests to the bottom
namespace std {
    template<typename T>
    struct tuple_size<math::Vector2<T>> : integral_constant<size_t, 2> {};

    template<usize I, typename T>
    struct tuple_element<I, math::Vector2<T>> {
        using type = T;
    };

    template<typename T>
    struct tuple_size<math::Vector3<T>> : integral_constant<size_t, 3> {};

    template<usize I, typename T>
    struct tuple_element<I, math::Vector3<T>> {
        using type = T;
    };

    template<typename T>
    struct tuple_size<math::Vector4<T>> : integral_constant<size_t, 4> {};

    template<usize I, typename T>
    struct tuple_element<I, math::Vector4<T>> {
        using type = T;
    };
}

namespace math {

template <IsVector V>
[[nodiscard]] constexpr auto operator + (const V& a, const V& b) -> V {
    V result {};
    for (usize i = 0; i < V::size; ++i) result[i] = a[i] + b[i];
    return result;
}

template <IsVector V>
constexpr auto operator += (V& a, const V& b) -> V& {
    for (usize i = 0; i < V::size; ++i) a[i] += b[i];
    return a;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator - (const V& a, const V& b) -> V {
    V result {};
    for (usize i = 0; i < V::size; ++i) result[i] = a[i] - b[i];
    return result;
}

template <IsVector V>
constexpr auto operator -= (V& a, const V& b) -> V& {
    for (usize i = 0; i < V::size; ++i) a[i] -= b[i];
    return a;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator * (const V& v, typename V::value_type scalar) -> V {
    V result{};
    for (usize i = 0; i < V::size; ++i) result[i] = v[i] * scalar;
    return result;
}

template <IsVector V>
constexpr auto operator *= (V& a, typename V::value_type scalar) -> V& {
    for (usize i = 0; i < V::size; ++i) a[i] *= scalar;
    return a;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator * (typename V::value_type scalar, const V& v) -> V {
    return v * scalar;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator / (const V& v, typename V::value_type scalar) -> V {
    V result{};
    for (usize i = 0; i < V::size; i++) result[i] = v[i] / scalar;
    return result;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator - (const V& v) -> V {
    V result{};
    for (usize i = 0; i < V::size; i++) result[i] = -v[i];
    return result;
}

template <IsVector V>
constexpr auto operator == (const V& a, const V& b) -> bool {
    for (usize i = 0; i < V::size; i++) {
        if (a[i] != b[i])
            return false;
    }
    return true;
}


template <IsVector V>
constexpr auto operator != (const V& a, const V& b) -> bool {
    return !(a == b);
}

template <IsVector V>
[[nodiscard]] force_inline constexpr auto dot(const V& a, const V& b) -> typename V::value_type {
    typename V::value_type result{};
    for (usize i = 0; i < V::size; i++) result += a[i] * b[i];
    return result;
}

template <IsVector V>
[[nodiscard]] force_inline constexpr auto length_squared(const V& v) -> typename V::value_type {
    return dot(v, v);
}

template <IsVector V>
[[nodiscard]] force_inline constexpr auto det_xy(const V& v1, const V& v2) -> typename V::value_type {
    return v1.x * v2.y - v1.y * v2.x;
}

#ifdef UNIT_TESTS

template <IsVector V>
void expect_values(const V& v, std::initializer_list<typename V::value_type> values) {
    EXPECT_EQ(V::size, values.size());
    for (int i = 0; i < V::size; i++) {
        EXPECT_EQ(v[i], values.begin()[i]);
    }
}

TEST(Vector, create) {
    expect_values(Vector2<s32>{1,2}, {1,2});
    expect_values(Vector3<s32>{1,2,3}, {1,2,3});
    expect_values(Vector4<s32>{1,2,3,4}, {1,2,3,4});
}

template <IsVector V>
void test_zero() {
    auto v = V::zero();
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(v[i], 0);
}

TEST(Vector, zero) {
    test_zero<Vector2<s32>>();
    test_zero<Vector3<s32>>();
    test_zero<Vector4<s32>>();
    test_zero<Vector2<f32>>();
    test_zero<Vector3<f32>>();
    test_zero<Vector4<f32>>();
}

template <IsVector V>
void test_one() {
    auto v = V::one();
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(v[i], 1);
}


TEST(Vector, one) {
    test_one<Vector2<s32>>();
    test_one<Vector3<s32>>();
    test_one<Vector4<s32>>();
    test_one<Vector2<f32>>();
    test_one<Vector3<f32>>();
    test_one<Vector4<f32>>();
}

template <IsVector V>
void test_add() {
    V a{}, b{};

    for (usize i = 0; i < V::size; ++i) {
        a[i] = i + 1;
        b[i] = i + 2;
    }

    auto result = a + b;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(result[i], (i + 1) + (i + 2));
}

TEST(Vector, add) {
    test_add<Vector2<s32>>();
    test_add<Vector3<s32>>();
    test_add<Vector4<s32>>();
    test_add<Vector2<f32>>();
    test_add<Vector3<f32>>();
    test_add<Vector4<f32>>();
}

template <IsVector V>
void test_add_assign() {
    V a{}, b{};

    for (usize i = 0; i < V::size; ++i) {
        a[i] = i + 1;
        b[i] = i + 2;
    }

    a += b;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(a[i], (i + 1) + (i + 2));
}

TEST(Vector, add_assign) {
    test_add_assign<Vector2<s32>>();
    test_add_assign<Vector3<s32>>();
    test_add_assign<Vector4<s32>>();
    test_add_assign<Vector2<f32>>();
    test_add_assign<Vector3<f32>>();
    test_add_assign<Vector4<f32>>();
}

template <IsVector V>
void test_subtract() {
    V a{}, b{};

    for (usize i = 0; i < V::size; ++i) {
        a[i] = i + 5;
        b[i] = i + 2;
    }

    auto result = a - b;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(result[i], 3);
}

TEST(Vector, subtract) {
    test_subtract<Vector2<s32>>();
    test_subtract<Vector3<s32>>();
    test_subtract<Vector4<s32>>();
    test_subtract<Vector2<f32>>();
    test_subtract<Vector3<f32>>();
    test_subtract<Vector4<f32>>();
}

template <IsVector V>
void test_subtract_assign() {
    V a{}, b{};

    for (usize i = 0; i < V::size; ++i) {
        a[i] = i + 5;
        b[i] = i + 2;
    }

    a -= b;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(a[i], 3);
}

TEST(Vector, subtract_assign) {
    test_subtract_assign<Vector2<s32>>();
    test_subtract_assign<Vector3<s32>>();
    test_subtract_assign<Vector4<s32>>();
    test_subtract_assign<Vector2<f32>>();
    test_subtract_assign<Vector3<f32>>();
    test_subtract_assign<Vector4<f32>>();
}

template <IsVector V>
void test_scalar_multiply() {
    V v{};

    for (usize i = 0; i < V::size; ++i)
        v[i] = i + 1;

    auto result = v * 2;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(result[i], (i + 1) * 2);
}

TEST(Vector, multiply_scalar) {
    test_scalar_multiply<Vector2<s32>>();
    test_scalar_multiply<Vector3<s32>>();
    test_scalar_multiply<Vector4<s32>>();
    test_scalar_multiply<Vector2<f32>>();
    test_scalar_multiply<Vector3<f32>>();
    test_scalar_multiply<Vector4<f32>>();
}

template <IsVector V>
void test_scalar_left_multiply() {
    V v{};

    for (usize i = 0; i < V::size; ++i)
        v[i] = i + 1;

    auto result = 2 * v;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(result[i], (i + 1) * 2);
}

TEST(Vector, scalar_left_multiply) {
    test_scalar_left_multiply<Vector2<s32>>();
    test_scalar_left_multiply<Vector3<s32>>();
    test_scalar_left_multiply<Vector4<s32>>();
    test_scalar_left_multiply<Vector2<f32>>();
    test_scalar_left_multiply<Vector3<f32>>();
    test_scalar_left_multiply<Vector4<f32>>();
}

template <IsVector V>
void test_scalar_multiply_assign() {
    V v{};

    for (usize i = 0; i < V::size; ++i)
        v[i] = i + 1;

    v *= 2;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(v[i], (i + 1) * 2);
}

TEST(Vector, multiply_scalar_assign) {
    test_scalar_multiply_assign<Vector2<s32>>();
    test_scalar_multiply_assign<Vector3<s32>>();
    test_scalar_multiply_assign<Vector4<s32>>();
    test_scalar_multiply_assign<Vector2<f32>>();
    test_scalar_multiply_assign<Vector3<f32>>();
    test_scalar_multiply_assign<Vector4<f32>>();
}

template <IsVector V>
void test_scalar_divide() {
    V v{};

    for (usize i = 0; i < V::size; ++i)
        v[i] = (i + 1) * 2;

    auto result = v / 2;
    for (usize i = 0; i < V::size; ++i) EXPECT_EQ(result[i], i + 1);
}

TEST(Vector, divide_scalar) {
    test_scalar_divide<Vector2<s32>>();
    test_scalar_divide<Vector3<s32>>();
    test_scalar_divide<Vector4<s32>>();
    test_scalar_divide<Vector2<f32>>();
    test_scalar_divide<Vector3<f32>>();
    test_scalar_divide<Vector4<f32>>();
}

template <IsVector V>
void test_unary_minus() {
    V v{};

    for (usize i = 0; i < V::size; ++i)
        v[i] = i + 1;

    auto result = -v;
    for (s32 i = 0; i < V::size; ++i) EXPECT_EQ(result[i], -(i + 1));
}

TEST(Vector, unary_minus) {
    test_unary_minus<Vector2<s32>>();
    test_unary_minus<Vector3<s32>>();
    test_unary_minus<Vector4<s32>>();
    test_unary_minus<Vector2<f32>>();
    test_unary_minus<Vector3<f32>>();
    test_unary_minus<Vector4<f32>>();
}

template <IsVector V>
void test_equality() {
    V a{}, b{}, c{};

    for (usize i = 0; i < V::size; ++i) {
        a[i] = i;
        b[i] = i;
        c[i] = i + 1;
    }

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_TRUE(a != c);
}

TEST(Vector, equality) {
    test_equality<Vector2<s32>>();
    test_equality<Vector3<s32>>();
    test_equality<Vector4<s32>>();
    test_equality<Vector2<f32>>();
    test_equality<Vector3<f32>>();
    test_equality<Vector4<f32>>();
}

template <IsVector V>
void test_dot() {
    V a{}, b{};

    typename V::value_type expected = 0;

    for (usize i = 0; i < V::size; ++i) {
        a[i] = i + 1;
        b[i] = i + 2;
        expected += a[i] * b[i];
    }

    EXPECT_EQ(dot(a, b), expected);
}

TEST(Vector, dot) {
    test_dot<Vector2<s32>>();
    test_dot<Vector3<s32>>();
    test_dot<Vector4<s32>>();
    test_dot<Vector2<f32>>();
    test_dot<Vector3<f32>>();
    test_dot<Vector4<f32>>();
}

template <IsVector V>
void test_length_squared() {
    V v{};

    typename V::value_type expected = 0;

    for (usize i = 0; i < V::size; ++i) {
        v[i] = i + 1;
        expected += v[i] * v[i];
    }

    EXPECT_EQ(length_squared(v), expected);
}

TEST(Vector, length_squared) {
    test_length_squared<Vector2<s32>>();
    test_length_squared<Vector3<s32>>();
    test_length_squared<Vector4<s32>>();
    test_length_squared<Vector2<f32>>();
    test_length_squared<Vector3<f32>>();
    test_length_squared<Vector4<f32>>();
}

TEST(Vector, structured_binding) {
    const auto [x, y] = Vector2<s32>{1, 2};

    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 2);


    const auto [a, b, c] = Vector3<s32>{1, 2, 3};

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 2);
    EXPECT_EQ(c, 3);


    const auto [d, e, f, g] = Vector4<s32>{1, 2, 3, 4};

    EXPECT_EQ(d, 1);
    EXPECT_EQ(e, 2);
    EXPECT_EQ(f, 3);
    EXPECT_EQ(g, 4);
}

TEST(Vector, structured_binding_float) {
    const auto [x, y] = Vector2<f32>{1.0f, 2.0f};

    EXPECT_EQ(x, 1.0f);
    EXPECT_EQ(y, 2.0f);


    const auto [a, b, c] = Vector3<f32>{1.0f, 2.0f, 3.0f};

    EXPECT_EQ(a, 1.0f);
    EXPECT_EQ(b, 2.0f);
    EXPECT_EQ(c, 3.0f);


    const auto [d, e, f, g] = Vector4<f32>{1.0f, 2.0f, 3.0f, 4.0f};

    EXPECT_EQ(d, 1.0f);
    EXPECT_EQ(e, 2.0f);
    EXPECT_EQ(f, 3.0f);
    EXPECT_EQ(g, 4.0f);
}

TEST(Vector4, construct_from_vector3) {
    Vector3<s32> v{1, 2, 3};

    Vector4<s32> result{v, 4};

    EXPECT_EQ(result.x, 1);
    EXPECT_EQ(result.y, 2);
    EXPECT_EQ(result.z, 3);
    EXPECT_EQ(result.w, 4);
}

TEST(Vector4, construct_from_vector3_float) {
    Vector3<f32> v{1.0f, 2.0f, 3.0f};

    Vector4<f32> result{v, 4.0f};

    EXPECT_EQ(result.x, 1.0f);
    EXPECT_EQ(result.y, 2.0f);
    EXPECT_EQ(result.z, 3.0f);
    EXPECT_EQ(result.w, 4.0f);
}


TEST(Vector4, convert_to_vector3) {
    Vector4<s32> v{1, 2, 3, 4};

    Vector3<s32> result = static_cast<Vector3<s32>>(v);

    EXPECT_EQ(result.x, 1);
    EXPECT_EQ(result.y, 2);
    EXPECT_EQ(result.z, 3);
}

TEST(Vector4, convert_to_vector3_float) {
    Vector4<f32> v{1.0f, 2.0f, 3.0f, 4.0f};

    Vector3<f32> result = static_cast<Vector3<f32>>(v);

    EXPECT_EQ(result.x, 1.0f);
    EXPECT_EQ(result.y, 2.0f);
    EXPECT_EQ(result.z, 3.0f);
}

template <IsVector V>
void test_det_xy_zero() {
    V v1{}, v2{};

    typename V::value_type expected = 0;

    for (usize i = 0; i < V::size; ++i) {
        v1[i] = i + 1;
        v2[i] = i + 1;
    }

    EXPECT_EQ(det_xy(v1, v2), expected);
}

TEST(Vector, det_xy_is_zero_on_same_vectors) {
    test_det_xy_zero<Vector2<s32>>();
    test_det_xy_zero<Vector3<s32>>();
    test_det_xy_zero<Vector4<s32>>();
    test_det_xy_zero<Vector2<f32>>();
    test_det_xy_zero<Vector3<f32>>();
    test_det_xy_zero<Vector4<f32>>();
}

template <IsVector V>
void test_det_xy_non_zero() {
    V v1{}, v2{};

    typename V::value_type expected = -1;

    for (usize i = 0; i < V::size; ++i) {
        v1[i] = i + 1;
        v2[i] = i + 2;
    }

    EXPECT_EQ(det_xy(v1, v2), expected);
}

TEST(Vector, can_compute_det_xy) {
    test_det_xy_non_zero<Vector2<s32>>();
    test_det_xy_non_zero<Vector3<s32>>();
    test_det_xy_non_zero<Vector4<s32>>();
    test_det_xy_non_zero<Vector2<f32>>();
    test_det_xy_non_zero<Vector3<f32>>();
    test_det_xy_non_zero<Vector4<f32>>();
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

struct Rect {
    u32 x1 = 0, y1 = 0;
    u32 x2 = 0, y2 = 0;

    constexpr Rect() = default;
    constexpr Rect(u32 x1, u32 y1, u32 x2, u32 y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}

    template<IsVector V>
    constexpr Rect(const V& v1, const V& v2, const V& v3)
    requires(std::is_integral_v<decltype(v1.x)>)
        : x1(static_cast<u32>(std::min({v1.x, v2.x, v3.x}))),
          y1(static_cast<u32>(std::min({v1.y, v2.y, v3.y}))),
          x2(static_cast<u32>(std::max({v1.x, v2.x, v3.x}))),
          y2(static_cast<u32>(std::max({v1.y, v2.y, v3.y}))) {}

    template<IsVector V>
    constexpr Rect(const V& v1, const V& v2, const V& v3)
    requires(std::is_floating_point_v<decltype(v1.x)>)
        : x1(static_cast<u32>(floor(std::min({v1.x, v2.x, v3.x})))),
          y1(static_cast<u32>(floor(std::min({v1.y, v2.y, v3.y})))),
          x2(static_cast<u32>(ceil(std::max({v1.x, v2.x, v3.x})))),
          y2(static_cast<u32>(ceil(std::max({v1.y, v2.y, v3.y})))) {}

    static constexpr auto from_size(u32 x, u32 y, u32 w, u32 h) -> Rect {
        return Rect{ x, y, x + w, y + h };
    }

    constexpr auto width()  const -> u32 { return x2 - x1; }
    constexpr auto height() const -> u32 { return y2 - y1; }
    constexpr auto empty()  const -> u32 { return x1 >= x2 || y1 >= y2; }

    force_inline void clip(u32 screen_width, u32 screen_height) {
        x1 = std::clamp(x1, static_cast<u32>(0), screen_width);
        x2 = std::clamp(x2, static_cast<u32>(0), screen_width);
        y1 = std::clamp(y1, static_cast<u32>(0), screen_height);
        y2 = std::clamp(y2, static_cast<u32>(0), screen_height);
    }
};

}  // namespace math
