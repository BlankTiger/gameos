#pragma once

#include <cstring>

#include "int.hh"

auto strlen(const char* str) -> usize {
    usize len = 0;
    while (str[len]) len++;
    return len;
}

force_inline auto memcpy(void* __restrict destination, const void* __restrict source, size_t length) -> void* {
    return __builtin_memcpy(destination, source, length);
}

force_inline auto memset(void* buffer, int value, size_t length) -> void* {
    return __builtin_memset(buffer, value, length);
}
