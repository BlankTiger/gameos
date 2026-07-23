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
    u8* dst = (u8*)destination;
    const u8* src = (const u8*)source;

    while (length--) {
        *dst++ = *src++;
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
