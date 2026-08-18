#pragma once

#include <cstring>

#include "kstd/cstring.hh"

force_inline auto kstd_memcpy(void* __restrict destination, const void* __restrict source, usize length) -> void* {
    return std::memcpy(destination, source, length);
}

force_inline auto kstd_memset(void* buffer, int value, usize length) -> void* {
    return std::memset(buffer, value, length);
}

force_inline auto kstd_memset32(void* buffer, u32 value, usize count) -> void* {
    auto* destination = static_cast<u32*>(buffer);
    for (usize i = 0; i < count; ++i) destination[i] = value;
    return buffer;
}

force_inline auto kstd_memcmp(const void* a, const void* b, usize length) -> int {
    return std::memcmp(a, b, length);
}
