#pragma once
#include "core/container/dynamic_array.h"
#include "core/foundation_types.h"
#include "core/math/aabb.h"
#include "core/math/vec.h"

namespace Entelechy
{

// ------------------------------------------------------------------
// MeshVertex — interleaved vertex layout
// ------------------------------------------------------------------
// Matches the glTF cook output: position / normal / uv /
// tangent. `tangentW` is the bitangent handedness (glTF TANGENT.w,
// +1 or -1). Core has no Vec4 type, so the tangent is split into
// Vec3 + f32; the GPU-side attribute layout is still 4 floats.
// ------------------------------------------------------------------
struct MeshVertex
{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec3 tangent;
    f32 tangentW = 1.0f;
};

// ------------------------------------------------------------------
// MeshAsset — CPU-side mesh data
// ------------------------------------------------------------------
// Owns the interleaved vertex stream, the index buffer and the local
// AABB (used for frustum culling). Populated either procedurally on
// the game side or by the offline glTF cook.
// Must remain default-constructible (HandleTable<T> requirement).
// ------------------------------------------------------------------
struct MeshAsset
{
    DynamicArray<MeshVertex> vertices;
    DynamicArray<u32> indices;
    AABB bounds;

    // Recomputes `bounds` from the vertex positions. An empty mesh
    // gets a zero-sized box at the origin.
    void computeBounds()
    {
        if (vertices.size() == 0)
        {
            bounds = AABB::fromMinMax(Vec3{}, Vec3{});
            return;
        }
        bounds = AABB::fromMinMax(vertices[0].position, vertices[0].position);
        for (usize i = 1; i < vertices.size(); ++i)
            bounds.expand(vertices[i].position);
    }

    [[nodiscard]] static constexpr u32 vertexStride()
    {
        return static_cast<u32>(sizeof(MeshVertex));
    }
};

} // namespace Entelechy
