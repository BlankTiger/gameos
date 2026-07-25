#pragma once

#include "basic.hh"

constexpr auto kstd_strlen(const char* str) -> usize {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
#include <cstring>
#endif

force_inline auto kstd_memcpy(void* __restrict destination, const void* __restrict source, usize length) -> void* {
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
    return std::memcpy(destination, source, length);
#else
    usize dwords = length / 4;
    usize remainder = length % 4;

    void* dst = destination;
    const void* src = source;

    asm volatile("rep movsl" : "+D"(dst), "+S"(src), "+c"(dwords) : : "memory");

    u8* dst_bytes = static_cast<u8*>(dst);
    const u8* src_bytes = static_cast<const u8*>(src);

    while (remainder--) {
        *dst_bytes++ = *src_bytes++;
    }

    return destination;
#endif
}

force_inline auto kstd_memset(void* buffer, int value, usize length) -> void* {
#if defined(__STDC_HOSTED__) && __STDC_HOSTED__
    return std::memset(buffer, value, length);
#else
    u8* dst = (u8*)buffer;

    while (length--) {
        *dst++ = (u8)value;
    }

    return buffer;
#endif
}
