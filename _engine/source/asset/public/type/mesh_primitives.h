#pragma once
#include "asset/type/mesh_asset.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// Mesh primitives — procedural MeshAsset builders
// ------------------------------------------------------------------
// Shared by the Prepare stage (fallback cube) and the game side (demo
// scene). All builders emit the full interleaved MeshVertex layout
// (position/normal/uv/tangent) with CCW winding seen from outside
// (matches the default rasterizer state: front = CCW, cull = back)
// and a computed local AABB.
// ------------------------------------------------------------------

// Unit cube centered on the origin (edge length 2 * halfExtent),
// 24 vertices (4 per face, flat normals), 36 indices, per-face [0,1] UVs.
inline MeshAsset buildCubeMesh(f32 halfExtent = 0.5f)
{
    struct FaceDesc
    {
        Vec3 normal;
        Vec3 origin; // corner at uv (0,0)
        Vec3 uAxis;  // unit axis along increasing u (== tangent)
        Vec3 vAxis;  // unit axis along increasing v
    };

    const f32 h = halfExtent;
    const FaceDesc faces[6] = {
        // cross(uAxis, vAxis) == normal guarantees CCW winding from outside
        {{0.0f, 1.0f, 0.0f}, {-h, h, h}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},  // +Y
        {{0.0f, -1.0f, 0.0f}, {-h, -h, -h}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, // -Y
        {{0.0f, 0.0f, 1.0f}, {-h, -h, h}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},   // +Z
        {{0.0f, 0.0f, -1.0f}, {h, -h, -h}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, // -Z
        {{1.0f, 0.0f, 0.0f}, {h, -h, h}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},   // +X
        {{-1.0f, 0.0f, 0.0f}, {-h, -h, -h}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, // -X
    };

    MeshAsset mesh;
    mesh.vertices.reserve(24);
    mesh.indices.reserve(36);

    for (const FaceDesc &face : faces)
    {
        const u32 base = static_cast<u32>(mesh.vertices.size());
        const f32 edge = 2.0f * h;
        for (u32 v = 0; v < 4; ++v)
        {
            const f32 s = static_cast<f32>(v & 1u);         // u: 0,1,0,1
            const f32 t = static_cast<f32>((v >> 1u) & 1u); // v: 0,0,1,1
            MeshVertex vertex{};
            vertex.position = face.origin + face.uAxis * (s * edge) + face.vAxis * (t * edge);
            vertex.normal = face.normal;
            vertex.uv = {s, t};
            vertex.tangent = face.uAxis;
            vertex.tangentW = 1.0f;
            mesh.vertices.pushBack(vertex);
        }
        // Vertex order is (0,0) (1,0) (0,1) (1,1); both triangles keep
        // cross(e0, e1) == normal (CCW seen from outside).
        mesh.indices.pushBack(base + 0);
        mesh.indices.pushBack(base + 1);
        mesh.indices.pushBack(base + 3);
        mesh.indices.pushBack(base + 0);
        mesh.indices.pushBack(base + 3);
        mesh.indices.pushBack(base + 2);
    }

    mesh.computeBounds();
    return mesh;
}

// Horizontal quad on the XZ plane (normal +Y), centered on the origin,
// UVs tiled [0, uvTiling] so checker textures repeat. 4 vertices, 6 indices.
inline MeshAsset buildGroundMesh(f32 halfSize, f32 uvTiling)
{
    MeshAsset mesh;
    mesh.vertices.reserve(4);
    mesh.indices.reserve(6);

    const Vec3 normal{0.0f, 1.0f, 0.0f};
    const Vec3 tangent{1.0f, 0.0f, 0.0f};
    // Same winding rule as the cube top face: uAxis=+X, vAxis=-Z.
    const Vec3 origin{-halfSize, 0.0f, halfSize};
    const f32 edge = 2.0f * halfSize;

    for (u32 v = 0; v < 4; ++v)
    {
        const f32 s = static_cast<f32>(v & 1u);
        const f32 t = static_cast<f32>((v >> 1u) & 1u);
        MeshVertex vertex{};
        vertex.position = origin + Vec3{edge * s, 0.0f, -edge * t};
        vertex.normal = normal;
        vertex.uv = {s * uvTiling, t * uvTiling};
        vertex.tangent = tangent;
        vertex.tangentW = 1.0f;
        mesh.vertices.pushBack(vertex);
    }
    mesh.indices = {0, 1, 3, 0, 3, 2};

    mesh.computeBounds();
    return mesh;
}

} // namespace Entelechy
