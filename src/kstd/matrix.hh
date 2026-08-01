#pragma once

#include <algorithm>
#include <concepts>
#include <optional>

#include "basic.hh"
#include "math.hh"

namespace math {

template <std::floating_point T>
struct Matrix2 {
    using Value_Type  = T;
    using Row_Type    = Vector2<T>;
    using Column_Type = Vector2<T>;
    static constexpr usize rows = 2;
    static constexpr usize cols = 2;
    static constexpr usize size = rows * cols;

    union {
        struct { T _11, _12, _21, _22; };
        Vector2<T> v[rows];  // Row vectors.
        T coef[rows][cols];
        T floats[size] = {};
    };

    [[nodiscard]] static constexpr auto identity() -> Matrix2 {
        Matrix2 m{};
        m._11 = T(1);
        m._22 = T(1);
        return m;
    }
};

template <std::floating_point T>
struct Matrix3 {
    using Value_Type  = T;
    using Row_Type    = Vector3<T>;
    using Column_Type = Vector3<T>;
    static constexpr usize rows = 3;
    static constexpr usize cols = 3;
    static constexpr usize size = rows * cols;

    union {
        struct { T _11, _12, _13, _21, _22, _23, _31, _32, _33; };
        Vector3<T> v[rows];  // Row vectors.
        T coef[rows][cols];
        T floats[size] = {};
    };

    [[nodiscard]] static constexpr auto identity() -> Matrix3 {
        Matrix3 m{};
        m._11 = T(1);
        m._22 = T(1);
        m._33 = T(1);
        return m;
    }
};

template <std::floating_point T>
struct Matrix4 {
    using Value_Type  = T;
    using Row_Type    = Vector4<T>;
    using Column_Type = Vector4<T>;
    static constexpr usize rows = 4;
    static constexpr usize cols = 4;
    static constexpr usize size = rows * cols;

    union {
        struct { T _11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34, _41, _42, _43, _44; };
        Vector4<T> v[rows];  // Row vectors.
        T coef[rows][cols];
        T floats[size] = {};
    };

    [[nodiscard]] static constexpr auto identity() -> Matrix4 {
        Matrix4 m{};
        m._11 = T(1);
        m._22 = T(1);
        m._33 = T(1);
        m._44 = T(1);
        return m;
    }
};

// Used when a matrix is mathematically a Matrix4, but the bottom row is
// known to be [0, 0, 0, 1] (a common case for affine transforms). Storage
// for that row is dropped, saving memory and extra multiplies/adds.
template <std::floating_point T>
struct Matrix4x3 {
    using Value_Type  = T;
    using Row_Type    = Vector4<T>;
    using Column_Type = Vector3<T>;
    static constexpr usize rows = 3;
    static constexpr usize cols = 4;
    static constexpr usize size = rows * cols;

    union {
        struct { T _11, _12, _13, _14, _21, _22, _23, _24, _31, _32, _33, _34; };
        Vector4<T> v[rows];  // Row vectors.
        T coef[rows][cols];
        T floats[size] = {};
    };

    [[nodiscard]] static constexpr auto identity() -> Matrix4x3 {
        Matrix4x3 m{};
        m._11 = T(1);
        m._22 = T(1);
        m._33 = T(1);
        return m;
    }
};

template <typename T>
struct is_matrix : std::false_type {};

template <std::floating_point T>
struct is_matrix<Matrix2<T>> : std::true_type {};

template <std::floating_point T>
struct is_matrix<Matrix3<T>> : std::true_type {};

template <std::floating_point T>
struct is_matrix<Matrix4<T>> : std::true_type {};

template <std::floating_point T>
struct is_matrix<Matrix4x3<T>> : std::true_type {};

template <typename T>
concept IsMatrix = is_matrix<std::remove_cvref_t<T>>::value;

template <typename T>
concept AnyMatrix3x3 = IsMatrix<T> && (T::rows >= 3) && (T::cols >= 3);

template <typename T>
concept AnyMatrix4x3Ish = IsMatrix<T> && (T::rows >= 3) && (T::cols >= 4);

template <std::floating_point T>
constexpr auto row(const Matrix4<T>& m, usize i) -> Vector4<T> {
    return m.v[i];
}

template <std::floating_point T>
constexpr auto column(const Matrix4<T>& m, usize i) -> Vector4<T> {
    return Vector4<T>{ m.coef[0][i], m.coef[1][i], m.coef[2][i], m.coef[3][i] };
}

// Separate overloads: Matrix4x3*Vector4 -> Vector3, not Vector4.

template <std::floating_point T>
constexpr auto multiply(const Matrix4<T>& m, const Vector4<T>& v) -> Vector4<T> {
    return Vector4<T>{
        m._11 * v.x + m._12 * v.y + m._13 * v.z + m._14 * v.w,
        m._21 * v.x + m._22 * v.y + m._23 * v.z + m._24 * v.w,
        m._31 * v.x + m._32 * v.y + m._33 * v.z + m._34 * v.w,
        m._41 * v.x + m._42 * v.y + m._43 * v.z + m._44 * v.w
    };
}

template <std::floating_point T>
constexpr auto multiply(const Matrix4x3<T>& m, const Vector4<T>& v) -> Vector3<T> {
    return Vector3<T>{
        m._11 * v.x + m._12 * v.y + m._13 * v.z + m._14 * v.w,
        m._21 * v.x + m._22 * v.y + m._23 * v.z + m._24 * v.w,
        m._31 * v.x + m._32 * v.y + m._33 * v.z + m._34 * v.w
    };
}

template <std::floating_point T>
constexpr auto multiply(const Matrix2<T>& m, const Vector2<T>& v) -> Vector2<T> {
    return Vector2<T>{
        m._11 * v.x + m._12 * v.y,
        m._21 * v.x + m._22 * v.y
    };
}

template <AnyMatrix3x3 M>
constexpr auto multiply(const M& m, const Vector3<@T(M::Value_Type)>& v) -> Vector3<@T(M::Value_Type)> {
    return Vector3<@T(M::Value_Type)>{
        m._11 * v.x + m._12 * v.y + m._13 * v.z,
        m._21 * v.x + m._22 * v.y + m._23 * v.z,
        m._31 * v.x + m._32 * v.y + m._33 * v.z
    };
}

template <std::floating_point T>
constexpr auto operator * (const Matrix4<T>& m, const Vector4<T>& v) -> Vector4<T> { return multiply(m, v); }

template <std::floating_point T>
constexpr auto operator * (const Matrix4x3<T>& m, const Vector4<T>& v) -> Vector3<T> { return multiply(m, v); }

template <std::floating_point T>
constexpr auto operator * (const Matrix2<T>& m, const Vector2<T>& v) -> Vector2<T> { return multiply(m, v); }

template <std::floating_point T>
constexpr auto operator * (const Matrix3<T>& m, const Vector3<T>& v) -> Vector3<T> { return multiply(m, v); }

template <AnyMatrix4x3Ish M>
constexpr auto transform_point(const M& m, const Vector3<@T(M::Value_Type)>& v) -> Vector3<@T(M::Value_Type)> {
    return Vector3<@T(M::Value_Type)>{
        m._11 * v.x + m._12 * v.y + m._13 * v.z + m._14,
        m._21 * v.x + m._22 * v.y + m._23 * v.z + m._24,
        m._31 * v.x + m._32 * v.y + m._33 * v.z + m._34
    };
}

template <AnyMatrix4x3Ish M>
constexpr auto transform_point(const M& m, const Vector4<@T(M::Value_Type)>& v) -> Vector4<@T(M::Value_Type)> {
    using T = @T(M::Value_Type);
    if constexpr (M::rows == 4) {
        return multiply(m, v);
    } else {
        return Vector4<T>{
            m._11 * v.x + m._12 * v.y + m._13 * v.z + m._14 * v.w,
            m._21 * v.x + m._22 * v.y + m._23 * v.z + m._24 * v.w,
            m._31 * v.x + m._32 * v.y + m._33 * v.z + m._34 * v.w,
            v.w
        };
    }
}

template <AnyMatrix3x3 M>
constexpr auto transform_vector(const M& m, const Vector3<@T(M::Value_Type)>& v) -> Vector3<@T(M::Value_Type)> {
    return multiply(m, v);
}

template <std::floating_point T>
constexpr auto multiply(const Matrix2<T>& m, const Matrix2<T>& n) -> Matrix2<T> {
    Matrix2<T> result{};

    result._11 = m._11 * n._11 + m._12 * n._21;
    result._21 = m._21 * n._11 + m._22 * n._21;

    result._12 = m._11 * n._12 + m._12 * n._22;
    result._22 = m._21 * n._12 + m._22 * n._22;

    return result;
}

template <std::floating_point T>
constexpr auto multiply(const Matrix3<T>& m, const Matrix3<T>& n) -> Matrix3<T> {
    Matrix3<T> result{};

    result._11 = m._11*n._11 + m._12*n._21 + m._13*n._31;
    result._21 = m._21*n._11 + m._22*n._21 + m._23*n._31;
    result._31 = m._31*n._11 + m._32*n._21 + m._33*n._31;

    result._12 = m._11*n._12 + m._12*n._22 + m._13*n._32;
    result._22 = m._21*n._12 + m._22*n._22 + m._23*n._32;
    result._32 = m._31*n._12 + m._32*n._22 + m._33*n._32;

    result._13 = m._11*n._13 + m._12*n._23 + m._13*n._33;
    result._23 = m._21*n._13 + m._22*n._23 + m._23*n._33;
    result._33 = m._31*n._13 + m._32*n._23 + m._33*n._33;

    return result;
}

template <std::floating_point T>
constexpr auto multiply(const Matrix4<T>& m, const Matrix3<T>& n) -> Matrix4<T> {
    Matrix4<T> result{};

    result._11 = m._11*n._11 + m._12*n._21 + m._13*n._31;
    result._21 = m._21*n._11 + m._22*n._21 + m._23*n._31;
    result._31 = m._31*n._11 + m._32*n._21 + m._33*n._31;
    result._41 = m._41*n._11 + m._42*n._21 + m._43*n._31;

    result._12 = m._11*n._12 + m._12*n._22 + m._13*n._32;
    result._22 = m._21*n._12 + m._22*n._22 + m._23*n._32;
    result._32 = m._31*n._12 + m._32*n._22 + m._33*n._32;
    result._42 = m._41*n._12 + m._42*n._22 + m._43*n._32;

    result._13 = m._11*n._13 + m._12*n._23 + m._13*n._33;
    result._23 = m._21*n._13 + m._22*n._23 + m._23*n._33;
    result._33 = m._31*n._13 + m._32*n._23 + m._33*n._33;
    result._43 = m._41*n._13 + m._42*n._23 + m._43*n._33;

    result._14 = m._14;
    result._24 = m._24;
    result._34 = m._34;
    result._44 = m._44;

    return result;
}

template <std::floating_point T>
constexpr auto multiply(const Matrix4<T>& m, const Matrix4<T>& n) -> Matrix4<T> {
    Matrix4<T> result{};

    result._11 = m._11*n._11 + m._12*n._21 + m._13*n._31 + m._14*n._41;
    result._21 = m._21*n._11 + m._22*n._21 + m._23*n._31 + m._24*n._41;
    result._31 = m._31*n._11 + m._32*n._21 + m._33*n._31 + m._34*n._41;
    result._41 = m._41*n._11 + m._42*n._21 + m._43*n._31 + m._44*n._41;

    result._12 = m._11*n._12 + m._12*n._22 + m._13*n._32 + m._14*n._42;
    result._22 = m._21*n._12 + m._22*n._22 + m._23*n._32 + m._24*n._42;
    result._32 = m._31*n._12 + m._32*n._22 + m._33*n._32 + m._34*n._42;
    result._42 = m._41*n._12 + m._42*n._22 + m._43*n._32 + m._44*n._42;

    result._13 = m._11*n._13 + m._12*n._23 + m._13*n._33 + m._14*n._43;
    result._23 = m._21*n._13 + m._22*n._23 + m._23*n._33 + m._24*n._43;
    result._33 = m._31*n._13 + m._32*n._23 + m._33*n._33 + m._34*n._43;
    result._43 = m._41*n._13 + m._42*n._23 + m._43*n._33 + m._44*n._43;

    result._14 = m._11*n._14 + m._12*n._24 + m._13*n._34 + m._14*n._44;
    result._24 = m._21*n._14 + m._22*n._24 + m._23*n._34 + m._24*n._44;
    result._34 = m._31*n._14 + m._32*n._24 + m._33*n._34 + m._34*n._44;
    result._44 = m._41*n._14 + m._42*n._24 + m._43*n._34 + m._44*n._44;

    return result;
}

template <std::floating_point T>
constexpr auto multiply(const Matrix4x3<T>& m, const Matrix4x3<T>& n) -> Matrix4x3<T> {
    Matrix4x3<T> result{};

    result._11 = m._11*n._11 + m._12*n._21 + m._13*n._31;
    result._21 = m._21*n._11 + m._22*n._21 + m._23*n._31;
    result._31 = m._31*n._11 + m._32*n._21 + m._33*n._31;

    result._12 = m._11*n._12 + m._12*n._22 + m._13*n._32;
    result._22 = m._21*n._12 + m._22*n._22 + m._23*n._32;
    result._32 = m._31*n._12 + m._32*n._22 + m._33*n._32;

    result._13 = m._11*n._13 + m._12*n._23 + m._13*n._33;
    result._23 = m._21*n._13 + m._22*n._23 + m._23*n._33;
    result._33 = m._31*n._13 + m._32*n._23 + m._33*n._33;

    result._14 = m._11*n._14 + m._12*n._24 + m._13*n._34 + m._14;
    result._24 = m._21*n._14 + m._22*n._24 + m._23*n._34 + m._24;
    result._34 = m._31*n._14 + m._32*n._24 + m._33*n._34 + m._34;

    return result;
}

template <std::floating_point T>
constexpr auto multiply(const Matrix4<T>& m, const Matrix4x3<T>& n) -> Matrix4<T> {
    Matrix4<T> result{};

    result._11 = m._11*n._11 + m._12*n._21 + m._13*n._31;
    result._21 = m._21*n._11 + m._22*n._21 + m._23*n._31;
    result._31 = m._31*n._11 + m._32*n._21 + m._33*n._31;
    result._41 = m._41*n._11 + m._42*n._21 + m._43*n._31;

    result._12 = m._11*n._12 + m._12*n._22 + m._13*n._32;
    result._22 = m._21*n._12 + m._22*n._22 + m._23*n._32;
    result._32 = m._31*n._12 + m._32*n._22 + m._33*n._32;
    result._42 = m._41*n._12 + m._42*n._22 + m._43*n._32;

    result._13 = m._11*n._13 + m._12*n._23 + m._13*n._33;
    result._23 = m._21*n._13 + m._22*n._23 + m._23*n._33;
    result._33 = m._31*n._13 + m._32*n._23 + m._33*n._33;
    result._43 = m._41*n._13 + m._42*n._23 + m._43*n._33;

    result._14 = m._11*n._14 + m._12*n._24 + m._13*n._34 + m._14;
    result._24 = m._21*n._14 + m._22*n._24 + m._23*n._34 + m._24;
    result._34 = m._31*n._14 + m._32*n._24 + m._33*n._34 + m._34;
    result._44 = m._41*n._14 + m._42*n._24 + m._43*n._34 + m._44;

    return result;
}

template <std::floating_point T>
constexpr auto multiply(const Matrix4x3<T>& m, const Matrix4<T>& n) -> Matrix4<T> {
    Matrix4<T> result{};

    result._11 = m._11*n._11 + m._12*n._21 + m._13*n._31 + m._14*n._41;
    result._21 = m._21*n._11 + m._22*n._21 + m._23*n._31 + m._24*n._41;
    result._31 = m._31*n._11 + m._32*n._21 + m._33*n._31 + m._34*n._41;
    result._41 = n._41;

    result._12 = m._11*n._12 + m._12*n._22 + m._13*n._32 + m._14*n._42;
    result._22 = m._21*n._12 + m._22*n._22 + m._23*n._32 + m._24*n._42;
    result._32 = m._31*n._12 + m._32*n._22 + m._33*n._32 + m._34*n._42;
    result._42 = n._42;

    result._13 = m._11*n._13 + m._12*n._23 + m._13*n._33 + m._14*n._43;
    result._23 = m._21*n._13 + m._22*n._23 + m._23*n._33 + m._24*n._43;
    result._33 = m._31*n._13 + m._32*n._23 + m._33*n._33 + m._34*n._43;
    result._43 = n._43;

    result._14 = m._11*n._14 + m._12*n._24 + m._13*n._34 + m._14*n._44;
    result._24 = m._21*n._14 + m._22*n._24 + m._23*n._34 + m._24*n._44;
    result._34 = m._31*n._14 + m._32*n._24 + m._33*n._34 + m._34*n._44;
    result._44 = n._44;

    return result;
}

template <std::floating_point T>
constexpr auto operator * (const Matrix2<T>& a, const Matrix2<T>& b) -> Matrix2<T> { return multiply(a, b); }

template <std::floating_point T>
constexpr auto operator * (const Matrix3<T>& a, const Matrix3<T>& b) -> Matrix3<T> { return multiply(a, b); }

template <std::floating_point T>
constexpr auto operator * (const Matrix4<T>& a, const Matrix4<T>& b) -> Matrix4<T> { return multiply(a, b); }

template <std::floating_point T>
constexpr auto operator * (const Matrix4x3<T>& a, const Matrix4x3<T>& b) -> Matrix4x3<T> { return multiply(a, b); }

template <std::floating_point T>
constexpr auto operator * (const Matrix4<T>& a, const Matrix4x3<T>& b) -> Matrix4<T> { return multiply(a, b); }

template <std::floating_point T>
constexpr auto operator * (const Matrix4x3<T>& a, const Matrix4<T>& b) -> Matrix4<T> { return multiply(a, b); }

template <std::floating_point T>
constexpr auto operator * (const Matrix4<T>& a, const Matrix3<T>& b) -> Matrix4<T> { return multiply(a, b); }

template <IsMatrix M>
constexpr auto operator * (const M& a, @T(M::Value_Type) t) -> M {
    M r{};
    for (usize i = 0; i < M::size; ++i) r.floats[i] = a.floats[i] * t;
    return r;
}

template <IsMatrix M>
constexpr auto operator * (@T(M::Value_Type) t, const M& a) -> M {
    return a * t;
}

template <IsMatrix M>
constexpr auto operator + (const M& a, const M& b) -> M {
    M r{};
    for (usize i = 0; i < M::size; ++i) r.floats[i] = a.floats[i] + b.floats[i];
    return r;
}

template <IsMatrix M>
constexpr auto operator - (const M& a, const M& b) -> M {
    M r{};
    for (usize i = 0; i < M::size; ++i) r.floats[i] = a.floats[i] - b.floats[i];
    return r;
}

template <IsMatrix M>
constexpr auto operator == (const M& a, const M& b) -> bool {
    for (usize i = 0; i < M::size; ++i) {
        if (a.floats[i] != b.floats[i]) return false;
    }
    return true;
}

template <AnyMatrix3x3 M>
constexpr auto make_matrix3(const M& m) -> Matrix3<@T(M::Value_Type)> {
    Matrix3<@T(M::Value_Type)> r{};
    for (usize i = 0; i < 3; ++i)
        for (usize j = 0; j < 3; ++j)
            r.coef[i][j] = m.coef[i][j];
    return r;
}

template <AnyMatrix3x3 M>
constexpr auto make_matrix4(const M& m) -> Matrix4<@T(M::Value_Type)> {
    auto r = Matrix4<@T(M::Value_Type)>::identity();

    constexpr usize row_max = std::min<usize>(3, M::rows - 1);  // Index of the highest row (0-based).
    constexpr usize col_max = std::min<usize>(3, M::cols - 1);  // Index of the highest column (0-based).

    for (usize i = 0; i <= row_max; ++i)
        for (usize j = 0; j <= col_max; ++j)
            r.coef[i][j] = m.coef[i][j];

    return r;
}

template <std::floating_point T>
constexpr auto transpose(const Matrix4<T>& m) -> Matrix4<T> {
    Matrix4<T> r{};
    for (usize i = 0; i < 4; ++i)
        for (usize j = 0; j < 4; ++j)
            r.coef[i][j] = m.coef[j][i];
    return r;
}

template <std::floating_point T>
constexpr auto transpose(const Matrix3<T>& m) -> Matrix3<T> {
    Matrix3<T> r{};
    for (usize i = 0; i < 3; ++i)
        for (usize j = 0; j < 3; ++j)
            r.coef[i][j] = m.coef[j][i];
    return r;
}

template <std::floating_point T>
constexpr auto transpose(Matrix4<T>* m) -> void {
    std::swap(m->coef[0][1], m->coef[1][0]);
    std::swap(m->coef[0][2], m->coef[2][0]);
    std::swap(m->coef[0][3], m->coef[3][0]);

    std::swap(m->coef[1][2], m->coef[2][1]);
    std::swap(m->coef[1][3], m->coef[3][1]);

    std::swap(m->coef[2][3], m->coef[3][2]);
}

template <std::floating_point T>
constexpr auto transpose(Matrix3<T>* m) -> void {
    std::swap(m->coef[0][1], m->coef[1][0]);
    std::swap(m->coef[0][2], m->coef[2][0]);
    std::swap(m->coef[1][2], m->coef[2][1]);
}

template <std::floating_point T>
constexpr auto make_scale_matrix4(const Vector3<T>& v) -> Matrix4<T> {
    auto m = Matrix4<T>::identity();
    m._11 = v.x;
    m._22 = v.y;
    m._33 = v.z;
    return m;
}

template <AnyMatrix3x3 M>
constexpr auto scale(M* m, @T(M::Value_Type) s) -> void {
    m->_11 *= s; m->_21 *= s; m->_31 *= s;
    m->_12 *= s; m->_22 *= s; m->_32 *= s;
    m->_13 *= s; m->_23 *= s; m->_33 *= s;
}

template <AnyMatrix3x3 M>
constexpr auto scale(M m, @T(M::Value_Type) s) -> M {
    scale(&m, s);
    return m;
}

template <AnyMatrix3x3 M>
constexpr auto scale(M* m, const Vector3<@T(M::Value_Type)>& v) -> void {
    m->_11 *= v.x; m->_21 *= v.x; m->_31 *= v.x;
    m->_12 *= v.y; m->_22 *= v.y; m->_32 *= v.y;
    m->_13 *= v.z; m->_23 *= v.z; m->_33 *= v.z;
}

template <AnyMatrix3x3 M>
constexpr auto scale(M m, const Vector3<@T(M::Value_Type)>& v) -> M {
    scale(&m, v);
    return m;
}

template <std::floating_point T>
constexpr auto make_translation_matrix4(const Vector3<T>& v) -> Matrix4<T> {
    auto m = Matrix4<T>::identity();
    m._14 = v.x;
    m._24 = v.y;
    m._34 = v.z;
    return m;
}

template <std::floating_point T>
constexpr auto translate(Matrix4<T>* m, const Vector3<T>& t) -> void {
    m->coef[0][3] += m->coef[0][0]*t.x + m->coef[0][1]*t.y + m->coef[0][2]*t.z;
    m->coef[1][3] += m->coef[1][0]*t.x + m->coef[1][1]*t.y + m->coef[1][2]*t.z;
    m->coef[2][3] += m->coef[2][0]*t.x + m->coef[2][1]*t.y + m->coef[2][2]*t.z;
    m->coef[3][3] += m->coef[3][0]*t.x + m->coef[3][1]*t.y + m->coef[3][2]*t.z;
}

template <std::floating_point T>
constexpr auto translate(Matrix4<T> m, const Vector3<T>& t) -> Matrix4<T> {
    translate(&m, t);
    return m;
}

template <typename Result_Type>
constexpr auto make_rotation_matrix(const Quaternion<@T(Result_Type::Value_Type)>& q) -> Result_Type {
    using T = @T(Result_Type::Value_Type);
    Result_Type m{};

    if constexpr (std::same_as<Result_Type, Matrix4<T>>) m._44 = T(1);

    T xs = q.x * T(2);
    T ys = q.y * T(2);
    T zs = q.z * T(2);

    T wx = q.w * xs;
    T wy = q.w * ys;
    T wz = q.w * zs;

    T xx = q.x * xs;
    T xy = q.x * ys;
    T xz = q.x * zs;

    T yy = q.y * ys;
    T yz = q.y * zs;
    T zz = q.z * zs;

    m._11 = T(1) - (yy + zz);
    m._12 = xy - wz;
    m._13 = xz + wy;

    m._21 = xy + wz;
    m._22 = T(1) - (xx + zz);
    m._23 = yz - wx;

    m._31 = xz - wy;
    m._32 = yz + wx;
    m._33 = T(1) - (xx + yy);

    return m;
}

template <std::floating_point T>
constexpr auto rotate(const Matrix3<T>& m, const Quaternion<T>& q) -> Matrix3<T> {
    Matrix3<T> r = make_rotation_matrix<Matrix3<T>>(q);
    return m * r;
}

template <std::floating_point T>
constexpr auto rotate(const Matrix4<T>& m, const Quaternion<T>& q) -> Matrix4<T> {
    Matrix3<T> r = make_rotation_matrix<Matrix3<T>>(q);
    return m * r;
}

// make_matrix_from_rows()/make_matrix_from_columns() are convenience
// functions useful when building matrices that transform from one space to
// another: put the basis vectors in one matrix as rows, in the other as
// columns, then multiply them together.

template <typename Result_Type, std::floating_point T>
constexpr auto make_matrix_from_rows(const Vector3<T>& xprime, const Vector3<T>& yprime, const Vector3<T>& zprime) -> Result_Type {
    Result_Type result{};

    result._11 = xprime.x;
    result._12 = xprime.y;
    result._13 = xprime.z;

    result._21 = yprime.x;
    result._22 = yprime.y;
    result._23 = yprime.z;

    result._31 = zprime.x;
    result._32 = zprime.y;
    result._33 = zprime.z;

    if constexpr (std::same_as<Result_Type, Matrix4<T>>) result._44 = T(1);

    return result;
}

template <std::floating_point T>
constexpr auto make_matrix_from_rows(const Vector3<T>& xprime, const Vector3<T>& yprime, const Vector3<T>& zprime) -> Matrix4<T> {
    return make_matrix_from_rows<Matrix4<T>>(xprime, yprime, zprime);
}

template <typename Result_Type, std::floating_point T>
constexpr auto make_matrix_from_columns(const Vector3<T>& xprime, const Vector3<T>& yprime, const Vector3<T>& zprime) -> Result_Type {
    Result_Type result{};

    result._11 = xprime.x;
    result._21 = xprime.y;
    result._31 = xprime.z;

    result._12 = yprime.x;
    result._22 = yprime.y;
    result._32 = yprime.z;

    result._13 = zprime.x;
    result._23 = zprime.y;
    result._33 = zprime.z;

    if constexpr (std::same_as<Result_Type, Matrix4<T>>) result._44 = T(1);

    return result;
}

template <std::floating_point T>
constexpr auto make_matrix_from_columns(const Vector3<T>& xprime, const Vector3<T>& yprime, const Vector3<T>& zprime) -> Matrix4<T> {
    return make_matrix_from_columns<Matrix4<T>>(xprime, yprime, zprime);
}

template <bool x_is_forward = true, std::floating_point T>
constexpr auto make_look_at_matrix(const Vector3<T>& viewpoint, const Vector3<T>& look_at, const Vector3<T>& reference_up_vector) -> Matrix4<T> {
    Vector3<T> forward = look_at - viewpoint;
    normalize(forward, Vector3<T>{ T(0), T(0), T(1) });

    Vector3<T> left = cross(reference_up_vector, forward);
    normalize(left, Vector3<T>{ T(0), T(0), T(1) });

    Vector3<T> up = cross(forward, left);
    normalize(up, Vector3<T>{ T(0), T(0), T(1) });  // Accuracy.

    // Inverse of transform mapping axis_forward -> forward: rows = inverse
    // rotation, then translate by -viewpoint.
    Matrix4<T> rotation;
    if constexpr (x_is_forward) {
        rotation = make_matrix_from_rows<Matrix4<T>>(forward, left, up);
    } else {
        rotation = make_matrix_from_rows<Matrix4<T>>(left * T(-1), up, forward * T(-1));
    }

    return translate(rotation, -viewpoint);
}

template <std::floating_point T>
constexpr auto orthographic_projection_matrix(T left, T right, T bottom, T top, T near, T far, bool depth_range_01 = false) -> Matrix4<T> {
    Matrix4<T> m{};

    m._11 = T(2) / (right - left);
    m._14 = -(right + left) / (right - left);

    m._22 = T(2) / (top - bottom);
    m._24 = -(top + bottom) / (top - bottom);

    m._33 = T(-2) / (far - near);
    m._34 = -(far + near) / (far - near);
    m._44 = T(1);

    if (depth_range_01) {
        // Map the -1,1 depth range to 0,1: z' = z * 0.5 + 0.5.
        m._33 = m._33 * T(0.5) + m._43 * T(0.5);
        m._34 = m._34 * T(0.5) + m._44 * T(0.5);
    }

    return m;
}

// Minus-z-forward projection (GL-style). Engine is x-forward; fixup
// projection elsewhere to match x-forward view matrices.
template <std::floating_point T>
constexpr auto make_projection_matrix(T fov_vertical, T aspect_ratio_horizontal_over_vertical, T z_near, T z_far, T x_offset = T(0), T y_offset = T(0), bool depth_range_01 = false) -> Matrix4<T> {
    auto result = Matrix4<T>::identity();

    T tan_theta = static_cast<T>(tan(static_cast<f32>(fov_vertical * T(0.5))));
    T cot_theta = T(1) / tan_theta;

    T f     = z_far;
    T n     = z_near;
    T denom = T(1) / (f - n);

    result._11 = cot_theta / aspect_ratio_horizontal_over_vertical;
    result._22 = cot_theta;
    result._33 = -(f + n) * denom;
    result._43 = T(-1);
    result._34 = T(-2) * f * n * denom;
    result._44 = T(0);

    result._13 = x_offset;
    result._23 = y_offset;

    if (depth_range_01) {
        // Map the -1,1 depth range to 0,1: z' = z * 0.5 + 0.5.
        result._33 = result._33 * T(0.5) + result._43 * T(0.5);
        result._34 = result._34 * T(0.5) + result._44 * T(0.5);
    }

    return result;
}

// RH perspective. Near -> z=-1, far -> z=1. Looks down -Z.
template <std::floating_point T>
constexpr auto make_frustum_matrix(T l, T r, T b, T t, T n, T f, bool depth_range_01 = false) -> Matrix4<T> {
    T double_znear = T(2) * n;
    T one_deltax   = T(1) / (r - l);
    T one_deltay   = T(1) / (t - b);
    T one_deltaz   = T(1) / (f - n);

    auto result = Matrix4<T>::identity();
    result._11 = double_znear * one_deltax;
    result._22 = double_znear * one_deltay;
    result._13 = (r + l) * one_deltax;
    result._23 = (t + b) * one_deltay;
    result._33 = -(f + n) * one_deltaz;
    result._43 = T(-1);
    result._34 = -f * double_znear * one_deltaz;
    result._44 = T(0);

    if (depth_range_01) {
        // Map the -1,1 depth range to 0,1: z' = z * 0.5 + 0.5.
        result._33 = result._33 * T(0.5) + result._43 * T(0.5);
        result._34 = result._34 * T(0.5) + result._44 * T(0.5);
    }

    return result;
}

// For scale/shear. Don't lerp rotations unless you know what you're doing.
template <std::floating_point T>
constexpr auto lerp(const Matrix3<T>& a, const Matrix3<T>& b, @T(Matrix3<T>::Value_Type) t) -> Matrix3<T> {
    Matrix3<T> r{};

    r._11 = a._11 + t*(b._11 - a._11);
    r._12 = a._12 + t*(b._12 - a._12);
    r._13 = a._13 + t*(b._13 - a._13);

    r._21 = a._21 + t*(b._21 - a._21);
    r._22 = a._22 + t*(b._22 - a._22);
    r._23 = a._23 + t*(b._23 - a._23);

    r._31 = a._31 + t*(b._31 - a._31);
    r._32 = a._32 + t*(b._32 - a._32);
    r._33 = a._33 + t*(b._33 - a._33);

    return r;
}

template <IsMatrix M>
struct InverseResult {
    M result;
    bool success;
};

template <std::floating_point T>
constexpr auto inverse(const Matrix3<T>& a, @T(Matrix3<T>::Value_Type) epsilon = T(0.001)) -> InverseResult<Matrix3<T>> {
    T c11 = a._22*a._33 - a._32*a._23;
    T c12 = a._21*a._33 - a._31*a._23;
    T c13 = a._21*a._32 - a._31*a._22;

    T det = a._11*c11 - a._12*c12 + a._13*c13;

    if (det < epsilon && det > -epsilon) return {{}, false};  // Singular.

    T idet = T(1) / det;

    Matrix3<T> r{};
    r._11 =  idet*c11;
    r._12 = -idet*(a._12*a._33 - a._13*a._32);
    r._13 =  idet*(a._12*a._23 - a._13*a._22);
    r._21 = -idet*c12;
    r._22 =  idet*(a._11*a._33 - a._13*a._31);
    r._23 = -idet*(a._11*a._23 - a._13*a._21);
    r._31 =  idet*c13;
    r._32 = -idet*(a._11*a._32 - a._12*a._31);
    r._33 =  idet*(a._11*a._22 - a._12*a._21);

    return {r, true};
}

// Fast 4x4 inverse. Good enough for well-conditioned game matrices.
// Ref: Lengyel, Foundations of Game Dev Math, 1.7.5.
template <std::floating_point T>
constexpr auto inverse(const Matrix4<T>& m, @T(Matrix4<T>::Value_Type) epsilon = T(0.0001)) -> InverseResult<Matrix4<T>> {
    Vector3<T> a{ m._11, m._21, m._31 };
    Vector3<T> b{ m._12, m._22, m._32 };
    Vector3<T> c{ m._13, m._23, m._33 };
    Vector3<T> d{ m._14, m._24, m._34 };

    T x = m._41;
    T y = m._42;
    T z = m._43;
    T w = m._44;

    Vector3<T> s = cross(a, b);
    Vector3<T> t = cross(c, d);
    Vector3<T> u = a * y - b * x;
    Vector3<T> v = c * w - d * z;

    T det = dot(s, v) + dot(t, u);

    if (abs(det) < epsilon) return {{}, false};

    T inv_det = T(1) / det;
    s *= inv_det;
    t *= inv_det;
    u *= inv_det;
    v *= inv_det;

    Vector3<T> r0 = cross(b, v) + t * y;
    Vector3<T> r1 = cross(v, a) - t * x;
    Vector3<T> r2 = cross(d, u) + s * w;
    Vector3<T> r3 = cross(u, c) - s * z;

    return {
        Matrix4<T>{
            r0.x, r0.y, r0.z, -dot(b, t),
            r1.x, r1.y, r1.z,  dot(a, t),
            r2.x, r2.y, r2.z, -dot(d, s),
            r3.x, r3.y, r3.z,  dot(c, s)
        },
        true
    };
}

template <std::floating_point T>
constexpr auto determinant(const Matrix4<T>& m) -> T {
    Vector3<T> a = static_cast<Vector3<T>>(m.v[0]);
    Vector3<T> b = static_cast<Vector3<T>>(m.v[1]);
    Vector3<T> c = static_cast<Vector3<T>>(m.v[2]);
    Vector3<T> d = static_cast<Vector3<T>>(m.v[3]);

    T x = m.v[0].w;
    T y = m.v[1].w;
    T z = m.v[2].w;
    T w = m.v[3].w;

    Vector3<T> s = cross(a, b);
    Vector3<T> t = cross(c, d);
    Vector3<T> u = a * y - b * x;
    Vector3<T> v = c * w - d * z;

    return dot(s, v) + dot(t, u);
}

template <std::floating_point T>
constexpr auto isometry_inverse(const Matrix4<T>& m) -> Matrix4<T> {
    auto result = Matrix4<T>::identity();

    // Transposed 3x3 upper-left.
    for (usize i = 0; i < 3; ++i)
        for (usize j = 0; j < 3; ++j)
            result.coef[i][j] = m.coef[j][i];

    // -t via known last row (0,0,0,1); cheaper than general translate.
    Vector3<T> t{ -m.coef[0][3], -m.coef[1][3], -m.coef[2][3] };

    result.coef[0][3] = result.coef[0][0]*t.x + result.coef[0][1]*t.y + result.coef[0][2]*t.z;
    result.coef[1][3] = result.coef[1][0]*t.x + result.coef[1][1]*t.y + result.coef[1][2]*t.z;
    result.coef[2][3] = result.coef[2][0]*t.x + result.coef[2][1]*t.y + result.coef[2][2]*t.z;

    return result;
}

#ifdef UNIT_TESTS_KSTD_MATRIX

TEST(Matrix, identity) {
    auto m2 = Matrix2<f32>::identity();
    EXPECT_EQ(m2._11, 1.0f); EXPECT_EQ(m2._12, 0.0f);
    EXPECT_EQ(m2._21, 0.0f); EXPECT_EQ(m2._22, 1.0f);

    auto m3 = Matrix3<f32>::identity();
    EXPECT_EQ(m3._11, 1.0f); EXPECT_EQ(m3._22, 1.0f); EXPECT_EQ(m3._33, 1.0f);
    EXPECT_EQ(m3._12, 0.0f); EXPECT_EQ(m3._13, 0.0f);

    auto m4 = Matrix4<f32>::identity();
    EXPECT_EQ(m4._11, 1.0f); EXPECT_EQ(m4._22, 1.0f);
    EXPECT_EQ(m4._33, 1.0f); EXPECT_EQ(m4._44, 1.0f);
    EXPECT_EQ(m4._12, 0.0f);

    auto m4x3 = Matrix4x3<f32>::identity();
    EXPECT_EQ(m4x3._11, 1.0f); EXPECT_EQ(m4x3._22, 1.0f); EXPECT_EQ(m4x3._33, 1.0f);
    EXPECT_EQ(m4x3._14, 0.0f);
}

TEST(Matrix, overlay_layout_matches) {
    auto m = Matrix4<f32>::identity();
    EXPECT_EQ(m.floats[0], m._11);
    EXPECT_EQ(m.coef[0][0], m._11);
    EXPECT_EQ(m.v[0].x, m._11);
    EXPECT_EQ(m.floats[5], m._22);
    EXPECT_EQ(m.coef[1][1], m._22);
}

TEST(Matrix, row_and_column) {
    Matrix4<f32> m{
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };

    auto r = row(m, 1);
    EXPECT_EQ(r.x, 5.0f); EXPECT_EQ(r.y, 6.0f); EXPECT_EQ(r.z, 7.0f); EXPECT_EQ(r.w, 8.0f);

    auto c = column(m, 1);
    EXPECT_EQ(c.x, 2.0f); EXPECT_EQ(c.y, 6.0f); EXPECT_EQ(c.z, 10.0f); EXPECT_EQ(c.w, 14.0f);
}

TEST(Matrix, multiply_matrix4_vector4) {
    auto m = Matrix4<f32>::identity();
    Vector4<f32> v{ 1, 2, 3, 4 };
    EXPECT_TRUE((m * v) == v);
}

TEST(Matrix, multiply_matrix4x3_vector4) {
    auto m = Matrix4x3<f32>::identity();
    Vector4<f32> v{ 1, 2, 3, 4 };
    auto result = m * v;
    EXPECT_EQ(result.x, 1.0f);
    EXPECT_EQ(result.y, 2.0f);
    EXPECT_EQ(result.z, 3.0f);
}

TEST(Matrix, multiply_matrix2_vector2) {
    auto m = Matrix2<f32>::identity();
    Vector2<f32> v{ 3, 4 };
    EXPECT_TRUE((m * v) == v);
}

TEST(Matrix, multiply_matrix3_vector3) {
    auto m = Matrix3<f32>::identity();
    Vector3<f32> v{ 3, 4, 5 };
    EXPECT_TRUE((m * v) == v);
}

TEST(Matrix, transform_point) {
    auto m = make_translation_matrix4(Vector3<f32>{ 1, 2, 3 });
    auto p = transform_point(m, Vector3<f32>{ 0, 0, 0 });
    EXPECT_EQ(p.x, 1.0f);
    EXPECT_EQ(p.y, 2.0f);
    EXPECT_EQ(p.z, 3.0f);
}

TEST(Matrix, multiply_matrix4_matrix4_identity) {
    auto m = Matrix4<f32>::identity();
    auto n = Matrix4<f32>::identity();
    EXPECT_TRUE((m * n) == Matrix4<f32>::identity());
}

TEST(Matrix, multiply_matrix3_matrix3) {
    Matrix3<f32> m{
        1, 2, 3,
        4, 5, 6,
        7, 8, 10
    };

    EXPECT_TRUE((m * Matrix3<f32>::identity()) == m);
}

TEST(Matrix, scalar_multiply) {
    auto m = Matrix3<f32>::identity();
    auto r = m * 2.0f;
    EXPECT_EQ(r._11, 2.0f);
    EXPECT_EQ(r._22, 2.0f);
    EXPECT_EQ(r._33, 2.0f);

    auto r2 = 2.0f * m;
    EXPECT_TRUE(r == r2);
}

TEST(Matrix, add_subtract) {
    auto a = Matrix3<f32>::identity();
    auto b = Matrix3<f32>::identity();

    auto sum = a + b;
    EXPECT_EQ(sum._11, 2.0f);

    auto diff = sum - a;
    EXPECT_TRUE(diff == b);
}

TEST(Matrix, equality) {
    auto a = Matrix4<f32>::identity();
    auto b = Matrix4<f32>::identity();
    auto c = a;
    c._11 = 5.0f;

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(Matrix, make_matrix3_from_matrix4) {
    auto m4 = Matrix4<f32>::identity();
    m4._11 = 2.0f;

    auto m3 = make_matrix3(m4);
    EXPECT_EQ(m3._11, 2.0f);
    EXPECT_EQ(m3._22, 1.0f);
    EXPECT_EQ(m3._33, 1.0f);
}

TEST(Matrix, make_matrix4_from_matrix3) {
    auto m3 = Matrix3<f32>::identity();
    m3._11 = 2.0f;

    auto m4 = make_matrix4(m3);
    EXPECT_EQ(m4._11, 2.0f);
    EXPECT_EQ(m4._44, 1.0f);
}

TEST(Matrix, make_matrix4_from_matrix4x3_copies_translation) {
    auto m4x3 = make_translation_matrix4(Vector3<f32>{1, 2, 3});
    Matrix4x3<f32> src{};
    src._11 = 1; src._22 = 1; src._33 = 1;
    src._14 = 1; src._24 = 2; src._34 = 3;

    auto m4 = make_matrix4(src);
    EXPECT_EQ(m4._14, 1.0f);
    EXPECT_EQ(m4._24, 2.0f);
    EXPECT_EQ(m4._34, 3.0f);
    EXPECT_EQ(m4._44, 1.0f);
    (void)m4x3;
}

TEST(Matrix, transpose_matrix4) {
    Matrix4<f32> m{
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };

    auto t = transpose(m);
    EXPECT_EQ(t._12, 5.0f);
    EXPECT_EQ(t._21, 2.0f);
    EXPECT_EQ(t._43, 12.0f);
}

TEST(Matrix, transpose_matrix4_in_place) {
    Matrix4<f32> m{
        1, 2, 3, 4,
        5, 6, 7, 8,
        9, 10, 11, 12,
        13, 14, 15, 16
    };

    auto expected = transpose(m);
    transpose(&m);
    EXPECT_TRUE(m == expected);
}

TEST(Matrix, transpose_matrix3) {
    Matrix3<f32> m{
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    auto t = transpose(m);
    EXPECT_EQ(t._12, 4.0f);
    EXPECT_EQ(t._21, 2.0f);
}

TEST(Matrix, transpose_matrix3_in_place) {
    Matrix3<f32> m{
        1, 2, 3,
        4, 5, 6,
        7, 8, 9
    };

    auto expected = transpose(m);
    transpose(&m);
    EXPECT_TRUE(m == expected);
}

TEST(Matrix, make_scale_matrix4) {
    auto m = make_scale_matrix4(Vector3<f32>{ 2, 3, 4 });
    EXPECT_EQ(m._11, 2.0f);
    EXPECT_EQ(m._22, 3.0f);
    EXPECT_EQ(m._33, 4.0f);
    EXPECT_EQ(m._44, 1.0f);
}

TEST(Matrix, scale_scalar) {
    auto m = Matrix3<f32>::identity();
    auto r = scale(m, 2.0f);
    EXPECT_EQ(r._11, 2.0f);
    EXPECT_EQ(r._22, 2.0f);
    EXPECT_EQ(r._33, 2.0f);
}

TEST(Matrix, scale_scalar_in_place) {
    auto m = Matrix3<f32>::identity();
    scale(&m, 2.0f);
    EXPECT_EQ(m._11, 2.0f);
    EXPECT_EQ(m._22, 2.0f);
    EXPECT_EQ(m._33, 2.0f);
}

TEST(Matrix, scale_vector) {
    auto m = Matrix3<f32>::identity();
    auto r = scale(m, Vector3<f32>{ 2, 3, 4 });
    EXPECT_EQ(r._11, 2.0f);
    EXPECT_EQ(r._22, 3.0f);
    EXPECT_EQ(r._33, 4.0f);
}

TEST(Matrix, make_translation_matrix4) {
    auto m = make_translation_matrix4(Vector3<f32>{ 1, 2, 3 });
    EXPECT_EQ(m._14, 1.0f);
    EXPECT_EQ(m._24, 2.0f);
    EXPECT_EQ(m._34, 3.0f);
}

TEST(Matrix, translate_value) {
    auto m = Matrix4<f32>::identity();
    auto r = translate(m, Vector3<f32>{ 1, 2, 3 });
    EXPECT_EQ(r._14, 1.0f);
    EXPECT_EQ(r._24, 2.0f);
    EXPECT_EQ(r._34, 3.0f);
}

TEST(Matrix, translate_in_place) {
    auto m = Matrix4<f32>::identity();
    translate(&m, Vector3<f32>{ 1, 2, 3 });
    EXPECT_EQ(m._14, 1.0f);
    EXPECT_EQ(m._24, 2.0f);
    EXPECT_EQ(m._34, 3.0f);
}

TEST(Matrix, rotation_matrix_identity_quaternion) {
    auto q = Quaternion<f32>::identity();

    auto m3 = make_rotation_matrix<Matrix3<f32>>(q);
    EXPECT_TRUE(m3 == Matrix3<f32>::identity());

    auto m4 = make_rotation_matrix<Matrix4<f32>>(q);
    EXPECT_TRUE(m4 == Matrix4<f32>::identity());
}

TEST(Matrix, rotate_with_identity_quaternion) {
    auto q = Quaternion<f32>::identity();

    auto m3 = Matrix3<f32>::identity();
    EXPECT_TRUE(rotate(m3, q) == Matrix3<f32>::identity());

    auto m4 = Matrix4<f32>::identity();
    EXPECT_TRUE(rotate(m4, q) == Matrix4<f32>::identity());
}

TEST(Matrix, make_matrix_from_rows_default_matrix4) {
    Vector3<f32> x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    auto m = make_matrix_from_rows(x, y, z);
    EXPECT_TRUE(m == Matrix4<f32>::identity());
}

TEST(Matrix, make_matrix_from_rows_matrix3) {
    Vector3<f32> x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    auto m = make_matrix_from_rows<Matrix3<f32>>(x, y, z);
    EXPECT_TRUE(m == Matrix3<f32>::identity());
}

TEST(Matrix, make_matrix_from_columns_default_matrix4) {
    Vector3<f32> x{1, 0, 0}, y{0, 1, 0}, z{0, 0, 1};
    auto m = make_matrix_from_columns(x, y, z);
    EXPECT_TRUE(m == Matrix4<f32>::identity());
}

TEST(Matrix, make_look_at_matrix_looking_down_x_axis) {
    Vector3<f32> viewpoint{0, 0, 0};
    Vector3<f32> look_at{1, 0, 0};
    Vector3<f32> up{0, 0, 1};

    auto m = make_look_at_matrix(viewpoint, look_at, up);

    // Looking down +x, x-forward: forward row should be (1,0,0).
    EXPECT_NEAR(m._11, 1.0f, 1e-5f);
    EXPECT_NEAR(m._12, 0.0f, 1e-5f);
    EXPECT_NEAR(m._13, 0.0f, 1e-5f);
}

TEST(Matrix, orthographic_projection_matrix_basic) {
    auto m = orthographic_projection_matrix<f32>(-1, 1, -1, 1, 0.1f, 100.0f);
    EXPECT_NEAR(m._11, 1.0f, 1e-5f);
    EXPECT_NEAR(m._22, 1.0f, 1e-5f);
    EXPECT_EQ(m._44, 1.0f);
}

TEST(Matrix, make_projection_matrix_produces_finite_values) {
    auto m = make_projection_matrix<f32>(1.0f, 16.0f/9.0f, 0.1f, 100.0f);
    EXPECT_TRUE(m._11 == m._11);  // Not NaN.
    EXPECT_EQ(m._43, -1.0f);
}

TEST(Matrix, make_frustum_matrix_basic) {
    auto m = make_frustum_matrix<f32>(-1, 1, -1, 1, 1, 100);
    EXPECT_NEAR(m._11, 1.0f, 1e-5f);
    EXPECT_EQ(m._43, -1.0f);
}

TEST(Matrix, lerp_matrix3) {
    auto a = Matrix3<f32>::identity();
    auto b = a * 3.0f;

    auto mid = lerp(a, b, 0.5f);
    EXPECT_NEAR(mid._11, 2.0f, 1e-6f);
}

TEST(Matrix, inverse_matrix3_identity) {
    auto m = Matrix3<f32>::identity();
    auto inv = inverse(m);
    ASSERT_TRUE(inv.has_value());
    EXPECT_TRUE(*inv == m);
}

TEST(Matrix, inverse_matrix3_singular_fails) {
    Matrix3<f32> m{};  // All zero: singular.
    auto inv = inverse(m);
    EXPECT_FALSE(inv.has_value());
}

TEST(Matrix, inverse_matrix3_round_trip) {
    Matrix3<f32> m{
        2, 0, 0,
        0, 3, 0,
        0, 0, 4
    };

    auto inv = inverse(m);
    ASSERT_TRUE(inv.has_value());

    auto product = m * (*inv);
    EXPECT_TRUE(product == Matrix3<f32>::identity());
}

TEST(Matrix, inverse_matrix4_identity) {
    auto m = Matrix4<f32>::identity();
    auto inv = inverse(m);
    ASSERT_TRUE(inv.has_value());
    EXPECT_TRUE(*inv == m);
}

TEST(Matrix, inverse_matrix4_singular_fails) {
    Matrix4<f32> m{};  // All zero: singular.
    auto inv = inverse(m);
    EXPECT_FALSE(inv.has_value());
}

TEST(Matrix, inverse_matrix4_round_trip) {
    auto m = make_translation_matrix4(Vector3<f32>{ 1, 2, 3 });

    auto inv = inverse(m);
    ASSERT_TRUE(inv.has_value());

    auto product = m * (*inv);
    EXPECT_TRUE(product == Matrix4<f32>::identity());
}

TEST(Matrix, determinant_identity_is_one) {
    EXPECT_NEAR(determinant(Matrix4<f32>::identity()), 1.0f, 1e-6f);
}

TEST(Matrix, isometry_inverse_undoes_translation_and_rotation) {
    auto m = make_translation_matrix4(Vector3<f32>{ 1, 2, 3 });
    auto inv = isometry_inverse(m);
    auto product = m * inv;
    EXPECT_TRUE(product == Matrix4<f32>::identity());
}

#endif

}  // namespace math
