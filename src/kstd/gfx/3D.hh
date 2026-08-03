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
    {{ 1,  1,  1}, BLUE},    {{-1,  1,  1}, BLUE},    {{-1, -1,  1}, BLUE},    {{ 1, -1,  1}, BLUE},
    // +X
    {{ 1,  1, -1}, GREEN},   {{ 1,  1,  1}, GREEN},   {{ 1, -1,  1}, GREEN},   {{ 1, -1, -1}, GREEN},
    // -Z
    {{-1,  1, -1}, RED},     {{ 1,  1, -1}, RED},     {{ 1, -1, -1}, RED},     {{-1, -1, -1}, RED},
    // -X
    {{-1,  1,  1}, YELLOW},  {{-1,  1, -1}, YELLOW},  {{-1, -1, -1}, YELLOW},  {{-1, -1,  1}, YELLOW},
    // +Y
    {{ 1,  1, -1}, CYAN},    {{-1,  1, -1}, CYAN},    {{-1,  1,  1}, CYAN},    {{ 1,  1,  1}, CYAN},
    // -Y
    {{-1, -1,  1}, MAGENTA}, {{-1, -1, -1}, MAGENTA}, {{ 1, -1, -1}, MAGENTA}, {{ 1, -1,  1}, MAGENTA},
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

    constexpr Projection_Settings() { recompute_matrix(); }
};

namespace hidden {
    inline Projection_Settings projection;
}

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
    Depth depth = DEPTH_FAR; // only used when queued into draw_commands_world_2D

    union {
        Mesh_Command mesh;
        Wireframe_Command wireframe;
    };
};

inline Array<Draw_Command_3D> draw_commands_world_3D(256);

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

auto inner_draw_mesh(Mesh_Instance instance, Camera3D camera, Depth depth = DEPTH_FAR) -> void {
    auto projected_positions = project(instance, camera);
    for (const auto& [i1, i2, i3]: instance.model.indices) {
        const Vector4<f32>& p1 = projected_positions[i1];
        const Vector4<f32>& p2 = projected_positions[i2];
        const Vector4<f32>& p3 = projected_positions[i3];
        if (p1.w <= 0.f || p2.w <= 0.f || p3.w <= 0.f) continue;
        const Vertex& a = instance.model.vertices[i1];
        // const Vertex& b = instance.model.vertices[i2];
        // const Vertex& c = instance.model.vertices[i3];
        inner_draw_triangle(p1, p2, p3, a.color, depth); // TODO: shading using 3 vertices colors + interpolation
    }
}

auto draw_mesh(Mesh_Instance instance, Camera3D camera, Depth depth = DEPTH_FAR) -> void {
    draw_commands_world_3D.push_back(
        Draw_Command_3D{
            .type = Draw_Command_3D_Type::DRAW_MESH,
            .camera = camera,
            .depth = depth,
            .mesh = Mesh_Command{.instance = instance}
        }
    );
}

auto inner_draw_wireframe(Mesh_Instance instance, Camera3D camera, Depth depth = DEPTH_FAR) -> void {
    auto projected_positions = project(instance, camera);
    for (const auto& [i1, i2, i3]: instance.model.indices) {
        const Vector4<f32>& p1 = projected_positions[i1];
        const Vector4<f32>& p2 = projected_positions[i2];
        const Vector4<f32>& p3 = projected_positions[i3];
        if (p1.w <= 0.f || p2.w <= 0.f || p3.w <= 0.f) continue;
        const Vertex& a = instance.model.vertices[i1];
        const Vertex& b = instance.model.vertices[i2];
        // const Vertex& c = instance.model.vertices[i3];
        inner_draw_raw_line(p1.x, p1.y, p2.x, p2.y, a.color, depth);
        inner_draw_raw_line(p1.x, p1.y, p3.x, p3.y, a.color, depth);
        inner_draw_raw_line(p2.x, p2.y, p3.x, p3.y, b.color, depth);
    }
}

auto draw_wireframe(Mesh_Instance instance, Camera3D camera, Depth depth = DEPTH_FAR) -> void {
    draw_commands_world_3D.push_back(
        Draw_Command_3D{
            .type = Draw_Command_3D_Type::DRAW_WIREFRAME,
            .camera = camera,
            .depth = depth,
            .wireframe = Wireframe_Command{.instance = instance}
        }
    );
}


force_inline auto draw_world_3D() -> void {
    for (const auto& command : draw_commands_world_3D) {
        using enum Draw_Command_3D_Type;
        switch (command.type) {
            case DRAW_MESH: {
                const Mesh_Command& cmd = command.mesh;
                inner_draw_mesh(cmd.instance, command.camera, command.depth);
            } break;
            case DRAW_WIREFRAME: {
                const Mesh_Command& cmd = command.mesh;
                inner_draw_wireframe(cmd.instance, command.camera, command.depth);
            } break;
        }
    }
    draw_commands_world_3D.clear();
}

}  // namespace gfx
