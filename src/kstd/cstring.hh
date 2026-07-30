#pragma once

#include "basic.hh"

constexpr auto kstd_strlen(const char* str) -> usize {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

#if HOSTED
#include <cstring>
#endif

force_inline auto kstd_memcpy(void* __restrict destination, const void* __restrict source, usize length) -> void* {
#if HOSTED
    return std::memcpy(destination, source, length);
#else
    usize dwords = length / 4;
    usize remainder = length % 4;

    void* dst = destination;
    const void* src = source;

    asm volatile("cld; rep movsl" : "+D"(dst), "+S"(src), "+c"(dwords) : : "memory", "cc");

    u8* dst_bytes = static_cast<u8*>(dst);
    const u8* src_bytes = static_cast<const u8*>(src);

    while (remainder--) {
        *dst_bytes++ = *src_bytes++;
    }

    return destination;
#endif
}

force_inline auto kstd_memset(void* buffer, int value, usize length) -> void* {
#if HOSTED
    return std::memset(buffer, value, length);
#else
    void* dst = buffer;
    usize bytes = length;
    asm volatile("cld; rep stosb" : "+D"(dst), "+c"(bytes) : "a"(static_cast<u8>(value)) : "memory", "cc");
    return buffer;
#endif
}

// Fill `count` dwords with `value` (rep stosl on freestanding i686).
force_inline auto kstd_memset32(void* buffer, u32 value, usize count) -> void* {
#if HOSTED
    auto* dst = static_cast<u32*>(buffer);
    for (usize i = 0; i < count; ++i) dst[i] = value;
    return buffer;
#else
    void* dst = buffer;
    usize n = count;
    asm volatile("cld; rep stosl" : "+D"(dst), "+c"(n) : "a"(value) : "memory", "cc");
    return buffer;
#endif
}
