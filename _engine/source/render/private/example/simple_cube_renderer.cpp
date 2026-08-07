#include "render/example/simple_cube_renderer.h"
#include "render/rhi/gl_rhi_device.h"
#include "log/core/log_macros.h"
#include <cstring>

namespace Entelechy
{

static const char *s_vertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char *s_fragmentShader = R"(#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// Cube vertices (8 corners, centered at origin, edge length 1)
static const f32 s_cubeVertices[8 * 3] = {
    -0.5f, -0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
    -0.5f, -0.5f, 0.5f,  0.5f, -0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  -0.5f, 0.5f, 0.5f,
};

// Indices for 12 triangles (6 faces * 2)
static const u32 s_cubeIndices[36] = {
    // front
    4,
    5,
    6,
    4,
    6,
    7,
    // back
    1,
    0,
    3,
    1,
    3,
    2,
    // left
    0,
    4,
    7,
    0,
    7,
    3,
    // right
    5,
    1,
    2,
    5,
    2,
    6,
    // top (CCW seen from +Y)
    3,
    7,
    6,
    3,
    6,
    2,
    // bottom
    0,
    1,
    5,
    0,
    5,
    4,
};

SimpleCubeRenderer::SimpleCubeRenderer() = default;

SimpleCubeRenderer::~SimpleCubeRenderer()
{
    if (m_initialized)
        shutdown();
}

bool SimpleCubeRenderer::createMesh()
{
    VertexAttributeDesc attr{};
    attr.location = 0;
    attr.components = 3;
    attr.normalized = false;
    attr.offset = 0;

    BufferDesc vbDesc{};
    vbDesc.size = sizeof(s_cubeVertices);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.vertexStride = 3 * sizeof(f32);
    vbDesc.vertexAttributes = &attr;
    vbDesc.vertexAttributeCount = 1;

    m_vertex_buffer = m_device->createBuffer(vbDesc, s_cubeVertices);
    if (!m_vertex_buffer)
    {
        LOG_ERROR(LogCategories::kEngine, "SimpleCubeRenderer: failed to create vertex buffer");
        return false;
    }

    BufferDesc ibDesc{};
    ibDesc.size = sizeof(s_cubeIndices);
    ibDesc.usage = BufferUsage::Index;

    m_index_buffer = m_device->createBuffer(ibDesc, s_cubeIndices);
    if (!m_index_buffer)
    {
        LOG_ERROR(LogCategories::kEngine, "SimpleCubeRenderer: failed to create index buffer");
        return false;
    }

    return true;
}

bool SimpleCubeRenderer::init(IRHIDevice *device)
{
    if (m_initialized)
        return true;

    if (!device)
    {
        LOG_ERROR(LogCategories::kEngine, "SimpleCubeRenderer: null device");
        return false;
    }
    m_device = device;

    m_shader_cache = std::make_unique<ShaderCache>();

    // Parameter layout: matches flattened HLSL cbuffer output
    // Vertex: type_PerDraw[0..3] = uMVP rows; Pixel: type_PerMaterial[0] = {uColor.xyz, pad}
    MaterialParamDesc params[] = {
        {"type_PerDraw[0]", MaterialParamType::Vec4},
        {"type_PerDraw[1]", MaterialParamType::Vec4},
        {"type_PerDraw[2]", MaterialParamType::Vec4},
        {"type_PerDraw[3]", MaterialParamType::Vec4},
        {"type_PerMaterial[0]", MaterialParamType::Vec4},
    };

    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    pipelineDesc.rasterizerState.cullMode = CullMode::Back;
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = true;

    if (!m_material.initFromBytecode(m_device, "shaders/simple_cube_vertex.glsl", "shaders/simple_cube_pixel.glsl",
                                     ShaderBytecodeFormat::GLSL, params, 5, pipelineDesc))
    {
        LOG_ERROR(LogCategories::kEngine, "SimpleCubeRenderer: failed to init material");
        return false;
    }

    if (!createMesh())
    {
        m_material.shutdown();
        return false;
    }

    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "SimpleCubeRenderer initialized (RHI + Material)");
    return true;
}

void SimpleCubeRenderer::shutdown()
{
    if (!m_initialized)
        return;

    m_material.shutdown();
    m_index_buffer.reset();
    m_vertex_buffer.reset();
    m_shader_cache.reset();
    // Device is borrowed — do not shut it down or delete it here.
    m_device = nullptr;
    m_initialized = false;
}

void SimpleCubeRenderer::drawCube(const Mat4 &mvp, const Vec3 &color)
{
    if (!m_initialized)
        return;

    auto *cmdList = m_device->createCommandList();

    m_material.setVec4("type_PerDraw[0]"_sid, Vec4{mvp.m[0], mvp.m[1], mvp.m[2], mvp.m[3]});
    m_material.setVec4("type_PerDraw[1]"_sid, Vec4{mvp.m[4], mvp.m[5], mvp.m[6], mvp.m[7]});
    m_material.setVec4("type_PerDraw[2]"_sid, Vec4{mvp.m[8], mvp.m[9], mvp.m[10], mvp.m[11]});
    m_material.setVec4("type_PerDraw[3]"_sid, Vec4{mvp.m[12], mvp.m[13], mvp.m[14], mvp.m[15]});
    m_material.setVec4("type_PerMaterial[0]"_sid, Vec4{color.x, color.y, color.z, 1.0f});
    m_material.bind(cmdList);

    cmdList->bindVertexBuffer(m_vertex_buffer.get(), 0, 0);
    cmdList->bindIndexBuffer(m_index_buffer.get(), 0);
    cmdList->drawIndexed(36, 0, 0);

    m_device->submit(cmdList);
}

void SimpleCubeRenderer::drawCube(const Mat4 &world, const Mat4 &view, const Mat4 &proj, const Vec3 &color)
{
    drawCube(proj * view * world, color);
}

void SimpleCubeRenderer::endFrame()
{
    if (!m_initialized || !m_device)
        return;
    m_device->signalFrame();
    m_device->flushPendingDeletes();
}

} // namespace Entelechy
