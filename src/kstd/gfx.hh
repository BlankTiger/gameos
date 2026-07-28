#pragma once

#include "gfx/2D.hh"
#include "gfx/3D.hh"
#include "gfx/common.hh"

namespace gfx {

auto draw_frame() -> void {
    draw_world_3D();
    draw_world_2D();
    draw_ui();
    swap_buffers();
}

}  // namespace gfx
