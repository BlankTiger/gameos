#pragma once

#include <algorithm>

#include "config.hh"

#include "kstd/array.hh"
#include "kstd/allocator.hh"
#include "kstd/font8x16.hh"
#include "kstd/math.hh"
#include "kstd/matrix.hh"
#include "kstd/multiboot2.hh"
#include "kstd/resource.hh"
#include "kstd/string.hh"

namespace gfx {

// -------------------------------------------------------------------
//  Common -- Color, Pixel, depth
// -------------------------------------------------------------------

struct Color {
    u8 r, g, b, a;

    auto with_alpha(u8 alpha) -> Color {
        return {r, g, b, alpha};
    }
};

constexpr Color BLACK{0, 0, 0, 255};
constexpr Color WHITE{255, 255, 255, 255};
constexpr Color RED{255, 0, 0, 255};
constexpr Color GREEN{0, 255, 0, 255};
constexpr Color BLUE{0, 0, 255, 255};
constexpr Color YELLOW{255, 255, 0, 255};
constexpr Color CYAN{0, 255, 255, 255};
constexpr Color MAGENTA{255, 0, 255, 255};
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
        if (fg.a == 0) return;
        if (fg.a == 255) {
            raw = (static_cast<u32>(fg.r) << fmt.red_pos)   |
                  (static_cast<u32>(fg.g) << fmt.green_pos) |
                  (static_cast<u32>(fg.b) << fmt.blue_pos)  |
                  (static_cast<u32>(0xFF) << Framebuffer_Format::alpha_pos);
            return;
        }

        u8 r = static_cast<u8>((raw >> fmt.red_pos)   & 0xFF);
        u8 g = static_cast<u8>((raw >> fmt.green_pos) & 0xFF);
        u8 b = static_cast<u8>((raw >> fmt.blue_pos)  & 0xFF);

        r = math::lerp(r, fg.r, fg.a, 255);
        g = math::lerp(g, fg.g, fg.a, 255);
        b = math::lerp(b, fg.b, fg.a, 255);

        raw = (static_cast<u32>(r)    << fmt.red_pos)   |
              (static_cast<u32>(g)    << fmt.green_pos) |
              (static_cast<u32>(b)    << fmt.blue_pos)  |
              (static_cast<u32>(0xFF) << Framebuffer_Format::alpha_pos);
    }
};

using Depth = u32;
constexpr Depth DEPTH_FAR = static_cast<Depth>(-1);

using namespace math;

// -------------------------------------------------------------------
//  Common -- 2D command types
// -------------------------------------------------------------------

enum struct Draw_Command_2D_Type: u8 {
    DRAW_CHAR,
    DRAW_TEXT,
    DRAW_CIRCLE,
    DRAW_LINE,
    DRAW_RAW_LINE,
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
    Depth depth = DEPTH_FAR;

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

// -------------------------------------------------------------------
//  Common -- 3D command types
// -------------------------------------------------------------------

struct Vertex {
    Vector3<f32> position;
    Color        color;
    Vector2<f32> uv;
};

using Index = Vector3<u32>;

struct Mesh {
    Array_View<Vertex> vertices;
    Array_View<Index>  indices;
};

struct Mesh_Instance {
    Mesh model;
    Resource_View texture{};
    Vector3<f32> translation;
    Quaternion<f32> rotation;
    Vector3<f32> scale;
    Matrix4<f32> transform;

    auto recompute_matrix() -> void {
        transform = make_translation_matrix4(translation) * make_rotation_matrix<Matrix4<f32>>(rotation) * make_scale_matrix4(scale);
    }

    Mesh_Instance(Mesh model) : model(model), scale{1.f, 1.f, 1.f}, transform(Matrix4<f32>::identity()) {}
    Mesh_Instance(Mesh model, Vector3<f32> translation = {}, Quaternion<f32> rotation = {}, Vector3<f32> scale = {1.f, 1.f, 1.f})
        : model(model), translation(translation), rotation(rotation), scale(scale)
        { recompute_matrix(); }
    Mesh_Instance(Mesh model, Resource_View texture, Vector3<f32> translation = {}, Quaternion<f32> rotation = {}, Vector3<f32> scale = {1.f, 1.f, 1.f})
        : model(model), texture(texture), translation(translation), rotation(rotation), scale(scale) { recompute_matrix(); }

    auto rotate(Vector3<f32> spin_axis, f32 angle) {
        rotation = Quaternion<f32>::from_axis_angle(spin_axis, angle) * rotation;
        recompute_matrix();
    }
};

struct Camera3D {
    Vector3<f32> position;
    Quaternion<f32> rotation;
    Matrix4<f32> M_camera;

    auto recompute_matrix() -> void {
        InverseResult M_camera_rotation_inv = inverse(make_rotation_matrix<Matrix4<f32>>(rotation));
        InverseResult M_camera_translation_inv = inverse(make_translation_matrix4(position));
        M_camera = M_camera_rotation_inv.result * M_camera_translation_inv.result;
    }

    constexpr Camera3D() : position(), rotation() { recompute_matrix(); }
};

enum struct Draw_Command_3D_Type: u8 {
    DRAW_WIREFRAME,
    DRAW_MESH,
};

struct Mesh_Command {
    Mesh_Instance instance;
};

struct Wireframe_Command {
    Mesh_Instance instance;
};

struct Draw_Command_3D {
    Draw_Command_3D_Type type;
    Camera3D camera;

    union {
        Mesh_Command mesh;
        Wireframe_Command wireframe;
    };
};

// -------------------------------------------------------------------
//  Common -- Framebuffer, double buffering, set_pixel, clear
// -------------------------------------------------------------------

struct Framebuffer {
    Array_View<Pixel, GFX_PIXEL_COUNT> pixels;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bits_per_pixel;
    u8 type;
    usize stride;
};

namespace hidden {
    constexpr usize GFX_ARENA_SIZE = 16 * 1024;
    constexpr u8 FRAME_OVERLAP = 2;

    struct Frame_Data {
        Static_Array<Pixel, GFX_PIXEL_COUNT> back_buffer;
        Static_Array<Depth, GFX_PIXEL_COUNT> depth_buffer;
        mem::Arena_Allocator<> arena{GFX_ARENA_SIZE};
        Array<Draw_Command_2D> draw_commands_ui{256};
        Array<Draw_Command_2D> draw_commands_world_2d{256};
        Array<Draw_Command_3D> draw_commands_world_3d{256};
    };

    inline Static_Array<Frame_Data, FRAME_OVERLAP> frames;
    inline u8 frame_number = 0;

    [[nodiscard]] force_inline auto current_slot() -> u8 {
        return frame_number % FRAME_OVERLAP;
    }
}

[[nodiscard]] force_inline auto current_frame() -> hidden::Frame_Data& {
    return hidden::frames[hidden::current_slot()];
}

namespace hidden {
    inline Framebuffer front_buffer;
    inline bool framebuffer_initialized;
}

force_inline auto is_initialized() -> bool {
    return hidden::framebuffer_initialized;
}

force_inline auto width() -> u32 {
    return hidden::front_buffer.width;
}

force_inline auto height() -> u32 {
    return hidden::front_buffer.height;
}

force_inline auto swap_buffers() -> void {
    using namespace hidden;
    kstd_debug_assert(front_buffer.pixels.data != nullptr);
    auto& current = current_frame();
    kstd_memcpy(front_buffer.pixels.data, current.back_buffer.data, front_buffer.pixels.size_in_bytes);
    ++frame_number;
    auto& next = current_frame();
    kstd_memset32(next.back_buffer.data, 0, next.back_buffer.size_in_bytes / sizeof(Pixel));
    kstd_memset32(next.depth_buffer.data, DEPTH_FAR, next.depth_buffer.size_in_bytes / sizeof(Depth));
}

[[nodiscard]] auto initialize(const boot::Multiboot2_Info* mbi) -> bool {
    using namespace hidden;
    if (framebuffer_initialized) return true;

    const auto* framebuffer_tag = boot::find_multiboot2_tag<boot::Multiboot2_Framebuffer_Tag>(mbi);
    if (framebuffer_tag == nullptr || framebuffer_tag->framebuffer_addr == 0) return false;

    auto* frontbuffer_pixels = reinterpret_cast<Pixel*>(framebuffer_tag->framebuffer_addr);
    front_buffer = {
        .pixels = Array_View<Pixel, GFX_PIXEL_COUNT>{frontbuffer_pixels},
        .pitch = framebuffer_tag->framebuffer_pitch,
        .width = framebuffer_tag->framebuffer_width,
        .height = framebuffer_tag->framebuffer_height,
        .bits_per_pixel = framebuffer_tag->framebuffer_bpp,
        .type = framebuffer_tag->framebuffer_type,
        .stride = framebuffer_tag->framebuffer_pitch / sizeof(u32),
    };
    kstd_assert(front_buffer.bits_per_pixel == 32, "Only 32BPP supported.");
    kstd_assert(GFX_PIXEL_COUNT == front_buffer.width * front_buffer.height);
    framebuffer_fmt.init(*framebuffer_tag);
    kstd_assert(!(framebuffer_fmt.red_pos == 0 && framebuffer_fmt.green_pos == 0 && framebuffer_fmt.blue_pos == 0),
                "Framebuffer_Format was not initialized");

    static_assert(sizeof(Pixel) == sizeof(u32));
    static_assert(sizeof(Depth) == sizeof(u32));
    for (auto& frame : frames) {
        kstd_memset32(frame.back_buffer.data, 0, frame.back_buffer.size_in_bytes / sizeof(Pixel));
        kstd_memset32(frame.depth_buffer.data, DEPTH_FAR, frame.depth_buffer.size_in_bytes / sizeof(Depth));
        frame.arena.reset();
    }
    kstd_memcpy(front_buffer.pixels.data, frames[0].back_buffer.data, front_buffer.pixels.size_in_bytes);
    framebuffer_initialized = true;
    return true;
}

template <bool IMMEDIATE>
static force_inline auto set_pixel(u32 x, u32 y, Color color) -> void {
    kstd_assert(x < width());
    kstd_assert(y < height());
    auto index = y * hidden::front_buffer.stride + x;
    auto& frame = current_frame();
    if constexpr (IMMEDIATE) hidden::front_buffer.pixels[index].blend_with(color);
    frame.back_buffer[index].blend_with(color);
}

static force_inline auto set_pixel(u32 x, u32 y, Color color, Depth depth) -> void {
    kstd_assert(x < width());
    kstd_assert(y < height());
    auto index = y * hidden::front_buffer.stride + x;
    auto& frame = current_frame();
    if (depth != DEPTH_FAR && depth >= frame.depth_buffer[index]) return;
    frame.back_buffer[index].blend_with(color);
    if (depth != DEPTH_FAR) frame.depth_buffer[index] = depth;
}

auto clear(Color color) -> void {
    for (u32 y = 0; y < height(); ++y)
        for (u32 x = 0; x < width(); ++x)
            set_pixel(x, y, color, DEPTH_FAR);
    auto& depth_buffer = current_frame().depth_buffer;
    kstd_memset32(depth_buffer.data, DEPTH_FAR, depth_buffer.size_in_bytes / sizeof(Depth));
}

enum struct Render_Pass : u8 {
    WORLD_2D,
    WORLD_3D,
    UI,
};

// -------------------------------------------------------------------
//  2D -- primitives, raster UI, command-queue execution
// -------------------------------------------------------------------

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
    auto& queue = (pass == Render_Pass::WORLD_2D) ? current_frame().draw_commands_world_2d : current_frame().draw_commands_ui;
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
    auto& arena = current_frame().arena;
    auto copied = copy_string(text, &arena);
    auto& queue = (pass == Render_Pass::WORLD_2D) ? current_frame().draw_commands_world_2d : current_frame().draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type  = Draw_Command_2D_Type::DRAW_TEXT,
            .z     = z,
            .depth = depth,
            .text  = Text_Command{x, y, copied, fg, bg},
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

auto inner_draw_circle(u32 x, u32 y, u32 r, Color color, Depth depth = DEPTH_FAR) -> void {
    u32 x1 = (x > r) ? x - r: 0;
    u32 y1 = (y > r) ? y - r: 0;
    u32 x2 = x + r + 1;
    u32 y2 = y + r + 1;

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
            else {
                if (AA_RES <= 1) continue;
                u32 in_circle = 0;
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
    auto& queue = (pass == Render_Pass::WORLD_2D) ? current_frame().draw_commands_world_2d : current_frame().draw_commands_ui;
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
constexpr s32 FIXED_POINT_ONE   = 1 << FIXED_POINT_SHIFT;

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

    s32 dx = static_cast<s32>(x2 - x1);
    s32 dy = static_cast<s32>(y2) - static_cast<s32>(y1);

    s32 gradient = 0;
    if (dx != 0) gradient = (dy << FIXED_POINT_SHIFT) / dx;

    s32 curr_y = inner_draw_line_endpoint(x1, y1, steep, color, depth) + gradient;

    (void) inner_draw_line_endpoint(x2, y2, steep, color, depth);

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
    if (width() < 2 || height() < 2) return;
    if (x1 >= width() || y1 >= height()) return;
    if (x2 >= width() || y2 >= height()) return;
    u32 max_x = width()  - 2;
    u32 max_y = height() - 2;
    x1 = std::min(x1, max_x);
    y1 = std::min(y1, max_y);
    x2 = std::min(x2, max_x);
    y2 = std::min(y2, max_y);

    auto& queue = (pass == Render_Pass::WORLD_2D) ? current_frame().draw_commands_world_2d : current_frame().draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type  = Draw_Command_2D_Type::DRAW_LINE,
            .z     = z,
            .depth = depth,
            .line  = Line_Command{x1, y1, x2, y2, color}
        }
    );
}

auto inner_draw_raw_line(u32 x1, u32 y1, u32 x2, u32 y2, Color color, Depth depth = DEPTH_FAR) -> void {
    bool steep = abs_diff(y1, y2) > abs_diff(x1, x2);
    if (steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }
    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
    }

    s32 dx = static_cast<s32>(x2 - x1);
    s32 dy = abs(static_cast<s32>(y2) - static_cast<s32>(y1));

    s32 y_step = y1 < y2 ? 1 : -1;
    s32 error = dx / 2;
    s32 y = static_cast<s32>(y1);

    for (u32 x = x1; x <= x2; ++x) {
        if (steep) {
            set_pixel(y, x, color, depth);
        }
        else {
            set_pixel(x, y, color, depth);
        }

        error -= dy;
        if (error < 0) {
            y += y_step;
            error += dx;
        }
    }
}

auto draw_raw_line(u32 x1, u32 y1, u32 x2, u32 y2, Color color, u8 z = 1, Render_Pass pass = Render_Pass::UI, Depth depth = DEPTH_FAR) -> void {
    if (x1 >= width() || y1 >= height()) return;
    if (x2 >= width() || y2 >= height()) return;
    u32 max_x = width()  - 2;
    u32 max_y = height() - 2;
    x1 = std::min(x1, max_x);
    y1 = std::min(y1, max_y);
    x2 = std::min(x2, max_x);
    y2 = std::min(y2, max_y);

    auto& queue = (pass == Render_Pass::WORLD_2D) ? current_frame().draw_commands_world_2d : current_frame().draw_commands_ui;
    queue.push_back(
        Draw_Command_2D{
            .type  = Draw_Command_2D_Type::DRAW_RAW_LINE,
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
    auto& queue = (pass == Render_Pass::WORLD_2D) ? current_frame().draw_commands_world_2d : current_frame().draw_commands_ui;
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
    auto& queue = (pass == Render_Pass::WORLD_2D) ? current_frame().draw_commands_world_2d : current_frame().draw_commands_ui;
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

auto inner_draw_triangle(Vector4<f32> v1, Vector4<f32> v2, Vector4<f32> v3, Color color, Depth depth = DEPTH_FAR) -> void {
    Rect bounding_box{v1, v2, v3};
    bounding_box.clip(width(), height());

    f32 dx12 = v2.x - v1.x;
    f32 dy12 = v2.y - v1.y;
    f32 dx23 = v3.x - v2.x;
    f32 dy23 = v3.y - v2.y;
    f32 dx31 = v1.x - v3.x;
    f32 dy31 = v1.y - v3.y;

    f32 start_x = bounding_box.x1 + 0.5f;
    f32 start_y = bounding_box.y1 + 0.5f;
    f32 e12 = dx12 * (start_y - v1.y) - dy12 * (start_x - v1.x);
    f32 e23 = dx23 * (start_y - v2.y) - dy23 * (start_x - v2.x);
    f32 e31 = dx31 * (start_y - v3.y) - dy31 * (start_x - v3.x);

    bool tl12 = is_top_left(dx12, dy12);
    bool tl23 = is_top_left(dx23, dy23);
    bool tl31 = is_top_left(dx31, dy31);

    f32 row12 = e12;
    f32 row23 = e23;
    f32 row31 = e31;
    for (u32 y = bounding_box.y1; y < bounding_box.y2; ++y) {
        f32 e12 = row12;
        f32 e23 = row23;
        f32 e31 = row31;
        for (u32 x = bounding_box.x1; x < bounding_box.x2; ++x) {
            if (is_inside(e12, tl12) && is_inside(e23, tl23) && is_inside(e31, tl31))
                set_pixel(x, y, color, depth);
            e12 -= dy12;
            e23 -= dy23;
            e31 -= dy31;
        }
        row12 += dx12;
        row23 += dx23;
        row31 += dx31;
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
    current_frame().draw_commands_world_2d.push_back(
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
        std::stable_sort(current_frame().draw_commands_ui.begin(), current_frame().draw_commands_ui.end(),
            [](const Draw_Command_2D& a, const Draw_Command_2D& b) {
                return a.z < b.z;
            }
        );
    }

    for (const auto& command : current_frame().draw_commands_ui) {
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
            case DRAW_RAW_LINE: {
                const Line_Command& cmd = command.line;
                inner_draw_raw_line(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.color, command.depth);
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

    current_frame().draw_commands_ui.clear();
}

force_inline auto draw_world_2D() -> void {
    for (const auto& command : current_frame().draw_commands_world_2d) {
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
            case DRAW_RAW_LINE: {
                const Line_Command& cmd = command.line;
                inner_draw_raw_line(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.color, command.depth);
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
                inner_draw_triangle(cmd.v1, cmd.v2, cmd.v3, cmd.color, command.depth);
            } break;
        }
    }

    current_frame().draw_commands_world_2d.clear();
}

// -------------------------------------------------------------------
//  3D -- camera, projection, meshes, texturing, raster and commands
// -------------------------------------------------------------------

static Static_Array<Vertex, 24> unit_cube_vertices{{
    {{ 1,  1,  1}, BLUE,    {1.f, 0.f}}, {{-1,  1,  1}, RED,     {0.f, 0.f}}, {{-1, -1,  1}, BLUE,    {0.f, 1.f}}, {{ 1, -1,  1}, RED,     {1.f, 1.f}},
    {{ 1,  1, -1}, GREEN,   {1.f, 0.f}}, {{ 1,  1,  1}, GREEN,   {0.f, 0.f}}, {{ 1, -1,  1}, GREEN,   {0.f, 1.f}}, {{ 1, -1, -1}, GREEN,   {1.f, 1.f}},
    {{-1,  1, -1}, RED,     {0.f, 0.f}}, {{ 1,  1, -1}, RED,     {1.f, 0.f}}, {{ 1, -1, -1}, RED,     {1.f, 1.f}}, {{-1, -1, -1}, RED,     {0.f, 1.f}},
    {{-1,  1,  1}, YELLOW,  {1.f, 0.f}}, {{-1,  1, -1}, YELLOW,  {0.f, 0.f}}, {{-1, -1, -1}, YELLOW,  {0.f, 1.f}}, {{-1, -1,  1}, YELLOW,  {1.f, 1.f}},
    {{ 1,  1, -1}, CYAN,    {1.f, 0.f}}, {{-1,  1, -1}, CYAN,    {0.f, 0.f}}, {{-1,  1,  1}, CYAN,    {0.f, 1.f}}, {{ 1,  1,  1}, CYAN,    {1.f, 1.f}},
    {{-1, -1,  1}, MAGENTA, {0.f, 0.f}}, {{-1, -1, -1}, MAGENTA, {0.f, 1.f}}, {{ 1, -1, -1}, MAGENTA, {1.f, 1.f}}, {{ 1, -1,  1}, MAGENTA, {1.f, 0.f}},
}};

static Static_Array<Index, 12> unit_cube_indices{{
    { 0,  1,  2}, { 0,  2,  3},
    { 4,  5,  6}, { 4,  6,  7},
    { 8,  9, 10}, { 8, 10, 11},
    {12, 13, 14}, {12, 14, 15},
    {16, 17, 18}, {16, 18, 19},
    {20, 21, 22}, {20, 22, 23},
}};
const inline Mesh UNIT_CUBE{unit_cube_vertices, unit_cube_indices};

force_inline auto update_camera(Camera3D& camera, Vector3<f32> new_position = {}, Quaternion<f32> new_rotation = {}) -> void {
    bool needs_update = false;
    if (camera.position != new_position) {
        needs_update = true;
        camera.position = new_position;
    }
    if (camera.rotation != new_rotation) {
        needs_update = true;
        camera.rotation = new_rotation;
    }
    if (!needs_update) return;
    camera.recompute_matrix();
}

struct Projection_Settings {
    f32 vertical_fov_degrees = 90.f;
    f32 aspect_ratio = GFX_ASPECT_RATIO;
    f32 z_near = 1.f, z_far = 10.f;
    f32 x_offset = 0.f, y_offset = 0.f;
    bool depth_range_01 = false;
    Matrix4<f32> M_projection;

    auto recompute_matrix() -> void {
        constexpr f32 DEG_TO_RAD = 3.14159265358979323846f / 180.f;
        M_projection = make_projection_matrix(
            vertical_fov_degrees * DEG_TO_RAD,
            aspect_ratio,
            z_near, z_far,
            x_offset, y_offset,
            depth_range_01
        );
    }

    constexpr Projection_Settings() { recompute_matrix(); }
};

namespace hidden {
    inline Projection_Settings projection;
}

force_inline auto sample_texture(const Resource_View& res, f32 u, f32 v) -> Color {
    u = std::clamp(u, f32(0), f32(1));
    v = std::clamp(v, f32(0), f32(1));
    const u32 tx = static_cast<u32>(u * static_cast<f32>(res.width  - 1));
    const u32 ty = static_cast<u32>(v * static_cast<f32>(res.height - 1));
    const Color* colors = reinterpret_cast<const Color*>(res.data.data);
    return colors[ty * res.width + tx];
}

auto inner_draw_triangle_3d(
    Vector4<f32> v1, Vector4<f32> v2, Vector4<f32> v3,
    Vector2<f32> uv1, Vector2<f32> uv2, Vector2<f32> uv3,
    Color color1, Color color2, Color color3,
    Resource_View texture = {}
) -> void {
    Vector4<f32> a = v2 - v1;
    Vector4<f32> b = v3 - v1;
    f32 det = det_xy(a, b);
    bool is_counter_clockwise = det < 0.0f;

    if (cull_mode == Cull_Mode::BACK_FACE && !is_counter_clockwise) return;

    if (is_counter_clockwise) {
        std::swap(v2, v3);
        std::swap(uv2, uv3);
        det = -det;
    }

    Rect bounding_box{v1, v2, v3};
    bounding_box.clip(width(), height());

    f32 dx12 = v2.x - v1.x;
    f32 dy12 = v2.y - v1.y;
    f32 dx23 = v3.x - v2.x;
    f32 dy23 = v3.y - v2.y;
    f32 dx31 = v1.x - v3.x;
    f32 dy31 = v1.y - v3.y;

    f32 area = dx12 * (v3.y - v1.y) - dy12 * (v3.x - v1.x);
    if (area == 0.f) return;

    f32 inv_area = 1.f / area;

    f32 start_x = bounding_box.x1 + 0.5f;
    f32 start_y = bounding_box.y1 + 0.5f;

    f32 e12 = dx12 * (start_y - v1.y) - dy12 * (start_x - v1.x);
    f32 e23 = dx23 * (start_y - v2.y) - dy23 * (start_x - v2.x);
    f32 e31 = dx31 * (start_y - v3.y) - dy31 * (start_x - v3.x);

    bool tl12 = is_top_left(dx12, dy12);
    bool tl23 = is_top_left(dx23, dy23);
    bool tl31 = is_top_left(dx31, dy31);

    const bool textured = texture.width != 0 && texture.height != 0;

    f32 iw1 = v1.w != 0.f ? 1.f / v1.w : 0.f;
    f32 iw2 = v2.w != 0.f ? 1.f / v2.w : 0.f;
    f32 iw3 = v3.w != 0.f ? 1.f / v3.w : 0.f;

    Plane z_plane = make_plane(v1.z, v2.z, v3.z, v1, v2, v3, inv_area, start_x, start_y);
    Plane iw_plane = make_plane(iw1, iw2, iw3, v1, v2, v3, inv_area, start_x, start_y);

    Plane u_plane = make_plane(
        uv1.x * iw1,
        uv2.x * iw2,
        uv3.x * iw3,
        v1, v2, v3,
        inv_area,
        start_x,
        start_y
    );

    Plane v_plane = make_plane(
        uv1.y * iw1,
        uv2.y * iw2,
        uv3.y * iw3,
        v1, v2, v3,
        inv_area,
        start_x,
        start_y
    );

    Plane r_plane = make_plane(color1.r, color2.r, color3.r, v1, v2, v3, inv_area, start_x, start_y);
    Plane g_plane = make_plane(color1.g, color2.g, color3.g, v1, v2, v3, inv_area, start_x, start_y);
    Plane b_plane = make_plane(color1.b, color2.b, color3.b, v1, v2, v3, inv_area, start_x, start_y);
    Plane a_plane = make_plane(color1.a, color2.a, color3.a, v1, v2, v3, inv_area, start_x, start_y);

    f32 row12 = e12;
    f32 row23 = e23;
    f32 row31 = e31;

    f32 row_z = z_plane.value;
    f32 row_iw = iw_plane.value;
    f32 row_u = u_plane.value;
    f32 row_v = v_plane.value;

    f32 row_r = r_plane.value;
    f32 row_g = g_plane.value;
    f32 row_b = b_plane.value;
    f32 row_a = a_plane.value;

    for (u32 y = bounding_box.y1; y < bounding_box.y2; ++y) {
        f32 e12 = row12;
        f32 e23 = row23;
        f32 e31 = row31;

        f32 z = row_z;
        f32 iw = row_iw;

        f32 u = row_u;
        f32 v = row_v;

        f32 r = row_r;
        f32 g = row_g;
        f32 b = row_b;
        f32 a = row_a;

        for (u32 x = bounding_box.x1; x < bounding_box.x2; ++x) {
            if (is_inside(e12, tl12) &&
                is_inside(e23, tl23) &&
                is_inside(e31, tl31)) {

                Color color{
                    static_cast<u8>(std::clamp(r, f32(0), f32(255))),
                    static_cast<u8>(std::clamp(g, f32(0), f32(255))),
                    static_cast<u8>(std::clamp(b, f32(0), f32(255))),
                    static_cast<u8>(std::clamp(a, f32(0), f32(255)))
                };

                if (textured) {
                    f32 recip = 1.f / iw;
                    set_pixel(
                        x,
                        y,
                        sample_texture(texture, u * recip, v * recip),
                        z
                    );
                } else {
                    set_pixel(x, y, color, z);
                }
            }

            e12 -= dy12;
            e23 -= dy23;
            e31 -= dy31;

            z += z_plane.dx;
            iw += iw_plane.dx;

            u += u_plane.dx;
            v += v_plane.dx;

            r += r_plane.dx;
            g += g_plane.dx;
            b += b_plane.dx;
            a += a_plane.dx;
        }

        row12 += dx12;
        row23 += dx23;
        row31 += dx31;

        row_z += z_plane.dy;
        row_iw += iw_plane.dy;

        row_u += u_plane.dy;
        row_v += v_plane.dy;

        row_r += r_plane.dy;
        row_g += g_plane.dy;
        row_b += b_plane.dy;
        row_a += a_plane.dy;
    }
}

auto clip_to_screen(Vector4<f32> clip) -> Vector4<f32> {
    f32 inv_w = 1.f / clip.w;
    f32 ndc_x = clip.x * inv_w;
    f32 ndc_y = clip.y * inv_w;
    f32 ndc_z = clip.z * inv_w;
    f32 sx = (ndc_x * 0.5f + 0.5f) * static_cast<f32>(width());
    f32 sy = (1.f - (ndc_y * 0.5f + 0.5f)) * static_cast<f32>(height());
    return Vector4<f32>{sx, sy, ndc_z, clip.w};
}

force_inline static auto project(Mesh_Instance instance, Camera3D camera) -> Array<Vector4<f32>> {
    Matrix4<f32> M = hidden::projection.M_projection * camera.M_camera * instance.transform;
    const usize vertex_count = instance.model.vertices.size;
    Array<Vector4<f32>> projected_positions(vertex_count, Vector4<f32>::zero());
    for (usize i = 0; i < vertex_count; ++i) {
        Vector4<f32> clip = multiply(M, Vector4<f32>::as_point(instance.model.vertices[i].position));
        if (clip.w == 0.f) {
            projected_positions[i] = Vector4<f32>::zero();
            continue;
        }
        projected_positions[i] = clip_to_screen(clip);
    }
    return projected_positions;
}

auto inner_draw_mesh(Mesh_Instance instance, Camera3D camera) -> void {
    auto projected_positions = project(instance, camera);
    const bool textured = instance.texture.width != 0 && instance.texture.height != 0;
    for (const auto& [i1, i2, i3]: instance.model.indices) {
        const Vector4<f32>& p1 = projected_positions[i1];
        const Vector4<f32>& p2 = projected_positions[i2];
        const Vector4<f32>& p3 = projected_positions[i3];
        if (p1.w <= 0.f || p2.w <= 0.f || p3.w <= 0.f) continue;
        const Vertex& a = instance.model.vertices[i1];
        const Vertex& b = instance.model.vertices[i2];
        const Vertex& c = instance.model.vertices[i3];
        if (textured) {
            inner_draw_triangle_3d(p1, p2, p3, a.uv, b.uv, c.uv, a.color, b.color, c.color, instance.texture);
        } else {
            inner_draw_triangle_3d(p1, p2, p3, {}, {}, {}, a.color, b.color, c.color);
        }
    }
}

auto draw_mesh(Mesh_Instance instance, Camera3D camera) -> void {
    current_frame().draw_commands_world_3d.push_back(
        Draw_Command_3D{
            .type = Draw_Command_3D_Type::DRAW_MESH,
            .camera = camera,
            .mesh = Mesh_Command{.instance = instance}
        }
    );
}

auto inner_draw_raw_line_3d(
    Vector4<f32> p1,
    Vector4<f32> p2,
    Color color
) -> void {
    u32 x1 = static_cast<u32>(p1.x);
    u32 y1 = static_cast<u32>(p1.y);
    u32 x2 = static_cast<u32>(p2.x);
    u32 y2 = static_cast<u32>(p2.y);

    bool steep = abs_diff(y1, y2) > abs_diff(x1, x2);
    if (steep) {
        std::swap(x1, y1);
        std::swap(x2, y2);
    }
    if (x1 > x2) {
        std::swap(x1, x2);
        std::swap(y1, y2);
        std::swap(p1, p2);
    }

    s32 dx = static_cast<s32>(x2 - x1);
    s32 dy = abs(static_cast<s32>(y2) - static_cast<s32>(y1));

    s32 y_step = y1 < y2 ? 1 : -1;
    s32 error = dx / 2;
    s32 y = static_cast<s32>(y1);

    f32 dz = dx != 0
        ? (p2.z - p1.z) / static_cast<f32>(dx)
        : 0.f;

    f32 z = p1.z;

    for (u32 x = x1; x <= x2; ++x) {
        if (steep) {
            set_pixel(y, x, color, z);
        }
        else {
            set_pixel(x, y, color, z);
        }

        if (dx != 0)
            z += dz;

        error -= dy;
        if (error < 0) {
            y += y_step;
            error += dx;
        }
    }
}

auto inner_draw_wireframe(Mesh_Instance instance, Camera3D camera) -> void {
    auto projected_positions = project(instance, camera);
    for (const auto& [i1, i2, i3]: instance.model.indices) {
        const Vector4<f32>& p1 = projected_positions[i1];
        const Vector4<f32>& p2 = projected_positions[i2];
        const Vector4<f32>& p3 = projected_positions[i3];
        if (p1.w <= 0.f || p2.w <= 0.f || p3.w <= 0.f) continue;
        const Vertex& a = instance.model.vertices[i1];
        const Vertex& b = instance.model.vertices[i2];
        inner_draw_raw_line_3d(p1, p2, a.color);
        inner_draw_raw_line_3d(p1, p3, a.color);
        inner_draw_raw_line_3d(p2, p3, b.color);
    }
}

auto draw_wireframe(Mesh_Instance instance, Camera3D camera) -> void {
    current_frame().draw_commands_world_3d.push_back(
        Draw_Command_3D{
            .type = Draw_Command_3D_Type::DRAW_WIREFRAME,
            .camera = camera,
            .wireframe = Wireframe_Command{.instance = instance}
        }
    );
}


force_inline auto draw_world_3D() -> void {
    for (const auto& command : current_frame().draw_commands_world_3d) {
        using enum Draw_Command_3D_Type;
        switch (command.type) {
            case DRAW_MESH: {
                const Mesh_Command& cmd = command.mesh;
                inner_draw_mesh(cmd.instance, command.camera);
            } break;
            case DRAW_WIREFRAME: {
                const Mesh_Command& cmd = command.mesh;
                inner_draw_wireframe(cmd.instance, command.camera);
            } break;
        }
    }
    current_frame().draw_commands_world_3d.clear();
}

// -------------------------------------------------------------------
//  Frame -- top-level drawing orchestration
// -------------------------------------------------------------------

auto draw_frame() -> void {
    draw_world_3D();
    draw_world_2D();
    draw_ui();
    swap_buffers();
    current_frame().arena.reset();
}

}  // namespace gfx
