#pragma once
#include "foundation_types.h"
#include "math/vec.h"
#include "math/mat4.h"
#include "rhi_device.h"
#include "rhi_resources.h"
#include "material.h"
#include "shader_cache.h"
#include <memory>

namespace Entelechy {

// Forward declaration
class GLRHIDevice;

// ------------------------------------------------------------------
// Minimal cube renderer for batch B visual validation.
// Indexed cube mesh + MVP shader, now backed by Material + RHI.
//
// Phase 1: uses an internal GLRHIDevice to exercise the RHI layer.
// Future: cube mesh and material will be managed by ECS + render pipeline.
// ------------------------------------------------------------------
class SimpleCubeRenderer {
public:
    SimpleCubeRenderer();
    ~SimpleCubeRenderer();

    bool init();
    void shutdown();

    // Draw one cube. color is RGB, mvp is model-view-projection matrix.
    void drawCube(const Mat4& mvp, const Vec3& color);

    // Convenience: draw a cube at world transform with view + projection.
    void drawCube(const Mat4& world, const Mat4& view, const Mat4& proj, const Vec3& color);

private:
    bool createMesh();

    std::unique_ptr<GLRHIDevice> m_device;
    std::unique_ptr<ShaderCache> m_shaderCache;
    Material m_material;

    RHIBufferRef m_vertexBuffer;
    RHIBufferRef m_indexBuffer;

    bool m_initialized = false;
};

} // namespace Entelechy
