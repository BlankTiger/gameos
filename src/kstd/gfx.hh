#pragma once

// Graphics types and constants.
// Backend-specific functions are provided in kstd.hh after the backend is selected.

namespace gfx {

struct Color {
    u8 b, g, r, w;
};

constexpr Color BLACK{0, 0, 0, 255};
constexpr Color WHITE{255, 255, 255, 255};
constexpr Color RED{0, 0, 255, 255};
constexpr Color GREEN{0, 255, 0, 255};
constexpr Color BLUE{255, 0, 0, 255};
constexpr Color TRANSPARENT{0, 0, 0, 0};

}  // namespace gfx

#if defined(USE_VGA_BACKEND)

#include "vga.hh"
using Gfx_Backend = vga::Gfx_Backend;

#elif defined(USE_FB_BACKEND)

#include "framebuffer.hh"
using Gfx_Backend = fb::Gfx_Backend;

// @TODO: #else #error (in fmt namespace too when switching on the backend)
#endif

namespace gfx {

[[nodiscard]] static auto initialize(const mem::Multiboot2_Info* mbi) -> bool {
    return Gfx_Backend::initialize(mbi);
}

static auto clear(Color color) -> void {
    for (u32 y = 0; y < Gfx_Backend::height(); ++y) {
        for (u32 x = 0; x < Gfx_Backend::width(); ++x) {
            Gfx_Backend::set_pixel(x, y, color);
        }
    }
}

static auto draw_rect(u32 x, u32 y, u32 w, u32 h, Color color) -> void {
    for (u32 _y = y; _y < y + h && _y < Gfx_Backend::height(); ++_y) {
        for (u32 _x = x; _x < x + w && _x < Gfx_Backend::width(); ++_x) {
            Gfx_Backend::set_pixel(_x, _y, color);
        }
    }
}

}  // namespace gfx
