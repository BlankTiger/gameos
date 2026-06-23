#pragma once

#include <bit>

#include "kstd.hh"

namespace fb {

struct Framebuffer {
    u32* pixels;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bits_per_pixel;
    u8 type;
};

static Framebuffer current;

static auto initialize(const kstd::mem::Multiboot2_Framebuffer_Tag* tag) -> void {
    current.pixels = reinterpret_cast<u32*>((uintptr_t)tag->framebuffer_addr);
    current.pitch = tag->framebuffer_pitch;
    current.width = tag->framebuffer_width;
    current.height = tag->framebuffer_height;
    current.bits_per_pixel = tag->framebuffer_bpp;
    current.type = tag->framebuffer_type;
}

struct Color {
    u8 r, g, b, w;
};

static_assert(sizeof(Color) == sizeof(u32));

auto clear(u32 color) -> void {
    kstd::debug_assert(current.pixels != nullptr);

    if (current.bits_per_pixel == 32) {
        const usize stride = current.pitch / sizeof(u32);

        for (u32 y = 0; y < current.height; ++y) {
            for (u32 x = 0; x < current.width; ++x) {
                current.pixels[y * stride + x] = color;
            }
        }
    }
}

force_inline auto clear(Color color) -> void {
    clear(std::bit_cast<u32>(color));
}

}  // namespace fb
