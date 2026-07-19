#pragma once

#include "math.hh"

namespace gfx {

struct Color {
    u8 b, g, r, w;

    force_inline auto blend(Color bg) const -> Color {
        return {
            .b = math::lerp(bg.b, b, w, 255),
            .g = math::lerp(bg.g, g, w, 255),
            .r = math::lerp(bg.r, r, w, 255),
            .w = 255,
        };
    }
};

constexpr Color BLACK{0, 0, 0, 255};
constexpr Color WHITE{255, 255, 255, 255};
constexpr Color RED{0, 0, 255, 255};
constexpr Color GREEN{0, 255, 0, 255};
constexpr Color BLUE{255, 0, 0, 255};
constexpr Color TRANSPARENT{0, 0, 0, 0};

}  // namespace gfx

#include "framebuffer/framebuffer.hh"
#include "framebuffer/term.hh"

namespace gfx {

[[nodiscard]] static auto initialize(const boot::Multiboot2_Info* mbi) -> bool {
    return fb::initialize(mbi);
}

auto is_initialized() -> bool {
    return fb::is_initialized();
}

auto clear(Color color) -> void {
    for (u32 y = 0; y < fb::height(); ++y) {
        for (u32 x = 0; x < fb::width(); ++x) {
            fb::set_pixel(x, y, color);
        }
    }
}

auto draw_rect(u32 x, u32 y, u32 w, u32 h, Color color) -> void {
    for (u32 _y = y; _y < y + h && _y < fb::height(); ++_y) {
        for (u32 _x = x; _x < x + w && _x < fb::width(); ++_x) {
            fb::set_pixel(_x, _y, color);
        }
    }
}

#ifndef AA_RES
#define AA_RES 8
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
    if (x2 < x) x2 = fb::width();
    if (y2 < y) y2 = fb::height();

    u32 color_alpha = color.w;
    for (usize i = 0; i < colors_table.size; ++i) {
        colors_table[i] = Color{
            .b = color.b,
            .g = color.g,
            .r = color.r,
            .w = (u8) ((color_alpha * i / AA_RES_POW2) & 0x000000FF),
        };
    }

    u32 inner_r = (r > 1) ? (r - 1) * (r - 1) : 0;
    u32 outer_r = (r + 1) * (r + 1);

    for (u32 py = y1; py < y2 && py < fb::height(); ++py) {
        for (u32 px = x1; px < x2 && px < fb::width(); ++px) {
            u32 dx = (px > x) ? px - x : x - px;
            u32 dy = (py > y) ? py - y : y - py;
            u32 d2 = dx * dx + dy * dy;

            if (d2 <= inner_r) {
                fb::set_pixel(px, py, color);
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
                const Color blended_color = colors_table[in_circle].blend(fb::get_pixel(px, py));
                fb::set_pixel(px, py, blended_color);
            }
        }
    }
}

}  // namespace gfx
