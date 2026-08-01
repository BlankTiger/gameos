#pragma once

#include "kstd/math.hh"
#include "kstd/matrix.hh"
#include "kstd/array.hh"
#include "kstd/resource.hh"

#include "common.hh"

namespace gfx {

using namespace math;
namespace hidden = gfx::hidden;

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

// 4 verts per face so each face keeps solid color / own UVs (shared verts would blend).
// Winding matches old shared-vertex cube (outward, CW in RH when viewed from outside).
// UV: full texture per face, v down (image space).
static Static_Array<Vertex, 24> unit_cube_vertices{{
    // +Z
    {{ 1,  1,  1}, BLUE,    {1.f, 0.f}}, {{-1,  1,  1}, BLUE,    {0.f, 0.f}}, {{-1, -1,  1}, BLUE,    {0.f, 1.f}}, {{ 1, -1,  1}, BLUE,    {1.f, 1.f}},
    // +X
    {{ 1,  1, -1}, GREEN,   {1.f, 0.f}}, {{ 1,  1,  1}, GREEN,   {0.f, 0.f}}, {{ 1, -1,  1}, GREEN,   {0.f, 1.f}}, {{ 1, -1, -1}, GREEN,   {1.f, 1.f}},
    // -Z
    {{-1,  1, -1}, RED,     {0.f, 0.f}}, {{ 1,  1, -1}, RED,     {1.f, 0.f}}, {{ 1, -1, -1}, RED,     {1.f, 1.f}}, {{-1, -1, -1}, RED,     {0.f, 1.f}},
    // -X
    {{-1,  1,  1}, YELLOW,  {1.f, 0.f}}, {{-1,  1, -1}, YELLOW,  {0.f, 0.f}}, {{-1, -1, -1}, YELLOW,  {0.f, 1.f}}, {{-1, -1,  1}, YELLOW,  {1.f, 1.f}},
    // +Y
    {{ 1,  1, -1}, CYAN,    {1.f, 0.f}}, {{-1,  1, -1}, CYAN,    {0.f, 0.f}}, {{-1,  1,  1}, CYAN,    {0.f, 1.f}}, {{ 1,  1,  1}, CYAN,    {1.f, 1.f}},
    // -Y
    {{-1, -1,  1}, MAGENTA, {0.f, 0.f}}, {{-1, -1, -1}, MAGENTA, {0.f, 1.f}}, {{ 1, -1, -1}, MAGENTA, {1.f, 1.f}}, {{ 1, -1,  1}, MAGENTA, {1.f, 0.f}},
}};
static Static_Array<Index, 12> unit_cube_indices{{
    { 0,  1,  2}, { 0,  2,  3}, // +Z
    { 4,  5,  6}, { 4,  6,  7}, // +X
    { 8,  9, 10}, { 8, 10, 11}, // -Z
    {12, 13, 14}, {12, 14, 15}, // -X
    {16, 17, 18}, {16, 18, 19}, // +Y
    {20, 21, 22}, {20, 22, 23}, // -Y
}};
const inline Mesh UNIT_CUBE{unit_cube_vertices, unit_cube_indices};

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
    f32 vertical_fov_degrees = 90.f;
    f32 aspect_ratio = GFX_ASPECT_RATIO;
    f32 z_near = 1.f, z_far = 10.f;
    f32 x_offset = 0.f, y_offset = 0.f;
    bool depth_range_01 = false;
    Matrix4<f32> M_projection;

    // Realistically, this should be computed only once since we cannot change
    // aspect ratio and such, but in case someone wants to change projection on the fly
    // this function does that
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

// Defined in 2D.hh; 3D queues screen-space tris through it for now.
auto draw_triangle(
    Vector4<f32> v1, Vector4<f32> v2, Vector4<f32> v3,
    Color color, u8 z
) -> void;
auto draw_triangle(
    Vector4<f32> v1, Vector4<f32> v2, Vector4<f32> v3,
    Vector2<f32> uv1, Vector2<f32> uv2, Vector2<f32> uv3,
    Resource_View texture, Color color, u8 z
) -> void;

auto clip_to_screen(Vector4<f32> clip) -> Vector4<f32> {
    f32 inv_w = 1.f / clip.w;
    f32 ndc_x = clip.x * inv_w;
    f32 ndc_y = clip.y * inv_w;
    f32 ndc_z = clip.z * inv_w;
    f32 sx = (ndc_x * 0.5f + 0.5f) * static_cast<f32>(width());
    f32 sy = (1.f - (ndc_y * 0.5f + 0.5f)) * static_cast<f32>(height());
    return Vector4<f32>{sx, sy, ndc_z, clip.w};
}

auto draw_mesh(Mesh_Instance instance) -> void {
    // TODO: implement draw command
    Matrix4<f32> M = projection.M_projection * camera.M_camera * instance.transform;
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
            draw_triangle(p1, p2, p3, a.uv, b.uv, c.uv, instance.texture, a.color, 1);
        } else {
            draw_triangle(p1, p2, p3, a.color, 1); // TODO: shading using 3 vertices colors + interpolation
        }
    }
}

force_inline auto draw_world_3D() -> void {
    // TODO: implement recomputing camera if it changed
    // TODO: implement draw commands
    draw_commands_world_3D.clear();
}

}  // namespace gfx
