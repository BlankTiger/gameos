#pragma once

#include <bit>

#include "font8x16.hh"
#include "../kstd.hh"
#include "../multiboot2.hh"

namespace fb {

struct Framebuffer {
    u32* pixels;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bits_per_pixel;
    u8 type;
};

static Framebuffer __current_frame;

static auto initialize(const kstd::mem::Multiboot2_Framebuffer_Tag* tag) -> bool {
    if (tag->framebuffer_addr == 0) return false;

    __current_frame.pixels = reinterpret_cast<u32*>((uintptr_t)tag->framebuffer_addr);
    __current_frame.pitch = tag->framebuffer_pitch;
    __current_frame.width = tag->framebuffer_width;
    __current_frame.height = tag->framebuffer_height;
    __current_frame.bits_per_pixel = tag->framebuffer_bpp;
    __current_frame.type = tag->framebuffer_type;
    kstd::assert(__current_frame.bits_per_pixel == 32, "Only 32BPP supported.");

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
    kstd::debug_assert(__current_frame.pixels != nullptr);

    const usize stride = __current_frame.pitch / sizeof(u32);
    __current_frame.pixels[y * stride + x] = std::bit_cast<u32>(color);
}

auto clear(u32 color) -> void {
    kstd::debug_assert(__current_frame.pixels != nullptr);

    const usize stride = __current_frame.pitch / sizeof(u32);

    for (u32 y = 0; y < __current_frame.height; ++y) {
        for (u32 x = 0; x < __current_frame.width; ++x) {
            __current_frame.pixels[y * stride + x] = color;
        }
    }
}

force_inline auto clear(Color color) -> void {
    clear(std::bit_cast<u32>(color));
}

// struct Sprite {};
//
// auto draw_rect(u32 x, u32 y, u32 w, u32 h) -> void {}
// auto draw_sprite(u32 x, u32 y, const Sprite sprite) -> void {}

auto draw_char(u32 x, u32 y, char c, Color fg, Color bg) -> void {
    const auto index = static_cast<u8>(c);
    const auto& glyph = font::DATA[index];

    for (u32 row = 0; row < font::GLYPH_HEIGHT; ++row) {
        const u32 py = y + row;
        if (py >= __current_frame.height) break;

        const font::Glyph_Width bits = glyph[row];
        for (u32 col = 0; col < font::GLYPH_WIDTH; ++col) {
            const u32 px = x + col;
            if (px >= __current_frame.width) break;

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

        if (cx + font::GLYPH_WIDTH > __current_frame.width) {
            cx = x;
            cy += font::GLYPH_HEIGHT;
        }

        if (cy + font::GLYPH_HEIGHT > __current_frame.height) break;

        draw_char(cx, cy, *text, fg, bg);
        cx += font::GLYPH_WIDTH;
    }
}

}  // namespace fb
