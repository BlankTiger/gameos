#pragma once

#include "array.hh"

struct Resource_View {
    Array_View<const u8> data;
    u32 width;
    u32 height;

    template <typename T>
    auto data_as() const -> Array_View<const T> {
        kstd_assert(data.size % size_of(T) == 0, "resource size is not a multiple of target type size");
        kstd_assert(ptr_addr(data.data) % align_of(T) == 0, "resource data is not aligned for target type");
        return { data.size / size_of(T), cast(const T*)data.data };
    }
};

template <s64 N>
struct Resource {
    Static_Array<u8, N> data;
    const u32 width;
    const u32 height;

    constexpr auto view() const -> Resource_View {
        return { Array_View<const u8>{data.size, data.data}, width, height };
    }

    constexpr operator const Resource_View() const {
        return view();
    }
};
