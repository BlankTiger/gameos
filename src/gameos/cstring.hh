#pragma once

#include "kstd/cstring.hh"

force_inline auto kstd_memcpy(void* __restrict destination, const void* __restrict source, s64 length) -> void* {
    if (length <= 0) return destination;
    int qwords    = cast(int)(length / 8);
    int remainder = cast(int)(length % 8);
    void* dst       = destination;
    const void* src = source;

    asm volatile("cld; rep movsq" : "+D"(dst), "+S"(src), "+c"(qwords) : : "memory", "cc");

    u8* dst_bytes       = cast(u8*)dst;
    const u8* src_bytes = cast(const u8*)src;
    while (remainder--) *dst_bytes++ = *src_bytes++;
    return destination;
}

force_inline auto kstd_memset(void* buffer, int value, s64 length) -> void* {
    if (length <= 0) return buffer;
    void* dst   = buffer;
    usize bytes = cast(usize)length;
    asm volatile("cld; rep stosb" : "+D"(dst), "+c"(bytes) : "a"(cast(u8)value) : "memory", "cc");
    return buffer;
}

force_inline auto kstd_memset32(void* buffer, u32 value, s64 count) -> void* {
    if (count <= 0) return buffer;
    void* dst = buffer;
    usize n   = cast(usize)count;
    asm volatile("cld; rep stosl" : "+D"(dst), "+c"(n) : "a"(value) : "memory", "cc");
    return buffer;
}

force_inline auto kstd_memcmp(const void* a, const void* b, s64 length) -> int {
    if (length <= 0) return 0;
    const u8* p = cast(const u8*)a;
    const u8* q = cast(const u8*)b;
    for (s64 i = 0; i < length; ++i) {
        if (p[i] != q[i]) return cast(int)p[i] - cast(int)q[i];
    }
    return 0;
}
