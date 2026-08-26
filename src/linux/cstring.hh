#pragma once

#include <cstring>

#include "kstd/cstring.hh"

force_inline auto kstd_memcpy(void* __restrict destination, const void* __restrict source, ssize length) -> void* {
    if (length <= 0) return destination;
    return std::memcpy(destination, source, cast(usize)length);
}

force_inline auto kstd_memset(void* buffer, int value, ssize length) -> void* {
    if (length <= 0) return buffer;
    return std::memset(buffer, value, cast(usize)length);
}

force_inline auto kstd_memset32(void* buffer, u32 value, ssize count) -> void* {
    if (count <= 0) return buffer;
    auto* destination = cast(u32*)buffer;
    for (ssize i = 0; i < count; ++i) destination[i] = value;
    return buffer;
}

force_inline auto kstd_memcmp(const void* a, const void* b, ssize length) -> int {
    if (length <= 0) return 0;
    return std::memcmp(a, b, cast(usize)length);
}
