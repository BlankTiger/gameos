#pragma once

#include <cstdint>
#include <stdfloat>
#include <limits>

using s8  = std::int8_t;
using s16 = std::int16_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using f32 = std::float32_t;
using f64 = std::float64_t;

using usize = std::size_t;
using psize = std::uintptr_t;

constexpr auto S8_MAX  = std::numeric_limits<s8>::max();
constexpr auto S16_MAX = std::numeric_limits<s16>::max();
constexpr auto S32_MAX = std::numeric_limits<s32>::max();
constexpr auto S64_MAX = std::numeric_limits<s64>::max();

constexpr auto S8_MIN  = std::numeric_limits<s8>::min();
constexpr auto S16_MIN = std::numeric_limits<s16>::min();
constexpr auto S32_MIN = std::numeric_limits<s32>::min();
constexpr auto S64_MIN = std::numeric_limits<s64>::min();

constexpr auto U8_MAX  = std::numeric_limits<u8>::max();
constexpr auto U16_MAX = std::numeric_limits<u16>::max();
constexpr auto U32_MAX = std::numeric_limits<u32>::max();
constexpr auto U64_MAX = std::numeric_limits<u64>::max();

constexpr auto U8_MIN  = std::numeric_limits<u8>::min();
constexpr auto U16_MIN = std::numeric_limits<u16>::min();
constexpr auto U32_MIN = std::numeric_limits<u32>::min();
constexpr auto U64_MIN = std::numeric_limits<u64>::min();

constexpr auto F32_MAX = std::numeric_limits<f32>::max();
constexpr auto F64_MAX = std::numeric_limits<f64>::max();

constexpr auto F32_MIN = std::numeric_limits<f32>::lowest();
constexpr auto F64_MIN = std::numeric_limits<f64>::lowest();

// constexpr auto INT_MAX   = std::numeric_limits<int>::max();
// constexpr auto FLOAT_MAX = std::numeric_limits<float>::max();
//
// constexpr auto INT_MIN   = std::numeric_limits<int>::min();
// constexpr auto FLOAT_MIN = std::numeric_limits<float>::min();

constexpr auto USIZE_MAX = std::numeric_limits<usize>::max();
constexpr auto PSIZE_MAX = std::numeric_limits<psize>::max();

constexpr auto USIZE_MIN = std::numeric_limits<usize>::min();
constexpr auto PSIZE_MIN = std::numeric_limits<psize>::min();
