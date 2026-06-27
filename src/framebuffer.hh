#pragma once

#include <bit>

#include "font8x16.hh"
#include "kstd.hh"
#include "multiboot2.hh"

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

static auto initialize(const kstd::mem::Multiboot2_Framebuffer_Tag* tag) -> bool {
    if (tag->framebuffer_addr == 0) return false;

    current.pixels = reinterpret_cast<u32*>((uintptr_t)tag->framebuffer_addr);
    current.pitch = tag->framebuffer_pitch;
    current.width = tag->framebuffer_width;
    current.height = tag->framebuffer_height;
    current.bits_per_pixel = tag->framebuffer_bpp;
    current.type = tag->framebuffer_type;
    kstd::assert(current.bits_per_pixel == 32, "Only 32BPP supported.");

    return true;
}

struct Color {
    u8 b, g, r, w;
};

static_assert(sizeof(Color) == sizeof(u32));

constexpr Color BLACK{0, 0, 0, 255};
constexpr Color WHITE{255, 255, 255, 255};
constexpr Color RED{0, 0, 255, 255};
constexpr Color GREEN{0, 255, 0, 255};
constexpr Color BLUE{255, 0, 0, 255};

auto set_pixel(u32 x, u32 y, Color color) -> void {
    kstd::debug_assert(current.pixels != nullptr);

    const usize stride = current.pitch / sizeof(u32);
    current.pixels[y * stride + x] = std::bit_cast<u32>(color);
}

auto clear(u32 color) -> void {
    kstd::debug_assert(current.pixels != nullptr);

    const usize stride = current.pitch / sizeof(u32);

    for (u32 y = 0; y < current.height; ++y) {
        for (u32 x = 0; x < current.width; ++x) {
            current.pixels[y * stride + x] = color;
        }
    }
}

force_inline auto clear(Color color) -> void {
    clear(std::bit_cast<u32>(color));
}

auto draw_char(u32 x, u32 y, char c, Color fg, Color bg) -> void {
    const auto index = static_cast<u8>(c);
    const auto& glyph = font::DATA[index];

    for (u32 row = 0; row < font::GLYPH_HEIGHT; ++row) {
        const u32 py = y + row;
        if (py >= current.height) break;

        const font::Glyph_Width bits = glyph[row];
        for (u32 col = 0; col < font::GLYPH_WIDTH; ++col) {
            const u32 px = x + col;
            if (px >= current.width) break;

            //
            // @TODO: Maybe we should consider doing transparency by default for the bg color.
            //
            const auto color = bits & (0b1000'0000 >> col) ? fg : bg;
            set_pixel(px, py, color);
        }
    }
}

auto draw_text(u32 x, u32 y, const char* text, Color fg = WHITE, Color bg = BLACK) -> void {
    u32 cx = x;
    u32 cy = y;

    for (; *text; ++text) {
        if (*text == '\n') {
            cx = x;
            cy += font::GLYPH_HEIGHT;
            continue;
        }

        if (cx + font::GLYPH_WIDTH > current.width) {
            cx = x;
            cy += font::GLYPH_HEIGHT;
        }

        if (cy + font::GLYPH_HEIGHT > current.height) break;

        draw_char(cx, cy, *text, fg, bg);
        cx += font::GLYPH_WIDTH;
    }
}

}  // namespace fb
