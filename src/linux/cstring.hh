#pragma once

#include <cstring>

#include "kstd/cstring.hh"

force_inline auto kstd_memcpy(void* __restrict destination, const void* __restrict source, s64 length) -> void* {
    if (length <= 0) return destination;
    return std::memcpy(destination, source, static_cast<usize>(length));
}

force_inline auto kstd_memset(void* buffer, int value, s64 length) -> void* {
    if (length <= 0) return buffer;
    return std::memset(buffer, value, static_cast<usize>(length));
}

force_inline auto kstd_memset32(void* buffer, u32 value, s64 count) -> void* {
    if (count <= 0) return buffer;
    auto* destination = static_cast<u32*>(buffer);
    for (s64 i = 0; i < count; ++i) destination[i] = value;
    return buffer;
}

force_inline auto kstd_memcmp(const void* a, const void* b, s64 length) -> int {
    if (length <= 0) return 0;
    return std::memcmp(a, b, static_cast<usize>(length));
}
