#pragma once

#include <bit>

#include "config.hh"
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
    usize stride;
};

static Framebuffer front_buffer;
static Pixel back_buffer[GFX_PIXEL_COUNT];
constexpr usize BUFFER_SIZE = GFX_PIXEL_COUNT * sizeof(Pixel);
static bool __framebuffer_initialized;

force_inline auto swap_buffers() -> void {
    debug_assert(front_buffer.pixels != nullptr);
    memcpy(front_buffer.pixels, back_buffer, BUFFER_SIZE);
}

[[nodiscard]] auto initialize(const boot::Multiboot2_Info* mbi) -> bool {
    if (__framebuffer_initialized) return true;

    const auto* framebuffer_tag = boot::find_multiboot2_framebuffer_tag(mbi);
    if (framebuffer_tag == nullptr || framebuffer_tag->framebuffer_addr == 0) return false;

    front_buffer.pixels = reinterpret_cast<Pixel*>(framebuffer_tag->framebuffer_addr);
    front_buffer.pitch = framebuffer_tag->framebuffer_pitch;
    front_buffer.width = framebuffer_tag->framebuffer_width;
    front_buffer.height = framebuffer_tag->framebuffer_height;
    front_buffer.bits_per_pixel = framebuffer_tag->framebuffer_bpp;
    front_buffer.type = framebuffer_tag->framebuffer_type;
    front_buffer.stride = front_buffer.pitch / sizeof(u32);
    assert(front_buffer.bits_per_pixel == 32, "Only 32BPP supported.");

    memset(back_buffer, 0, BUFFER_SIZE);
    swap_buffers();

    __framebuffer_initialized = true;
    return true;
}

force_inline auto is_initialized() -> bool {
    return __framebuffer_initialized;
}

force_inline auto width() -> u32 {
    return front_buffer.width;
}

force_inline auto height() -> u32 {
    return front_buffer.height;
}

static auto set_pixel(u32 x, u32 y, gfx::Color color) -> void {
    back_buffer[y * front_buffer.stride + x].color.blend_with(color);
}

auto draw_char(u32 x, u32 y, char c, gfx::Color fg, gfx::Color bg) -> void {
    const auto index = static_cast<u8>(c);
    const auto& glyph = font::DATA[index];

    for (u32 row = 0; row < font::GLYPH_HEIGHT; ++row) {
        const u32 py = y + row;
        if (py >= front_buffer.height) break;

        const font::Glyph_Width bits = glyph[row];
        for (u32 col = 0; col < font::GLYPH_WIDTH; ++col) {
            const u32 px = x + col;
            if (px >= front_buffer.width) break;

            const auto color = bits & (0b1000'0000 >> col) ? fg : bg;
            set_pixel(px, py, color);
        }
    }
}

auto draw_text(u32 x, u32 y, const char* text, gfx::Color fg = gfx::WHITE, gfx::Color bg = gfx::BLACK) -> void {
    u32 cx = x;
    u32 cy = y;
    u32 frame_width = front_buffer.width;
    u32 frame_height = front_buffer.height;

    for (; *text; ++text) {
        if (*text == '\n') {
            cx = x;
            cy += font::GLYPH_HEIGHT;
            continue;
        }

        if (cx + font::GLYPH_WIDTH > frame_width) {
            cx = x;
            cy += font::GLYPH_HEIGHT;
        }

        if (cy + font::GLYPH_HEIGHT > frame_height) break;

        draw_char(cx, cy, *text, fg, bg);
        cx += font::GLYPH_WIDTH;
    }
}

}  // namespace fb
