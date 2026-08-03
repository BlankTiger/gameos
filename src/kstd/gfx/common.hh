#pragma once

#include "config.hh"

#include "kstd/array.hh"
#include "kstd/math.hh"
#include "kstd/multiboot2.hh"

namespace gfx {

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

struct Framebuffer {
    Array_View<Pixel, GFX_PIXEL_COUNT> pixels;
    u32 pitch;
    u32 width;
    u32 height;
    u8 bits_per_pixel;
    u8 type;
    usize stride;
};

// TODO: reimplement so that all drawing algorithms calculate depth
using Depth = u32;
constexpr Depth DEPTH_FAR = static_cast<Depth>(-1); // 0xFFFFFFFF, memset-able with 0xFF

namespace hidden {
    inline Framebuffer front_buffer;
    // @TODO(blanktiger): Should probably be runtime allocated cause this takes more than 6MB off of our stack.
    inline Static_Array<Pixel, GFX_PIXEL_COUNT> back_buffer;
    inline Static_Array<Depth, GFX_PIXEL_COUNT> depth_buffer; // smaller means closer
    inline bool framebuffer_initialized;
}

force_inline auto swap_buffers() -> void {
    using namespace hidden;

    kstd_debug_assert(front_buffer.pixels.data != nullptr);
    kstd_memcpy(front_buffer.pixels.data, back_buffer.data, front_buffer.pixels.size_in_bytes);
}

[[nodiscard]] auto initialize(const boot::Multiboot2_Info* mbi) -> bool {
    using namespace hidden;

    if (framebuffer_initialized) return true;

    const auto* framebuffer_tag = boot::find_multiboot2_tag<boot::Multiboot2_Framebuffer_Tag>(mbi);
    if (framebuffer_tag == nullptr || framebuffer_tag->framebuffer_addr == 0) return false;

    auto* frontbuffer_pixels = reinterpret_cast<Pixel*>(framebuffer_tag->framebuffer_addr);
    front_buffer = {
        .pixels         = Array_View<Pixel, GFX_PIXEL_COUNT>{frontbuffer_pixels},
        .pitch          = framebuffer_tag->framebuffer_pitch,
        .width          = framebuffer_tag->framebuffer_width,
        .height         = framebuffer_tag->framebuffer_height,
        .bits_per_pixel = framebuffer_tag->framebuffer_bpp,
        .type           = framebuffer_tag->framebuffer_type,
        .stride         = framebuffer_tag->framebuffer_pitch / sizeof(u32),
    };
    kstd_assert(front_buffer.bits_per_pixel == 32, "Only 32BPP supported.");
    kstd_assert(GFX_PIXEL_COUNT == front_buffer.width * front_buffer.height);

    framebuffer_fmt.init(*framebuffer_tag);
    kstd_assert(
        !(framebuffer_fmt.red_pos == 0 && framebuffer_fmt.green_pos == 0 && framebuffer_fmt.blue_pos == 0),
        "Framebuffer_Format was not initialized"
    );

    static_assert(sizeof(Pixel) == sizeof(u32));
    kstd_memset32(back_buffer.data, 0, back_buffer.size_in_bytes / sizeof(Pixel));
    swap_buffers();

    static_assert(sizeof(Depth) == sizeof(u32));
    kstd_memset32(depth_buffer.data, DEPTH_FAR, depth_buffer.size_in_bytes / sizeof(Depth));

    framebuffer_initialized = true;
    return true;
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

template <bool IMMEDIATE>
static force_inline auto set_pixel(u32 x, u32 y, Color color) -> void {
    kstd_assert(x < width());
    kstd_assert(y < height());

    using namespace hidden;

    auto index = y * front_buffer.stride + x;
    if constexpr(IMMEDIATE) {
        front_buffer.pixels[index].blend_with(color);
        back_buffer[index].blend_with(color);
    } else {
        back_buffer[index].blend_with(color);
    }
}

// depth == DEPTH_FAR draws unconditionally (used by anything that should never
// be occluded, e.g. UI). Otherwise the pixel is only drawn, and depth_buffer
// updated, if depth is closer than what's already there.
static force_inline auto set_pixel(u32 x, u32 y, Color color, Depth depth = DEPTH_FAR) -> void {
    kstd_assert(x < width());
    kstd_assert(y < height());

    using namespace hidden;

    auto index = y * front_buffer.stride + x;
    if (depth != DEPTH_FAR && depth >= depth_buffer[index]) return;

    back_buffer[index].blend_with(color);
    if (depth != DEPTH_FAR) depth_buffer[index] = depth;
}

// @TODO(blanktiger): Optimize.
auto clear(Color color) -> void {
    for (u32 y = 0; y < height(); ++y) {
        for (u32 x = 0; x < width(); ++x) {
            set_pixel(x, y, color);
        }
    }

    static_assert(sizeof(Depth) == sizeof(u32));
    kstd_memset32(hidden::depth_buffer.data, DEPTH_FAR, hidden::depth_buffer.size_in_bytes / sizeof(Depth));
}

enum struct Render_Pass : u8 {
    WORLD_2D,
    WORLD_3D,
    UI,
};

}  // namespace gfx
