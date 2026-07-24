#pragma once

#include <algorithm>
#include <bit>

#include "math.hh"
#include "config.hh"
#include "font8x16.hh"
#include "multiboot2.hh"
#include "string_view.hh"
#include "resource.hh"

namespace gfx {

struct Color {
    u8 r, g, b, a;
};

constexpr Color BLACK{0, 0, 0, 255};
constexpr Color WHITE{255, 255, 255, 255};
constexpr Color RED{255, 0, 0, 255};
constexpr Color GREEN{0, 255, 0, 255};
constexpr Color BLUE{0, 0, 255, 255};
constexpr Color TRANSPARENT{0, 0, 0, 0};

struct Framebuffer_Format {
    u8 red_pos;
    u8 green_pos;
    u8 blue_pos;
    static constexpr u8 alpha_pos = 24;

    auto init(const boot::Multiboot2_Framebuffer_Tag& tag) -> void {
        red_pos   = tag.framebuffer_info.direct_color.framebuffer_red_field_position;
        green_pos = tag.framebuffer_info.direct_color.framebuffer_green_field_position;
        blue_pos  = tag.framebuffer_info.direct_color.framebuffer_blue_field_position;
    }
};

inline Framebuffer_Format framebuffer_fmt;

struct Pixel {
    u32 raw;

    Pixel() : raw(0) {}
    Pixel(Color c) {
        raw = (static_cast<u32>(c.r) << framebuffer_fmt.red_pos)   |
              (static_cast<u32>(c.g) << framebuffer_fmt.green_pos) |
              (static_cast<u32>(c.b) << framebuffer_fmt.blue_pos)  |
              (static_cast<u32>(c.a) << Framebuffer_Format::alpha_pos);
    }

    auto color() -> Color {
        return {
            .r = static_cast<u8>((raw >> framebuffer_fmt.red_pos)   & 0xFF),
            .g = static_cast<u8>((raw >> framebuffer_fmt.green_pos) & 0xFF),
            .b = static_cast<u8>((raw >> framebuffer_fmt.blue_pos)  & 0xFF),
            .a = static_cast<u8>((raw >> Framebuffer_Format::alpha_pos) & 0xFF),
        };
    }

    operator u32() const { return raw; }

    force_inline auto blend_with(Color fg, const Framebuffer_Format& fmt = framebuffer_fmt) -> void {
        u8 r = static_cast<u8>((raw >> fmt.red_pos)   & 0xFF);
        u8 g = static_cast<u8>((raw >> fmt.green_pos) & 0xFF);
        u8 b = static_cast<u8>((raw >> fmt.blue_pos)  & 0xFF);

        r = math::lerp(r, fg.r, fg.a, 255);
        g = math::lerp(g, fg.g, fg.a, 255);
        b = math::lerp(b, fg.b, fg.a, 255);

        raw = (static_cast<u32>(r) << fmt.red_pos)   |
              (static_cast<u32>(g) << fmt.green_pos) |
              (static_cast<u32>(b) << fmt.blue_pos)  |
              (0xFF << Framebuffer_Format::alpha_pos);
    }
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

inline Framebuffer front_buffer;
inline Pixel back_buffer[GFX_PIXEL_COUNT];
constexpr usize BUFFER_SIZE = GFX_PIXEL_COUNT * sizeof(Pixel);
inline bool __framebuffer_initialized;

force_inline auto swap_buffers() -> void {
    kstd_debug_assert(front_buffer.pixels != nullptr);
    kstd_memcpy(front_buffer.pixels, back_buffer, BUFFER_SIZE);
}

[[nodiscard]] auto initialize(const boot::Multiboot2_Info* mbi) -> bool {
    if (__framebuffer_initialized) return true;

    const auto* framebuffer_tag = boot::find_multiboot2_framebuffer_tag(mbi);
    if (framebuffer_tag == nullptr || framebuffer_tag->framebuffer_addr == 0) return false;

    front_buffer.pixels         = reinterpret_cast<Pixel*>(framebuffer_tag->framebuffer_addr);
    front_buffer.pitch          = framebuffer_tag->framebuffer_pitch;
    front_buffer.width          = framebuffer_tag->framebuffer_width;
    front_buffer.height         = framebuffer_tag->framebuffer_height;
    front_buffer.bits_per_pixel = framebuffer_tag->framebuffer_bpp;
    front_buffer.type           = framebuffer_tag->framebuffer_type;
    front_buffer.stride         = front_buffer.pitch / sizeof(u32);
    kstd_assert(front_buffer.bits_per_pixel == 32, "Only 32BPP supported.");
    framebuffer_fmt.init(*framebuffer_tag);
    kstd_assert(
        !(framebuffer_fmt.red_pos == 0 && framebuffer_fmt.green_pos == 0 && framebuffer_fmt.blue_pos == 0),
        "Framebuffer_Format was not initialized"
    );

    kstd_memset(back_buffer, 0, BUFFER_SIZE);
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
    kstd_assert(x < width());
    kstd_assert(y < height());

    auto index = y * front_buffer.stride + x;
    if constexpr(IMMEDIATE) {
        front_buffer.pixels[index].blend_with(color);
        back_buffer[index].blend_with(color);
    } else {
        back_buffer[index].blend_with(color);
    }
}

enum class Draw_Command_Type: u8 {
    DRAW_CHAR,
    DRAW_TEXT, // Probably we will need to copy text
    DRAW_RECT,
    DRAW_CIRCLE,
    DRAW_SPRITE,
};

struct Char_Command {
    u32 x, y;
    char c;
    Color fg, bg;
};

struct Text_Command {
    u32 x, y;
    string_view text;
    Color fg, bg;
};

struct Rect_Command {
    u32 x, y, w, h;
    Color color;
};

struct Circle_Command {
    u32 x, y, r;
    Color color;
};

struct Sprite_Command {
    Resource_View res;
    u32 x, y;
};

struct Draw_Command {
    Draw_Command_Type type;
    u8 z;

    union {
        Char_Command character;
        Text_Command text;
        Rect_Command rectangle;
        Circle_Command circle;
        Sprite_Command sprite;
    };
};

static Array<Draw_Command> draw_commands(512);

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
auto draw_char(Args&&... args, u8 z = 1) -> void {
    draw_commands.push_back(
        Draw_Command{
            .type = Draw_Command_Type::DRAW_CHAR,
            .z = z,
            .character = Char_Command{std::forward<Args>(args)...},
        }
    );
}

template <typename... Args>
auto draw_char_immediate(Args&&... args) -> void {
    inner_draw_char<true>(std::forward<Args>(args)...);
}

auto inner_draw_text(u32 x, u32 y, string_view text, Color fg = WHITE, Color bg = TRANSPARENT) -> void {
    u32 cx = x;
    u32 cy = y;
    u32 frame_width = front_buffer.width;
    u32 frame_height = front_buffer.height;

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

        inner_draw_char<false>(cx, cy, c, fg, bg);
        cx += font::GLYPH_WIDTH;
    }
}

auto draw_text(u32 x, u32 y, string_view text, Color fg = WHITE, Color bg = TRANSPARENT, u8 z = 1) -> void {
    draw_commands.push_back(
        Draw_Command{
            .type = Draw_Command_Type::DRAW_TEXT,
            .z = z,
            .text = Text_Command{x, y, text, fg, bg},
        }
    );
}

auto clear(Color color) -> void {
    for (u32 y = 0; y < height(); ++y) {
        for (u32 x = 0; x < width(); ++x) {
            set_pixel(x, y, color);
        }
    }
}

auto inner_draw_rect(u32 x, u32 y, u32 w, u32 h, Color color) -> void {
    for (u32 _y = y; _y < y + h && _y < height(); ++_y) {
        for (u32 _x = x; _x < x + w && _x < width(); ++_x) {
            set_pixel(_x, _y, color);
        }
    }
}

auto draw_rect(u32 x, u32 y, u32 w, u32 h, Color color, u8 z = 1) -> void {
    draw_commands.push_back(
        Draw_Command{
            .type = Draw_Command_Type::DRAW_RECT,
            .z = z,
            .rectangle = Rect_Command{x, y, w, h, color},
        }
    );
}

#ifndef AA_RES
#define AA_RES 4
#endif

constexpr static u32 AA_RES_POW2 = AA_RES * AA_RES;
constexpr static u32 AA_RES1 = AA_RES + 1;
constexpr static u32 AA_RES1_POW2 = AA_RES1 * AA_RES1;
static Static_Array<Color, AA_RES_POW2 + 1> colors_table;

// TODO: Can we make generic AA?
auto inner_draw_circle(u32 x, u32 y, u32 r, Color color) -> void {
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

auto draw_circle(u32 x, u32 y, u32 r, Color color, u8 z = 1) -> void {
    draw_commands.push_back(
        Draw_Command{
            .type = Draw_Command_Type::DRAW_CIRCLE,
            .z = z,
            .circle = Circle_Command{x, y, r, color},
        }
    );
}

auto inner_draw_sprite(const Resource_View res, u32 x, u32 y) -> void {
    if (res.width == 0 || res.height == 0) return;

    const Color* colors = reinterpret_cast<const Color*>(res.data);
    u32 clipped_width  = (x + res.width  >= width())  ? (width() - x)  : res.width;
    u32 clipped_height = (y + res.height >= height()) ? (height() - y) : res.height;
    for (u32 py = 0; py < clipped_height; ++py) {
        for (u32 px = 0; px < clipped_width; ++px) {
            set_pixel(x + px, y + py, colors[py * res.width + px]);
        }
    }
}

auto draw_sprite(const Resource_View res, u32 x, u32 y, u8 z = 1) -> void {
    draw_commands.push_back(
        Draw_Command{
            .type = Draw_Command_Type::DRAW_SPRITE,
            .z = z,
            .sprite = Sprite_Command{res, x, y},
        }
    );
}

template <bool z_sort = true>
auto draw_frame() -> void {
    if (z_sort) {
        std::stable_sort(draw_commands.begin(), draw_commands.end(),
            [](const Draw_Command& a, const Draw_Command& b) {
                return a.z < b.z;
            }
        );
    }
    for (const auto& command: draw_commands) {
        using enum Draw_Command_Type;
        switch (command.type) {
            case DRAW_CHAR: {
                const Char_Command& cmd = command.character;
                inner_draw_char<false>(cmd.x, cmd.y, cmd.c, cmd.fg, cmd.bg);
            } break;
            case DRAW_TEXT: {
                const Text_Command& cmd = command.text;
                inner_draw_text(cmd.x, cmd.y, cmd.text, cmd.fg, cmd.bg);
            } break;
            case DRAW_RECT: {
                const Rect_Command& cmd = command.rectangle;
                inner_draw_rect(cmd.x, cmd.y, cmd.w, cmd.h, cmd.color);
            } break;
            case DRAW_CIRCLE: {
                const Circle_Command& cmd = command.circle;
                inner_draw_circle(cmd.x, cmd.y, cmd.r, cmd.color);
            } break;
            case DRAW_SPRITE: {
                const Sprite_Command& cmd = command.sprite;
                inner_draw_sprite(cmd.res, cmd.x, cmd.y);
            } break;
        }
    }
    swap_buffers();
}

}  // namespace gfx
