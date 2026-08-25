#pragma once

#include "basic.hh"

constexpr auto kstd_strlen(const char* str) -> s64 {
    s64 len = 0;
    while (str[len]) len++;
    return len;
}

force_inline auto kstd_memcpy(void* __restrict destination, const void* __restrict source, s64 length) -> void*;
force_inline auto kstd_memset(void* buffer, int value, s64 length) -> void*;
force_inline auto kstd_memset32(void* buffer, u32 value, s64 count) -> void*;
force_inline auto kstd_memcmp(const void* a, const void* b, s64 length) -> int;

force_inline auto kstd_memeq(const void* a, const void* b, s64 length) -> bool {
    return kstd_memcmp(a, b, length) == 0;
}

template <typename T, usize N>
force_inline auto kstd_memeq(const void* bytes, const T (&array)[N]) -> bool {
    return kstd_memcmp(bytes, array, size_of(T) * N) == 0;
}

#if OS == GAMEOS
#include "gameos/cstring.hh"
#elif OS == LINUX
#include "linux/cstring.hh"
#else
#error "Unsupported OS"
#endif
