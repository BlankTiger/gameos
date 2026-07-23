#pragma once

#include "array.hh"

struct Resource_View {
    const u8* data;
    u32 size;
    u32 width;
    u32 height;
};

template <usize N>
struct Resource {
    Static_Array<u8, N> data;
    const u32 width;
    const u32 height;

    constexpr auto view() const -> Resource_View {
        return { data.elements(), data.size, width, height };
    }
};
