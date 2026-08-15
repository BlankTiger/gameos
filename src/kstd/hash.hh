#pragma once

#include <bit>
#include <type_traits>

#include "kstd/basic.hh"
#include "kstd/string.hh"

namespace hash {

constexpr u32 HASH_INIT = 0;

constexpr u64 FNV_64_PRIME       = 0x100000001b3;
constexpr u64 FNV_64_OFFSET_BIAS = 0xcbf29ce484222325;

template <typename Key_Type>
using Hash_Function = auto (*)(Key_Type&) -> u32;

auto sdbm_hash(const void* data, usize size, u32 hash = HASH_INIT) -> u32 {
    auto* bytes = static_cast<const u8*>(data);
    for (usize i = 0; i < size; ++i)
        hash = (hash << 16) + (hash << 6) - hash + bytes[i];

    return hash;
}

template <typename Float_Type>
auto sdbm_float_hash(const Float_Type* data, usize count, u32 hash = HASH_INIT) -> u32 {
    static_assert(std::is_floating_point_v<Float_Type>);
    static_assert(sizeof(Float_Type) == sizeof(u32) || sizeof(Float_Type) == sizeof(u64));

    for (usize i = 0; i < count; ++i) {
        if constexpr (sizeof(Float_Type) == sizeof(u32)) {
            auto bits = std::bit_cast<u32>(data[i]);
            if (bits == 0x80000000) bits = 0;
            hash = sdbm_hash(&bits, sizeof(bits), hash);
        } else {
            auto bits = std::bit_cast<u64>(data[i]);
            if (bits == 0x8000000000000000) bits = 0;
            hash = sdbm_hash(&bits, sizeof(bits), hash);
        }
    }

    return hash;
}

auto fnv1a_hash(u64 value, u64 hash = FNV_64_OFFSET_BIAS) -> u64 {
    return (hash ^ value) * FNV_64_PRIME;
}

auto fnv1a_hash(const void* data, usize size, u64 hash = FNV_64_OFFSET_BIAS) -> u64 {
    auto* bytes = static_cast<const u8*>(data);
    for (usize i = 0; i < size; ++i)
        hash = fnv1a_hash(bytes[i], hash);

    return hash;
}

auto knuth_hash(u64 value) -> u64 {
    constexpr u64 KNUTH_GOLDEN_RATIO_64 = 11400714819323198485ull;
    return KNUTH_GOLDEN_RATIO_64 * value;
}

template <typename T>
auto compute(T& value) -> u32 {
    using Value_Type = std::remove_cv_t<T>;

    if constexpr (std::is_same_v<Value_Type, string>) {
        auto hash_value = fnv1a_hash(value.data, value.size);
        return static_cast<u32>(hash_value ^ (hash_value >> 32));
    } else if constexpr (std::is_floating_point_v<Value_Type>) {
        return sdbm_hash(&value, sizeof(value));
    } else if constexpr (std::is_enum_v<Value_Type>) {
        using Underlying_Type = std::underlying_type_t<Value_Type>;
        return static_cast<u32>(knuth_hash(static_cast<u64>(static_cast<Underlying_Type>(value))) >> 32);
    } else if constexpr (std::is_integral_v<Value_Type>) {
        return static_cast<u32>(knuth_hash(static_cast<u64>(value)) >> 32);
    } else if constexpr (std::is_pointer_v<Value_Type>) {
        return static_cast<u32>(knuth_hash(reinterpret_cast<u64>(value)) >> 32);
    } else {
        static_assert(std::is_arithmetic_v<Value_Type>, "hash::compute does not support this type");
    }
}

}
