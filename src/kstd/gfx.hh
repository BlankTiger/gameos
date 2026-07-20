#pragma once

#include <bit>

#include "math.hh"
#include "config.hh"
#include "font8x16.hh"

namespace gfx {

struct Color {
    u8 b, g, r, a;

    force_inline void blend_with(Color fg) {
        b = math::lerp(b, fg.b, fg.a, 255);
        g = math::lerp(g, fg.g, fg.a, 255);
        r = math::lerp(r, fg.r, fg.a, 255);
        a = 255;
    }
};

constexpr Color BLACK{0, 0, 0, 255};
constexpr Color WHITE{255, 255, 255, 255};
constexpr Color RED{0, 0, 255, 255};
constexpr Color GREEN{0, 255, 0, 255};
constexpr Color BLUE{255, 0, 0, 255};
constexpr Color TRANSPARENT{0, 0, 0, 0};

union Pixel {
    u32 value;
    Color color;
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

template <bool IMMEDIATE = false>
static force_inline auto set_pixel(u32 x, u32 y, Color color) -> void {
    auto index = y * front_buffer.stride + x;
    if constexpr(IMMEDIATE) {
        front_buffer.pixels[index].color.blend_with(color);
        back_buffer[index].color.blend_with(color);
    } else {
        back_buffer[index].color.blend_with(color);
    }
}

template <bool IMMEDIATE>
auto inner_draw_char(u32 x, u32 y, char c, Color fg, Color bg) -> void {
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
            set_pixel<IMMEDIATE>(px, py, color);
        }
    }
}

template <typename... Args>
auto draw_char(Args&&... args) -> void {
    inner_draw_char<false>(std::forward<Args>(args)...);
}

template <typename... Args>
auto draw_char_immediate(Args&&... args) -> void {
    inner_draw_char<true>(std::forward<Args>(args)...);
}

auto draw_text(u32 x, u32 y, const char* text, Color fg = WHITE, Color bg = BLACK) -> void {
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

auto clear(Color color) -> void {
    for (u32 y = 0; y < height(); ++y) {
        for (u32 x = 0; x < width(); ++x) {
            set_pixel(x, y, color);
        }
    }
}

auto draw_frame() -> void {
    swap_buffers();
}

auto draw_rect(u32 x, u32 y, u32 w, u32 h, Color color) -> void {
    for (u32 _y = y; _y < y + h && _y < height(); ++_y) {
        for (u32 _x = x; _x < x + w && _x < width(); ++_x) {
            set_pixel(_x, _y, color);
        }
    }
}

#ifndef AA_RES
#define AA_RES 4
#endif

constexpr static u32 AA_RES_POW2 = AA_RES * AA_RES;
constexpr static u32 AA_RES1 = AA_RES + 1;
constexpr static u32 AA_RES1_POW2 = AA_RES1 * AA_RES1;
static Static_Array<Color, AA_RES_POW2 + 1> colors_table;

// TODO: Can we make generic AA?
auto draw_circle(u32 x, u32 y, u32 r, Color color) -> void {
    u32 x1 = (x > r) ? x - r: 0;
    u32 y1 = (y > r) ? y - r: 0;
    u32 x2 = x + r + 1;
    u32 y2 = y + r + 1;

    // Overflows
    if (x2 < x) x2 = width();
    if (y2 < y) y2 = height();

    u32 color_alpha = color.a;
    for (usize i = 0; i < colors_table.size; ++i) {
        colors_table[i] = Color{
            .b = color.b,
            .g = color.g,
            .r = color.r,
            .a = static_cast<u8>((color_alpha * i / AA_RES_POW2) & 0x00000100),
        };
    }

    u32 inner_r = (r > 1) ? (r - 1) * (r - 1) : 0;
    u32 outer_r = (r + 1) * (r + 1);

    for (u32 py = y1; py < y2 && py < height(); ++py) {
        for (u32 px = x1; px < x2 && px < width(); ++px) {
            u32 dx = (px > x) ? px - x : x - px;
            u32 dy = (py > y) ? py - y : y - py;
            u32 d2 = dx * dx + dy * dy;

            if (d2 <= inner_r) {
                set_pixel(px, py, color);
            }
            else if (d2 >= outer_r) {
                continue;
            }
            else { // Aliasing
                if (AA_RES <= 1) continue;
                int in_circle = 0;
                for (u32 off_x = 0; off_x < AA_RES; ++off_x) {
                    for (u32 off_y = 0; off_y < AA_RES; ++off_y) {
                        u32 sx = px * AA_RES1 * 2 + off_x * 2 + 2;
                        u32 sy = py * AA_RES1 * 2 + off_y * 2 + 2;
                        u32 cx = x * AA_RES1 * 2 + AA_RES1;
                        u32 cy = y * AA_RES1 * 2 + AA_RES1;

                        u32 dx = (sx > cx) ? (sx - cx) : (cx - sx);
                        u32 dy = (sy > cy) ? (sy - cy) : (cy - sy);
                        if (dx * dx + dy * dy <= r * r * AA_RES1_POW2 * 4) in_circle += 1;
                    }
                }
                set_pixel(px, py, colors_table[in_circle]);
            }
        }
    }
}

}  // namespace gfx
