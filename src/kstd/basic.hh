#pragma once

#include <cstddef>

#include "numbers.hh"

#define GAMEOS 1
#define LINUX  2

#if defined(__linux__)
#define OS LINUX
#else
#define OS GAMEOS
#endif

#if defined(__GNUC__) || defined(__clang__)
#define force_inline inline __attribute__((always_inline))
#else
#define force_inline inline
#endif

#if defined(__GNUC__) || defined(__clang__)
#define noinline __attribute__((noinline))
#elif defined(_MSC_VER)
#define noinline __declspec(noinline)
#else
#define noinline
#endif

#define NAME_CONCAT_IMPL(x, y) x##y
#define NAME_CONCAT(x, y) NAME_CONCAT_IMPL(x, y)
#define DEFER_UNIQ(prefix) NAME_CONCAT(prefix, __COUNTER__)

#define cast(type) (type)

#define size_of(value) cast(ssize)sizeof(value)
#define align_of(value) cast(ssize)alignof(value)
#define offset_of(type, member) cast(ssize)offsetof(type, member)

inline constexpr ssize MAX_ALIGN = align_of(std::max_align_t);

template <typename F>
struct Defer {
    F f;
    Defer(F f) : f(f) {}
    ~Defer() { f(); }
};

#define defer_copy(code) Defer DEFER_UNIQ(_defer_)([=]() { code; })
#define defer(code) Defer DEFER_UNIQ(_defer_)([&]() { code; })

// Extent value meaning "Array_View's size is only known at runtime"
// (as opposed to a compile-time N). Declared here rather than in
// array.hh since string.hh needs it for a forward declaration
// before array.hh itself is reachable (see string.hh comment).
inline constexpr ssize DYNAMIC_EXTENT = -1;

namespace mem {
    inline constexpr ssize PAGE_SSIZE = 4096;
    inline constexpr auto  PAGE_USIZE = cast(usize)PAGE_SSIZE;
}
