#pragma once

// This contains only the definitions
#include <cstring>

#include "int.hh"

auto strlen(const char* str) -> usize {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

force_inline auto memcpy(
    void* __restrict destination,
    const void* __restrict source,
    usize length
) -> void* {
    u8* dst = (u8*)destination;
    const u8* src = (const u8*)source;

    while (length--) {
        *dst++ = *src++;
    }

    return destination;
}

force_inline auto memset(
    void* buffer,
    int value,
    usize length) -> void*
{
    u8* dst = (u8*)buffer;

    while (length--) {
        *dst++ = (u8)value;
    }

    return buffer;
}
