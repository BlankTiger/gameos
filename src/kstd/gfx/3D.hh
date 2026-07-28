#pragma once

#include "kstd/math.hh"
#include "kstd/array.hh"

#include "common.hh"

namespace gfx {

using namespace math;

struct Vertex {
    Vector4<f32> position;
    Color        color;
};

using Index = u32;

struct Mesh {
    Array_View<Vertex> vertices;
    Array_View<Index>  indices;
};

struct Draw_Command_3D { };

inline Array<Draw_Command_3D> draw_commands_world_3D(256);

force_inline auto draw_world_3D() -> void {
    draw_commands_world_3D.clear();
}

}  // namespace gfx
