#pragma once

#include <bit>

#include "font8x16.hh"

namespace fb {

union Pixel {
    u32 value;
    gfx::Color color;
};

struct Framebuffer {
    Pixel* pixels;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bits_per_pixel;
    u8 type;
};

static Framebuffer __current_frame;
static bool __framebuffer_initialized;

static auto get_pixel(u32 x, u32 y) -> gfx::Color {
    debug_assert(__current_frame.pixels != nullptr);

    const usize stride = __current_frame.pitch / sizeof(u32);
    return __current_frame.pixels[y * stride + x].color;
}

static auto set_pixel(u32 x, u32 y, gfx::Color color) -> void {
    debug_assert(__current_frame.pixels != nullptr);

    const usize stride = __current_frame.pitch / sizeof(u32);
    __current_frame.pixels[y * stride + x].color = color;
}

struct Gfx_Backend {
    [[nodiscard]] static auto initialize(const mem::Multiboot2_Info* mbi) -> bool {
        if (__framebuffer_initialized) return true;

        const auto* framebuffer_tag = mem::find_multiboot2_framebuffer_tag(mbi);
        if (framebuffer_tag == nullptr || framebuffer_tag->framebuffer_addr == 0) return false;

        __current_frame.pixels = reinterpret_cast<Pixel*>((uintptr_t)framebuffer_tag->framebuffer_addr);
        __current_frame.pitch = framebuffer_tag->framebuffer_pitch;
        __current_frame.width = framebuffer_tag->framebuffer_width;
        __current_frame.height = framebuffer_tag->framebuffer_height;
        __current_frame.bits_per_pixel = framebuffer_tag->framebuffer_bpp;
        __current_frame.type = framebuffer_tag->framebuffer_type;
        assert(__current_frame.bits_per_pixel == 32, "Only 32BPP supported.");

        __framebuffer_initialized = true;
        return true;
    }

    static auto get_pixel(u32 x, u32 y) -> gfx::Color {
        return fb::get_pixel(x, y);
    }

    static auto set_pixel(u32 x, u32 y, gfx::Color color) -> void {
        fb::set_pixel(x, y, color);
    }

    static auto width() -> u32 {
        return __current_frame.width;
    }

    static auto height() -> u32 {
        return __current_frame.height;
    }
};

// struct Sprite {};

auto draw_char(u32 x, u32 y, char c, gfx::Color fg, gfx::Color bg) -> void {
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
            //        Which might not be easy.. but hey.. what's easy here?
            //
            const auto color = bits & (0b1000'0000 >> col) ? fg : bg;
            set_pixel(px, py, color);
        }
    }
}

auto draw_text(u32 x, u32 y, const char* text, gfx::Color fg = gfx::WHITE, gfx::Color bg = gfx::BLACK) -> void {
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
