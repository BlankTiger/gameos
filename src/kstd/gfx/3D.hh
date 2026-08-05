#pragma once

#include "kstd/math.hh"
#include "kstd/matrix.hh"
#include "kstd/array.hh"
#include "kstd/resource.hh"

#include "common.hh"
#include "2D.hh"

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
    {{ 1,  1,  1}, BLUE,    {1.f, 0.f}}, {{-1,  1,  1}, RED,     {0.f, 0.f}}, {{-1, -1,  1}, BLUE,    {0.f, 1.f}}, {{ 1, -1,  1}, RED,     {1.f, 1.f}},
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

    union {
        Mesh_Command mesh;
        Wireframe_Command wireframe;
    };
};

inline Array<Draw_Command_3D> draw_commands_world_3D(256);

force_inline auto sample_texture(const Resource_View& res, f32 u, f32 v) -> Color {
    if (u < 0.f) u = 0.f;
    else if (u > 1.f) u = 1.f;
    if (v < 0.f) v = 0.f;
    else if (v > 1.f) v = 1.f;
    const u32 tx = static_cast<u32>(u * static_cast<f32>(res.width  - 1));
    const u32 ty = static_cast<u32>(v * static_cast<f32>(res.height - 1));
    const Color* colors = reinterpret_cast<const Color*>(res.data.data);
    return colors[ty * res.width + tx];
}

// Possible improvements:
// - Iterate over chunks of pixels and check if bounding vertices are out of the triangle
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

    switch (cull_mode) {
        case Cull_Mode::NONE: break;
        case Cull_Mode::BACK_FACE: {
            if (!is_counter_clockwise) return;
        } break;
    }

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
                    static_cast<u8>(std::clamp(r, (f32) 0.f, (f32) 255.f)),
                    static_cast<u8>(std::clamp(g, (f32) 0.f, (f32) 255.f)),
                    static_cast<u8>(std::clamp(b, (f32) 0.f, (f32) 255.f)),
                    static_cast<u8>(std::clamp(a, (f32) 0.f, (f32) 255.f))
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
            inner_draw_triangle_3d(p1, p2, p3, {}, {}, {}, a.color, b.color, c.color); // TODO: shading using 3 vertices colors + interpolation
        }
    }
}

auto draw_mesh(Mesh_Instance instance, Camera3D camera) -> void {
    draw_commands_world_3D.push_back(
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
        // const Vertex& c = instance.model.vertices[i3];
        inner_draw_raw_line_3d(p1, p2, a.color);
        inner_draw_raw_line_3d(p1, p3, a.color);
        inner_draw_raw_line_3d(p2, p3, b.color);
    }
}

auto draw_wireframe(Mesh_Instance instance, Camera3D camera) -> void {
    draw_commands_world_3D.push_back(
        Draw_Command_3D{
            .type = Draw_Command_3D_Type::DRAW_WIREFRAME,
            .camera = camera,
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
                inner_draw_mesh(cmd.instance, command.camera);
            } break;
            case DRAW_WIREFRAME: {
                const Mesh_Command& cmd = command.mesh;
                inner_draw_wireframe(cmd.instance, command.camera);
            } break;
        }
    }
    draw_commands_world_3D.clear();
}

}  // namespace gfx
