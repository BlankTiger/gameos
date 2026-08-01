#pragma once

#include "kstd/math.hh"
#include "kstd/matrix.hh"
#include "kstd/array.hh"

#include "common.hh"

namespace gfx {

using namespace math;
namespace hidden = gfx::hidden;

struct Vertex {
    Vector3<f32> position;
    Color        color;
};

using Index = Vector3<u32>;

struct Mesh {
    Array_View<Vertex> vertices;
    Array_View<Index>  indices;
};

static Static_Array<Vertex, 8> unit_cube_vertices{{
    {{ 1,  1,  1}, BLUE},
    {{-1,  1,  1}, BLUE},
    {{-1, -1,  1}, BLUE},
    {{ 1, -1,  1}, BLUE},
    {{ 1,  1, -1}, RED},
    {{-1,  1, -1}, RED},
    {{-1, -1, -1}, RED},
    {{ 1, -1, -1}, RED},
}};
static Static_Array<Index, 12> unit_cube_indices{{
    {0, 1, 2},
    {0, 2, 3},
    {4, 0, 3},
    {4, 3, 7},
    {5, 3, 7},
    {5, 7, 6},
    {1, 5, 6},
    {1, 6, 2},
    {4, 5, 1},
    {4, 1, 0},
    {2, 6, 7},
    {2, 7, 3}
}};
const inline Mesh UNIT_CUBE{unit_cube_vertices, unit_cube_indices};

struct Mesh_Instance {
    Mesh model;
    Vector3<f32> translation;
    Quaternion<f32> rotation;
    Vector3<f32> scale;
    Matrix4<f32> transform;

    auto recompute_matrix() -> void {
        transform = make_translation_matrix4(translation) * make_rotation_matrix<Matrix4<f32>>(rotation) * make_scale_matrix4(scale);
    }

    Mesh_Instance(Mesh model) : model(model), transform(Matrix4<f32>::identity()) {}
    Mesh_Instance(Mesh model, Vector3<f32> translation = {}, Quaternion<f32> rotation = {}, Vector3<f32> scale = {})
        : model(model), translation(translation), rotation(rotation), scale(scale)
        { recompute_matrix(); }
};

struct Camera3D {
    Vector3<f32> position;
    Quaternion<f32> rotation;
    Matrix4<f32> M_camera;

    auto recompute_matrix() -> void {
        InverseResult M_camera_rotation_inv = inverse(make_rotation_matrix<Matrix4<f32>>(rotation));
        InverseResult M_camera_translation_inv = inverse(make_translation_matrix4(position));
        // TODO: they should ALWAYS be invertible but maybe check here
        M_camera = M_camera_rotation_inv.result * M_camera_translation_inv.result;
    }

    constexpr Camera3D() : position(), rotation() { recompute_matrix(); }
};

inline Camera3D camera;

struct ProjectionSettings {
    f32 vertical_fov = 90.f;
    f32 aspect_ratio = GFX_ASPECT_RATIO;
    f32 z_near = 1.f, z_far = 10.f;
    f32 x_offset = 0.f, y_offset = 0.f;
    bool depth_range_01 = false;
    Matrix4<f32> M_projection;

    // Realistically, this should be computed only once since we cannot change
    // aspect ratio and such, but in case someone wants to change projection on the fly
    // this function does that
    auto recompute_matrix() -> void {
        M_projection = make_projection_matrix(
            vertical_fov,
            aspect_ratio,
            z_near, z_far,
            x_offset, y_offset,
            depth_range_01
        );
    }

    constexpr ProjectionSettings() { recompute_matrix(); }
};

inline ProjectionSettings projection;

enum struct Draw_Command_3D_Type: u8 {
    DRAW_WIREFRAME,
    DRAW_MESH,
};

struct Mesh_Command {
    // TODO: implement
};

struct Wireframe_Command {
    // TODO: implement
};

struct Draw_Command_3D {
    // TODO: Implement
};

inline Array<Draw_Command_3D> draw_commands_world_3D(256);

auto draw_mesh(Mesh_Instance instance) -> void {
    // TODO: implement draw command
    Matrix4<f32> M = projection.M_projection * camera.M_camera * instance.transform;
    Array_View<Vector4<f32>> projected_positions;
    for (usize i = 0; i < projected_positions.size; ++i) {
        projected_positions[i] = multiply(M, Vector4<f32>::as_point(instance.model.vertices[i].position));
    }
    for (const auto& [v1, v2, v3]: instance.model.indices) {
        draw_triangle(
            projected_positions[v1],
            projected_positions[v2],
            projected_positions[v3],
            instance.model.vertices[v1].color // TODO: shading using 3 vertices colors + interpolation
        );
    }
}

force_inline auto draw_world_3D() -> void {
    // TODO: implement recomputing camera if it changed
    // TODO: implement draw commands
    draw_commands_world_3D.clear();
}

}  // namespace gfx
