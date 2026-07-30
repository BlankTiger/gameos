#pragma once

#include <algorithm>

#include "kstd/font8x16.hh"
#include "kstd/math.hh"
#include "kstd/resource.hh"
#include "kstd/string.hh"

#include "common.hh"

namespace gfx {

using namespace math;
namespace hidden = gfx::hidden;

enum struct Draw_Command_2D_Type: u8 {
    DRAW_CHAR,
    DRAW_TEXT, // Probably we will need to copy text
    DRAW_CIRCLE,
    DRAW_LINE,
    DRAW_RECT,
    DRAW_SPRITE,
    DRAW_TRIANGLE,
};

struct Char_Command {
    u32 x, y;
    char c;
    Color fg, bg;
};

struct Text_Command {
    u32 x, y;
    string text;
    Color fg, bg;
};

struct Circle_Command {
    u32 x, y, r;
    Color color;
};

struct Line_Command {
    u32 x1, y1, x2, y2;
    Color color;
};

struct Rect_Command {
    u32 x, y, w, h;
    Color color;
};

struct Sprite_Command {
    Resource_View res;
    u32 x, y;
};

struct Triangle_Command {
    Vector4<f32> v1, v2, v3;
    Color color;
};

struct Draw_Command_2D {
    Draw_Command_2D_Type type;
    u8 z;
    Depth depth = DEPTH_FAR; // only used when queued into draw_commands_world_2D

    union {
        Char_Command   character;
        Text_Command   text;
        Circle_Command circle;
        Line_Command   line;
        Rect_Command   rectangle;
        Sprite_Command sprite;
        Triangle_Command triangle;
    };
};

inline Array<Draw_Command_2D> draw_commands_ui(256);
inline Array<Draw_Command_2D> draw_commands_world_2D(256);

template <bool IMMEDIATE>
auto inner_draw_char(u32 x, u32 y, char c, Color fg, Color bg, Depth depth = DEPTH_FAR) -> void {
    const auto index = static_cast<u8>(c);
    const auto& glyph = font::DATA[index];

    for (u32 row = 0; row < font::GLYPH_HEIGHT; ++row) {
        const u32 py = y + row;
        if (py >= hidden::front_buffer.height) break;

        const font::Glyph_Width bits = glyph[row];
        for (u32 col = 0; col < font::GLYPH_WIDTH; ++col) {
            const u32 px = x + col;
            if (px >= hidden::front_buffer.width) break;

            const auto color = bits & (0b1000'0000 >> col) ? fg : bg;
            if constexpr (IMMEDIATE) {
                set_pixel<true>(px, py, color);
            } else {
                set_pixel(px, py, color, depth);
            }
        }
    }
}

auto draw_char(u32 x, u32 y, char c, Color fg, Color bg, u8 z = 1, Render_Pass pass = Render_Pass::UI, Depth depth = DEPTH_FAR) -> void {
    if (x >= width() || y >= height()) return;
    auto& queue = (pass == Render_Pass::WORLD_2D) ? draw_commands_world_2D : draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type      = Draw_Command_2D_Type::DRAW_CHAR,
            .z         = z,
            .depth     = depth,
            .character = Char_Command{x, y, c, fg, bg},
        }
    );
}

template <typename... Args>
auto draw_char_immediate(Args&&... args) -> void {
    inner_draw_char<true>(std::forward<Args>(args)...);
}

auto inner_draw_text(u32 x, u32 y, string text, Color fg = WHITE, Color bg = TRANSPARENT, Depth depth = DEPTH_FAR) -> void {
    u32 cx = x;
    u32 cy = y;
    u32 frame_width  = hidden::front_buffer.width;
    u32 frame_height = hidden::front_buffer.height;

    for (const auto c: text) {
        if (c == '\n') {
            cx = x;
            cy += font::GLYPH_HEIGHT;
            continue;
        }

        if (cx + font::GLYPH_WIDTH > frame_width) {
            cx = x;
            cy += font::GLYPH_HEIGHT;
        }

        if (cy + font::GLYPH_HEIGHT > frame_height) break;

        inner_draw_char<false>(cx, cy, c, fg, bg, depth);
        cx += font::GLYPH_WIDTH;
    }
}

auto draw_text(u32 x, u32 y, const string text, Color fg = WHITE, Color bg = TRANSPARENT, u8 z = 1, Render_Pass pass = Render_Pass::UI, Depth depth = DEPTH_FAR) -> void {
    auto& queue = (pass == Render_Pass::WORLD_2D) ? draw_commands_world_2D : draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type  = Draw_Command_2D_Type::DRAW_TEXT,
            .z     = z,
            .depth = depth,
            .text  = Text_Command{x, y, text, fg, bg},
        }
    );
}

#ifndef AA_RES
#define AA_RES 4
#endif

constexpr static u32 AA_RES_POW2 = AA_RES * AA_RES;
constexpr static u32 AA_RES1 = AA_RES + 1;
constexpr static u32 AA_RES1_POW2 = AA_RES1 * AA_RES1;
inline Static_Array<Color, AA_RES_POW2 + 1> colors_table;

// TODO: Can we make generic AA?
auto inner_draw_circle(u32 x, u32 y, u32 r, Color color, Depth depth = DEPTH_FAR) -> void {
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
            .r = color.r,
            .g = color.g,
            .b = color.b,
            .a = static_cast<u8>(color_alpha * i / AA_RES_POW2),
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
                set_pixel(px, py, color, depth);
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
                set_pixel(px, py, colors_table[in_circle], depth);
            }
        }
    }
}

auto draw_circle(u32 x, u32 y, u32 r, Color color, u8 z = 1, Render_Pass pass = Render_Pass::UI, Depth depth = DEPTH_FAR) -> void {
    if (x >= width() || y >= height()) return;
    auto& queue = (pass == Render_Pass::WORLD_2D) ? draw_commands_world_2D : draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type   = Draw_Command_2D_Type::DRAW_CIRCLE,
            .z      = z,
            .depth  = depth,
            .circle = Circle_Command{x, y, r, color},
        }
    );
}

constexpr s32 FIXED_POINT_SHIFT = 8;
constexpr s32 FIXED_POINT_ONE   = 1 << FIXED_POINT_SHIFT; // 256

force_inline s32 fixed_point_floor(s32 x) {
    return x >> FIXED_POINT_SHIFT;
}

force_inline s32 fractional_part(s32 x) {
    return x & (FIXED_POINT_ONE - 1);
}

force_inline s32 reverse_fractional_part(s32 x) {
    return FIXED_POINT_ONE - fractional_part(x);
}

force_inline u8 alpha(s32 x) {
    return static_cast<u8>((x * 255) >> FIXED_POINT_SHIFT);
}

auto inner_draw_line_endpoint(u32 x, u32 y, bool steep, Color color, Depth depth = DEPTH_FAR) -> s32 {
    s32 sy = static_cast<s32>(y) << FIXED_POINT_SHIFT;
    s32 _y = fixed_point_floor(sy);
    s32 rev_frac_sy_alpha = alpha(reverse_fractional_part(sy));
    s32 frac_sy_alpha = alpha(fractional_part(sy));

    if (steep) {
        set_pixel(_y,     x, color.with_alpha(rev_frac_sy_alpha), depth);
        set_pixel(_y + 1, x, color.with_alpha(frac_sy_alpha),     depth);
    }
    else {
        set_pixel(x, _y,     color.with_alpha(rev_frac_sy_alpha), depth);
        set_pixel(x, _y + 1, color.with_alpha(frac_sy_alpha),     depth);
    }

    return sy;
}

auto inner_draw_line(u32 x1, u32 y1, u32 x2, u32 y2, Color color, Depth depth = DEPTH_FAR) -> void {
    bool steep = abs_diff(y1, y2) > abs_diff(x1, x2);
    if (steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }
    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }

    u32 dx = x2 - x1;
    s32 dy = static_cast<s32>(y2) - static_cast<s32>(y1);

    s32 gradient = 0;
    if (dx != 0) gradient = (dy << FIXED_POINT_SHIFT) / static_cast<s32>(dx);

    // First point
    s32 curr_y = inner_draw_line_endpoint(x1, y1, steep, color, depth) + gradient;

    // Second point
    (void) inner_draw_line_endpoint(x2, y2, steep, color, depth);

    // Main loop
    if (steep) {
        for (u32 x = x1 + 1; x < x2; ++x) {
            s32 y = fixed_point_floor(curr_y);
            s32 rev_frac_curr_y_alpha = alpha(reverse_fractional_part(curr_y));
            s32 frac_curr_y_alpha = alpha(fractional_part(curr_y));
            set_pixel(y, x, color.with_alpha(rev_frac_curr_y_alpha), depth);
            set_pixel(y + 1, x, color.with_alpha(frac_curr_y_alpha), depth);
            curr_y += gradient;
        }
    }
    else {
        for (u32 x = x1 + 1; x < x2; ++x) {
            s32 y = fixed_point_floor(curr_y);
            s32 rev_frac_curr_y_alpha = alpha(reverse_fractional_part(curr_y));
            s32 frac_curr_y_alpha = alpha(fractional_part(curr_y));
            set_pixel(x, y, color.with_alpha(rev_frac_curr_y_alpha), depth);
            set_pixel(x, y + 1, color.with_alpha(frac_curr_y_alpha), depth);
            curr_y += gradient;
        }
    }
}

auto draw_line(u32 x1, u32 y1, u32 x2, u32 y2, Color color, u8 z = 1, Render_Pass pass = Render_Pass::UI, Depth depth = DEPTH_FAR) -> void {
    if (x2 >= width() || y2 >= height()) return;
    auto& queue = (pass == Render_Pass::WORLD_2D) ? draw_commands_world_2D : draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type  = Draw_Command_2D_Type::DRAW_LINE,
            .z     = z,
            .depth = depth,
            .line  = Line_Command{x1, y1, x2, y2, color}
        }
    );
}

auto inner_draw_rect(u32 x, u32 y, u32 w, u32 h, Color color, Depth depth = DEPTH_FAR) -> void {
    u32 clipped_width  = (x + w  >= width())  ? (width() - x)  : w;
    u32 clipped_height = (y + h >= height())  ? (height() - y) : h;
    for (u32 _y = y; _y < y +  clipped_height; ++_y) {
        for (u32 _x = x; _x < x + clipped_width; ++_x) {
            set_pixel(_x, _y, color, depth);
        }
    }
}

auto draw_rect(u32 x, u32 y, u32 w, u32 h, Color color, u8 z = 1, Render_Pass pass = Render_Pass::UI, Depth depth = DEPTH_FAR) -> void {
    if (x >= width() || y >= height()) return;
    auto& queue = (pass == Render_Pass::WORLD_2D) ? draw_commands_world_2D : draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type      = Draw_Command_2D_Type::DRAW_RECT,
            .z         = z,
            .depth     = depth,
            .rectangle = Rect_Command{x, y, w, h, color},
        }
    );
}

auto inner_draw_sprite(const Resource_View res, u32 x, u32 y, Depth depth = DEPTH_FAR) -> void {
    const Color* colors = reinterpret_cast<const Color*>(res.data.data);
    u32 clipped_width  = (x + res.width  >= width())  ? (width() - x)  : res.width;
    u32 clipped_height = (y + res.height >= height()) ? (height() - y) : res.height;
    for (u32 py = 0; py < clipped_height; ++py) {
        for (u32 px = 0; px < clipped_width; ++px) {
            set_pixel(x + px, y + py, colors[py * res.width + px], depth);
        }
    }
}

auto draw_sprite(const Resource_View res, u32 x, u32 y, u8 z = 1, Render_Pass pass = Render_Pass::UI, Depth depth = DEPTH_FAR) -> void {
    if (res.width == 0 || res.height == 0) return;
    if (x >= width() || y >= height()) return;
    auto& queue = (pass == Render_Pass::WORLD_2D) ? draw_commands_world_2D : draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type   = Draw_Command_2D_Type::DRAW_SPRITE,
            .z      = z,
            .depth  = depth,
            .sprite = Sprite_Command{res, x, y},
        }
    );
}

auto is_top_left(f32 dx, f32 dy) -> bool {
    return dy > 0.f || (dy == 0.f && dx < 0.f);
}

auto is_inside(f32 e, bool top_left) -> bool {
    return e > 0.f || (e == 0.f && top_left);
};

// Possible improvements:
// - Iterate over chunks of pixels and check if bounding vertices are out of the triangle
// - Barycentric coordinates & fragments?
auto inner_draw_triangle(Vector4<f32> v1, Vector4<f32> v2, Vector4<f32> v3, Color color) -> void {
    Rect bounding_box{v1, v2, v3};
    bounding_box.clip(width(), height());

    // Precompute edge deltas
    f32 dx12 = v2.x - v1.x;
    f32 dy12 = v2.y - v1.y;
    f32 dx23 = v3.x - v2.x;
    f32 dy23 = v3.y - v2.y;
    f32 dx31 = v1.x - v3.x;
    f32 dy31 = v1.y - v3.y;

    // Compute the original determinant orientations once:
    // det_xy(edge, point - start)
    f32 start_x = bounding_box.x1 + 0.5f;
    f32 start_y = bounding_box.y1 + 0.5f;
    f32 e12 = dx12 * (start_y - v1.y) - dy12 * (start_x - v1.x);
    f32 e23 = dx23 * (start_y - v2.y) - dy23 * (start_x - v2.x);
    f32 e31 = dx31 * (start_y - v3.y) - dy31 * (start_x - v3.x);

    // Top-left fill rule checking
    bool tl12 = is_top_left(dx12, dy12);
    bool tl23 = is_top_left(dx23, dy23);
    bool tl31 = is_top_left(dx31, dy31);

    // Increment edge values instead of recomputing determinants
    f32 row12 = e12;
    f32 row23 = e23;
    f32 row31 = e31;
    for (u32 y = bounding_box.y1; y < bounding_box.y2; ++y) {
        f32 e12 = row12;
        f32 e23 = row23;
        f32 e31 = row31;
        for (u32 x = bounding_box.x1; x < bounding_box.x2; ++x) {
            if (is_inside(e12, tl12) && is_inside(e23, tl23) && is_inside(e31, tl31))
                set_pixel(x, y, color);
            // Move one pixel right
            e12 += dx12;
            e23 += dx23;
            e31 += dx31;
        }
        // Move one pixel down
        row12 -= dy12;
        row23 -= dy23;
        row31 -= dy31;
    }
}

enum struct Cull_Mode : u8 {
    NONE,
    BACK_FACE,
};
inline auto cull_mode = Cull_Mode::BACK_FACE;

auto draw_triangle(Vector4<f32> v1, Vector4<f32> v2, Vector4<f32> v3, Color color, u8 z = 1) -> void {
    Vector4<f32> a = v2 - v1;
    Vector4<f32> b = v3 - v1;
    f32 det = det_xy(a, b);
    bool is_counter_clockwise = det < 0.0f;

    switch (cull_mode) {
        case Cull_Mode::NONE: break;
        case Cull_Mode::BACK_FACE: {
            if (!is_counter_clockwise) return;
        } break;
    }

    if (is_counter_clockwise) {
        std::swap(v2, v3);
        det = -det;
    }
    draw_commands_world_2D.push_back(
        Draw_Command_2D{
            .type = Draw_Command_2D_Type::DRAW_TRIANGLE,
            .z = z,
            .triangle = Triangle_Command{v1, v2, v3, color},
        }
    );
}

template <bool z_sort = true>
force_inline auto draw_ui() -> void {
    if (z_sort) {
        std::stable_sort(draw_commands_ui.begin(), draw_commands_ui.end(),
            [](const Draw_Command_2D& a, const Draw_Command_2D& b) {
                return a.z < b.z;
            }
        );
    }

    for (const auto& command : draw_commands_ui) {
        using enum Draw_Command_2D_Type;
        switch (command.type) {
            case DRAW_CHAR: {
                const Char_Command& cmd = command.character;
                inner_draw_char<false>(cmd.x, cmd.y, cmd.c, cmd.fg, cmd.bg);
            } break;
            case DRAW_TEXT: {
                const Text_Command& cmd = command.text;
                inner_draw_text(cmd.x, cmd.y, cmd.text, cmd.fg, cmd.bg);
            } break;
            case DRAW_CIRCLE: {
                const Circle_Command& cmd = command.circle;
                inner_draw_circle(cmd.x, cmd.y, cmd.r, cmd.color);
            } break;
            case DRAW_LINE: {
                const Line_Command& cmd = command.line;
                inner_draw_line(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.color);
            } break;
            case DRAW_RECT: {
                const Rect_Command& cmd = command.rectangle;
                inner_draw_rect(cmd.x, cmd.y, cmd.w, cmd.h, cmd.color);
            } break;
            case DRAW_SPRITE: {
                const Sprite_Command& cmd = command.sprite;
                inner_draw_sprite(cmd.res, cmd.x, cmd.y);
            } break;
            case DRAW_TRIANGLE: {
                const Triangle_Command& cmd = command.triangle;
                inner_draw_triangle(cmd.v1, cmd.v2, cmd.v3, cmd.color);
            } break;
        }
    }

    draw_commands_ui.clear();
}

force_inline auto draw_world_2D() -> void {
    for (const auto& command : draw_commands_world_2D) {
        using enum Draw_Command_2D_Type;
        switch (command.type) {
            case DRAW_CHAR: {
                const Char_Command& cmd = command.character;
                inner_draw_char<false>(cmd.x, cmd.y, cmd.c, cmd.fg, cmd.bg, command.depth);
            } break;
            case DRAW_TEXT: {
                const Text_Command& cmd = command.text;
                inner_draw_text(cmd.x, cmd.y, cmd.text, cmd.fg, cmd.bg, command.depth);
            } break;
            case DRAW_CIRCLE: {
                const Circle_Command& cmd = command.circle;
                inner_draw_circle(cmd.x, cmd.y, cmd.r, cmd.color, command.depth);
            } break;
            case DRAW_LINE: {
                const Line_Command& cmd = command.line;
                inner_draw_line(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.color, command.depth);
            } break;
            case DRAW_RECT: {
                const Rect_Command& cmd = command.rectangle;
                inner_draw_rect(cmd.x, cmd.y, cmd.w, cmd.h, cmd.color, command.depth);
            } break;
            case DRAW_SPRITE: {
                const Sprite_Command& cmd = command.sprite;
                inner_draw_sprite(cmd.res, cmd.x, cmd.y, command.depth);
            } break;
            case DRAW_TRIANGLE: {
                const Triangle_Command& cmd = command.triangle;
                inner_draw_triangle(cmd.v1, cmd.v2, cmd.v3, cmd.color);
            } break;
        }
    }

    draw_commands_world_2D.clear();
}

}  // namespace gfx
