#pragma once
#include "foundation_types.h"
#include "math/vec.h"
#include "math/mat4.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Minimal cube renderer for batch B visual validation.
// Hard-coded indexed cube mesh + simple MVP shader.
// No materials, no texture, no lighting — just flat shaded colored cubes.
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
    bool compileShader(const char* vertexSrc, const char* fragmentSrc);

    u32 m_shaderProgram = 0;
    u32 m_vao = 0;
    u32 m_vbo = 0;
    u32 m_ebo = 0;
    i32 m_mvpLoc = -1;
    i32 m_colorLoc = -1;
    bool m_initialized = false;
};

} // namespace Entelechy
