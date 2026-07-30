#pragma once

#include <algorithm>
#include <bit>
#include <tuple>
#include <type_traits>
#include <utility>

#include "basic.hh"
#include "array.hh"
#include "assert.hh"
#include "string_builder.hh"
#include "allocator.hh"

namespace math {

// Bit-level helpers backing sqrt/tan below. Internal to this TU only
// (anonymous namespace), just the frexp/ldexp range-reduction steps that the
// Cephes single-precision routines need, reimplemented with std::bit_cast
// instead of Cephes' original union-based frexpf/ldexpf.
namespace {

// Decompose f into mantissa * 2^exponent, with mantissa in [0.5, 1.0).
constexpr auto frexp_f32(f32 f) -> std::pair<f32, int> {
    if (f == 0.0f) return { 0.0f, 0 };

    u32 bits     = std::bit_cast<u32>(f);
    u32 exp_bits = (bits >> 23) & 0xFFu;

    if (exp_bits == 0) {
        // Subnormal input: not perf-critical, so just re-normalize by
        // scaling up first and correcting the returned exponent after.
        auto [mantissa, exponent] = frexp_f32(f * 16777216.0f);  // * 2^24
        return { mantissa, exponent - 24 };
    }

    if (exp_bits == 0xFFu) return { f, 0 };  // inf/nan: pass through unchanged.

    // f = 1.mantissa * 2^(exp_bits-127) = (1.mantissa/2) * 2^(exp_bits-126).
    int exponent      = static_cast<int>(exp_bits) - 126;
    u32 mantissa_bits = (bits & 0x807FFFFFu) | (126u << 23);  // force exponent field to 126, i.e. 2^-1.

    return { std::bit_cast<f32>(mantissa_bits), exponent };
}

// Compute mantissa * 2^e, undoing frexp_f32's range reduction. Best-effort
// (returns signed zero/infinity) on under/overflow, since that's not a case
// sqrtf/tanf's internal usage below ever actually hits.
constexpr auto ldexp_f32(f32 mantissa, int e) -> f32 {
    if (mantissa == 0.0f) return mantissa;

    u32 bits     = std::bit_cast<u32>(mantissa);
    int exp_bits = static_cast<int>((bits >> 23) & 0xFFu) + e;

    if (exp_bits <= 0)    return std::bit_cast<f32>(bits & 0x80000000u);                   // underflow -> signed zero.
    if (exp_bits >= 0xFF) return std::bit_cast<f32>((bits & 0x80000000u) | (0xFFu << 23)); // overflow  -> signed infinity.

    return std::bit_cast<f32>((bits & 0x807FFFFFu) | (static_cast<u32>(exp_bits) << 23));
}

}  // namespace


force_inline auto abs(s32 x) -> s32 {
    return (x ^ (x >> 31)) - (x >> 31);
}

template <std::floating_point T>
force_inline constexpr auto abs(T x) -> T {
    return x < T(0) ? -x : x;
}

force_inline auto abs_diff(u32 a, u32 b) {
    return (a > b) ? (a - b) : (b - a);
}

force_inline auto lerp(u8 a, u8 b, int pos, int max) -> u8 {
    return a + ((b - a) * pos) / max;
}

force_inline auto floor(f32 x) -> s32 {
    s32 i = static_cast<s32>(x);
    return x < static_cast<f32>(i) ? i - 1 : i;
}

force_inline auto ceil(f32 x) -> s32 {
    s32 i = static_cast<s32>(x);
    return x > static_cast<f32>(i) ? i + 1 : i;
}

// Square root, ported from the Cephes single-precision math library (sqrtf.c,
// Stephen L. Moshier, public domain, netlib.org/cephes). Range-reduces via
// frexp to a minimax polynomial evaluated over [0.5, sqrt(2)], then rescales
// via ldexp.
force_inline auto sqrt(f32 xx) -> f32 {
    if (xx <= 0.0f) return 0.0f;  // Domain error (x < 0) also returns 0, matching Cephes.

    auto [x, e] = frexp_f32(xx);  // xx = x * 2^e, 0.5 <= x < 1.0

    if (e & 1) {  // If the power of 2 is odd, double x and decrement it, so e is even.
        x = x + x;
        e -= 1;
    }
    e >>= 1;  // The power of 2 of the square root.

    f32 y;
    if (x > 1.41421356237f) {
        // x is between sqrt(2) and 2.
        x = x - 2.0f;
        y = (((((-9.8843065718E-4f  * x
              +   7.9479950957E-4f) * x
              -   3.5890535377E-3f) * x
              +   1.1028809744E-2f) * x
              -   4.4195203560E-2f) * x
              +   3.5355338194E-1f) * x
              +   1.41421356237E0f;
    } else if (x > 0.707106781187f) {
        // x is between sqrt(2)/2 and sqrt(2).
        x = x - 1.0f;
        y = (((((1.35199291026E-2f  * x
              -   2.26657767832E-2f) * x
              +   2.78720776889E-2f) * x
              -   3.89582788321E-2f) * x
              +   6.24811144548E-2f) * x
              -   1.25001503933E-1f) * x * x
              +   0.5f * x
              +   1.0f;
    } else {
        // x is between 0.5 and sqrt(2)/2.
        x = x - 0.5f;
        y = (((((-3.9495006054E-1f  * x
              +   5.1743034569E-1f) * x
              -   4.3214437330E-1f) * x
              +   3.5310730460E-1f) * x
              -   3.5354581892E-1f) * x
              +   7.0710676017E-1f) * x
              +   7.07106781187E-1f;
    }

    return ldexp_f32(y, e);
}

// Tangent, ported from the Cephes single-precision math library (tanf.c's
// tancotf with cotflg=0, i.e. cotf support dropped, Stephen L. Moshier, public
// domain, netlib.org/cephes). Range-reduces modulo pi/4 via Cody-Waite
// reduction, then a degree-6 odd minimax polynomial in z^2 for tan(z) on [0, pi/4].
force_inline auto tan(f32 xx) -> f32 {
    constexpr f32 FOPI   = 1.27323954473516f;         // 4 /pi
    constexpr f32 DP1    = 0.78515625f;               // pi/4, split into 3 constants for
    constexpr f32 DP2    = 2.4187564849853515625E-4f; // extended-precision Cody-Waite
    constexpr f32 DP3    = 3.77489497744594108E-8f;   // range reduction.
    constexpr f32 LOSSTH = 8192.0f;

    f32 x    = abs(xx);
    f32 sign = xx < 0.0f ? -1.0f : 1.0f;

    if (x > LOSSTH) return 0.0f;  // Total loss of precision; no error-reporting mechanism here.

    s32 j = static_cast<s32>(FOPI * x);  // Integer part of x / (pi/4).
    f32 y = static_cast<f32>(j);

    if (j & 1) {  // Map zeros and singularities to the origin.
        j += 1;
        y += 1.0f;
    }

    f32 z  = ((x - y * DP1) - y * DP2) - y * DP3;
    f32 zz = z * z;

    if (x > 1.0e-4f) {
        y = (((((9.38540185543E-3f  * zz
              +  3.11992232697E-3f) * zz
              +  2.44301354525E-2f) * zz
              +  5.34112807005E-2f) * zz
              +  1.33387994085E-1f) * zz
              +  3.33331568548E-1f) * zz * z
              +  z;
    } else {
        y = z;
    }

    if (j & 2) y = -1.0f / y;  // Undo the reduction (cotflg=0 branch only).

    return sign < 0.0f ? -y : y;
}

// Sine, ported from the Cephes single-precision math library (sinf.c,
// Stephen L. Moshier, public domain, netlib.org/cephes). Range-reduces
// modulo pi/4 via Cody-Waite reduction, then either a degree-3 odd
// minimax poly in z^2 for sin(z) or a degree-3 even poly for cos(z) on
// [0, pi/4], selected by octant.
force_inline auto sin(f32 xx) -> f32 {
    constexpr f32 FOPI  = 1.27323954473516f;         // 4/pi
    constexpr f32 DP1   = 0.78515625f;               // pi/4, split into 3 constants for
    constexpr f32 DP2   = 2.4187564849853515625E-4f; // extended-precision Cody-Waite
    constexpr f32 DP3   = 3.77489497744594108E-8f;   // range reduction.
    constexpr f32 T24M1 = 16777215.0f;

    f32 sign = 1.0f;
    f32 x    = xx;
    if (xx < 0.0f) {
        sign = -1.0f;
        x    = -xx;
    }

    if (x > T24M1) return 0.0f;  // Total loss of precision; no error-reporting mechanism here.

    s32 j = static_cast<s32>(FOPI * x);  // Integer part of x / (pi/4).
    f32 y = static_cast<f32>(j);

    if (j & 1) {  // Map zeros to the origin.
        j += 1;
        y += 1.0f;
    }

    j &= 7;  // Octant modulo 360 degrees.
    if (j > 3) {  // Reflect in x axis.
        sign = -sign;
        j   -= 4;
    }

    // Extended-precision Cody-Waite reduction.
    f32 reduced = ((x - y * DP1) - y * DP2) - y * DP3;
    f32 z       = reduced * reduced;

    if ((j == 1) || (j == 2)) {
        // cos(z) poly on [0, pi/4].
        y = ((  2.443315711809948E-005f * z
            -   1.388731625493765E-003f) * z
            +   4.166664568298827E-002f) * z * z;
        y = y - 0.5f * z + 1.0f;
    } else {
        // sin(z) poly on [0, pi/4].
        y = ((-1.9515295891E-4f * z
            +  8.3321608736E-3f) * z
            -  1.6666654611E-1f) * z * reduced
            +  reduced;
    }

    return sign < 0.0f ? -y : y;
}

// Cosine, ported from the Cephes single-precision math library (cosf.c,
// Stephen L. Moshier, public domain, netlib.org/cephes). Same reduction and
// polynomials as sin; octant handling differs (cos is even in the input,
// and the sin/cos poly selection is swapped relative to sin).
force_inline auto cos(f32 xx) -> f32 {
    constexpr f32 FOPI  = 1.27323954473516f;         // 4/pi
    constexpr f32 DP1   = 0.78515625f;               // pi/4, split into 3 constants for
    constexpr f32 DP2   = 2.4187564849853515625E-4f; // extended-precision Cody-Waite
    constexpr f32 DP3   = 3.77489497744594108E-8f;   // range reduction.
    constexpr f32 T24M1 = 16777215.0f;

    f32 sign = 1.0f;
    f32 x    = xx < 0.0f ? -xx : xx;  // cos is even.

    if (x > T24M1) return 0.0f;  // Total loss of precision; no error-reporting mechanism here.

    s32 j = static_cast<s32>(FOPI * x);  // Integer part of x / (pi/4).
    f32 y = static_cast<f32>(j);

    if (j & 1) {  // Map zeros to the origin.
        j += 1;
        y += 1.0f;
    }

    j &= 7;  // Octant modulo 360 degrees.
    if (j > 3) {
        j   -= 4;
        sign = -sign;
    }
    if (j > 1) sign = -sign;

    // Extended-precision Cody-Waite reduction.
    f32 reduced = ((x - y * DP1) - y * DP2) - y * DP3;
    f32 z       = reduced * reduced;

    if ((j == 1) || (j == 2)) {
        // sin(z) poly on [0, pi/4].
        y = ((-1.9515295891E-4f * z
            +  8.3321608736E-3f) * z
            -  1.6666654611E-1f) * z * reduced
            +  reduced;
    } else {
        // cos(z) poly on [0, pi/4].
        y = ((  2.443315711809948E-005f * z
            -   1.388731625493765E-003f) * z
            +   4.166664568298827E-002f) * z * z;
        y = y - 0.5f * z + 1.0f;
    }

    return sign < 0.0f ? -y : y;
}

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T>
struct Vector2 {
    using Value_Type = T;
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
    using Value_Type = T;
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
    using Value_Type = T;
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
    [[nodiscard]] static constexpr auto one()  -> Vector4 { return Vector4{ T(1), T(1), T(1), T(1) }; }
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
constexpr auto get(V& v) -> @T(V::Value_Type)& {
    static_assert(I < V::size);
    return v.data[I];
}

template<usize I, IsVector V>
constexpr auto get(const V& v) -> const @T(V::Value_Type)& {
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
[[nodiscard]] constexpr auto operator * (const V& v, @T(V::Value_Type) scalar) -> V {
    V result{};
    for (usize i = 0; i < V::size; ++i) result[i] = v[i] * scalar;
    return result;
}

template <IsVector V>
constexpr auto operator *= (V& a, @T(V::Value_Type) scalar) -> V& {
    for (usize i = 0; i < V::size; ++i) a[i] *= scalar;
    return a;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator * (@T(V::Value_Type) scalar, const V& v) -> V {
    return v * scalar;
}

template <IsVector V>
[[nodiscard]] constexpr auto operator / (const V& v, @T(V::Value_Type) scalar) -> V {
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
[[nodiscard]] force_inline constexpr auto dot(const V& a, const V& b) -> @T(V::Value_Type) {
    using T = @T(V::Value_Type);
    T result{};
    for (usize i = 0; i < V::size; i++) result += a[i] * b[i];
    return result;
}

template <IsVector V>
[[nodiscard]] force_inline constexpr auto length_squared(const V& v) -> @T(V::Value_Type) {
    return dot(v, v);
}

template <IsVector V>
[[nodiscard]] force_inline constexpr auto det_xy(const V& v1, const V& v2) -> @T(V::Value_Type) {
    using T = @T(V::Value_Type);
    return static_cast<T>(v1.x * v2.y - v1.y * v2.x);
}

// 2D "cross" = signed parallelogram area (same as det_xy).
template <Numeric T>
[[nodiscard]] force_inline constexpr auto cross(const Vector2<T>& a, const Vector2<T>& b) -> T {
    return det_xy(a, b);
}

template <Numeric T>
[[nodiscard]] force_inline constexpr auto cross(const Vector3<T>& a, const Vector3<T>& b) -> Vector3<T> {
    return Vector3<T>{
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// In-place normalize. If v is the zero vector (division by zero), v is set
// to fallback instead.
template <IsVector V>
requires std::floating_point<@T(V::Value_Type)>
force_inline auto normalize(V& v, const V& fallback = V::zero()) -> void {
    using T = @T(V::Value_Type);
    T len_sq = length_squared(v);
    if (len_sq == T(0)) {
        v = fallback;
        return;
    }
    f32 len = sqrt(static_cast<f32>(len_sq));
    v = v / static_cast<T>(len);
}

// Distinct type on purpose: quaternions must not pick up IsVector operations
// (component multiplication != Hamilton product), identity is (0,0,0,1) not
// one()/zero(), float-only, and rotations stay unmixed with Vector4 data.
template <std::floating_point T>
struct Quaternion {
    using Value_Type = T;
    T x{}, y{}, z{}, w{};

    [[nodiscard]] static constexpr auto identity() -> Quaternion { return Quaternion{ T(0), T(0), T(0), T(1) }; }
};

#ifdef UNIT_TESTS

template <IsVector V>
void expect_values(const V& v, std::initializer_list<@T(V::Value_Type)> values) {
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

    @T(V::Value_Type) expected = 0;

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

    @T(V::Value_Type) expected = 0;

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

    @T(V::Value_Type) expected = 0;

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

    @T(V::Value_Type) expected = -1;

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

TEST(Vector, cross2) {
    Vector2<f32> a{1, 0};
    Vector2<f32> b{0, 1};

    EXPECT_EQ(cross(a, b), 1.0f);
    EXPECT_EQ(cross(b, a), -1.0f);
    EXPECT_EQ(cross(a, a), 0.0f);
}

TEST(Vector, cross3) {
    Vector3<f32> x{1, 0, 0};
    Vector3<f32> y{0, 1, 0};

    EXPECT_TRUE(cross(x, y) == (Vector3<f32>{0, 0, 1}));
    EXPECT_TRUE(cross(y, x) == (Vector3<f32>{0, 0, -1}));
    EXPECT_TRUE(cross(x, x) == Vector3<f32>::zero());
}

TEST(Vector, normalize2) {
    Vector2<f32> v{3, 4};
    normalize(v);

    EXPECT_NEAR(v.x, 0.6f, 1e-6f);
    EXPECT_NEAR(v.y, 0.8f, 1e-6f);
    EXPECT_NEAR(length_squared(v), 1.0f, 1e-6f);
}

TEST(Vector, normalize3) {
    Vector3<f32> v{3, 0, 4};
    normalize(v);

    EXPECT_NEAR(v.x, 0.6f, 1e-6f);
    EXPECT_NEAR(v.y, 0.0f, 1e-6f);
    EXPECT_NEAR(v.z, 0.8f, 1e-6f);
    EXPECT_NEAR(length_squared(v), 1.0f, 1e-6f);
}

TEST(Vector, normalize4) {
    Vector4<f32> v{0, 3, 0, 4};
    normalize(v);

    EXPECT_NEAR(v.x, 0.0f, 1e-6f);
    EXPECT_NEAR(v.y, 0.6f, 1e-6f);
    EXPECT_NEAR(v.z, 0.0f, 1e-6f);
    EXPECT_NEAR(v.w, 0.8f, 1e-6f);
    EXPECT_NEAR(length_squared(v), 1.0f, 1e-6f);
}

TEST(Vector, normalize_zero_uses_fallback) {
    Vector2<f32> v2{0, 0};
    normalize(v2, Vector2<f32>{0, 1});
    EXPECT_TRUE(v2 == (Vector2<f32>{0, 1}));

    Vector3<f32> v3{0, 0, 0};
    normalize(v3, Vector3<f32>{0, 0, 1});
    EXPECT_TRUE(v3 == (Vector3<f32>{0, 0, 1}));

    Vector4<f32> v4{0, 0, 0, 0};
    normalize(v4, Vector4<f32>{0, 0, 0, 1});
    EXPECT_TRUE(v4 == (Vector4<f32>{0, 0, 0, 1}));
}

TEST(Quaternion, identity) {
    auto q = Quaternion<f32>::identity();

    EXPECT_EQ(q.x, 0.0f);
    EXPECT_EQ(q.y, 0.0f);
    EXPECT_EQ(q.z, 0.0f);
    EXPECT_EQ(q.w, 1.0f);
}

TEST(Math, abs_float) {
    EXPECT_EQ(abs(3.0f), 3.0f);
    EXPECT_EQ(abs(-3.0f), 3.0f);
    EXPECT_EQ(abs(0.0f), 0.0f);
}

TEST(Math, sqrt) {
    EXPECT_EQ(sqrt(0.0f), 0.0f);
    EXPECT_EQ(sqrt(-1.0f), 0.0f);
    EXPECT_NEAR(sqrt(4.0f), 2.0f, 1e-6f);
    EXPECT_NEAR(sqrt(2.0f), 1.41421356f, 1e-6f);
    EXPECT_NEAR(sqrt(0.5f), 0.70710678f, 1e-6f);
    EXPECT_NEAR(sqrt(100.0f), 10.0f, 1e-4f);
    EXPECT_NEAR(sqrt(1.0e10f), 1.0e5f, 1.0f);
}

TEST(Math, tan) {
    constexpr f32 PI_OVER_4 = 0.78539816339f;

    EXPECT_NEAR(tan(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(tan(PI_OVER_4), 1.0f, 1e-5f);
    EXPECT_NEAR(tan(-PI_OVER_4), -1.0f, 1e-5f);
    EXPECT_NEAR(tan(1.0f), 1.55740772f, 1e-5f);
}

TEST(Math, sin) {
    constexpr f32 PI        = 3.14159265359f;
    constexpr f32 PI_OVER_2 = 1.57079632679f;
    constexpr f32 PI_OVER_4 = 0.78539816339f;
    constexpr f32 PI_OVER_6 = 0.52359877559f;

    EXPECT_NEAR(sin(0.0f), 0.0f, 1e-6f);
    EXPECT_NEAR(sin(PI_OVER_6), 0.5f, 1e-5f);
    EXPECT_NEAR(sin(PI_OVER_4), 0.70710678f, 1e-5f);
    EXPECT_NEAR(sin(PI_OVER_2), 1.0f, 1e-5f);
    EXPECT_NEAR(sin(PI), 0.0f, 1e-5f);
    EXPECT_NEAR(sin(-PI_OVER_2), -1.0f, 1e-5f);
    EXPECT_NEAR(sin(1.0f), 0.84147098f, 1e-5f);
}

TEST(Math, cos) {
    constexpr f32 PI        = 3.14159265359f;
    constexpr f32 PI_OVER_2 = 1.57079632679f;
    constexpr f32 PI_OVER_3 = 1.04719755120f;
    constexpr f32 PI_OVER_4 = 0.78539816339f;

    EXPECT_NEAR(cos(0.0f), 1.0f, 1e-6f);
    EXPECT_NEAR(cos(PI_OVER_4), 0.70710678f, 1e-5f);
    EXPECT_NEAR(cos(PI_OVER_3), 0.5f, 1e-5f);
    EXPECT_NEAR(cos(PI_OVER_2), 0.0f, 1e-5f);
    EXPECT_NEAR(cos(PI), -1.0f, 1e-5f);
    EXPECT_NEAR(cos(-PI_OVER_4), 0.70710678f, 1e-5f);
    EXPECT_NEAR(cos(1.0f), 0.54030231f, 1e-5f);
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
        PUSH_ALLOCATOR(&mem::temporary_allocator);
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
