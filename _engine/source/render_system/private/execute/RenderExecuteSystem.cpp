#include "render_system/execute/RenderExecuteSystem.h"
#include "render_system/components/RenderCamera.h"
#include "render_system/components/RenderComponents.h"
#include "render_system/phase/RenderResources.h"
#include "render/rhi/gl_rhi_device.h"
#include "render/material/shader_cache.h"
#include "ecs/world/world.h"
#include "ecs/query/query.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

namespace
{

// Unlit solid-color shader pair (same GLSL as SimpleCubeRenderer).
const char *s_vertexShader = R"(#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

const char *s_fragmentShader = R"(#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

} // namespace

RenderExecuteSystem::RenderExecuteSystem() = default;

RenderExecuteSystem::~RenderExecuteSystem()
{
    if (m_initialized)
        shutdown();
}

bool RenderExecuteSystem::init()
{
    if (m_initialized)
        return true;

    m_device = std::make_unique<GLRHIDevice>();
    if (!m_device->initialize())
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: failed to initialize GLRHIDevice");
        return false;
    }

    m_shader_cache = std::make_unique<ShaderCache>();

    m_initialized = true;
    LOG_INFO(LogCategories::kEngine, "RenderExecuteSystem initialized (RHI + ShaderCache)");
    return true;
}

void RenderExecuteSystem::shutdown()
{
    if (!m_initialized)
        return;

    for (auto [id, material] : m_materials)
    {
        (void)id;
        material.shutdown();
    }
    m_materials.clear();
    m_meshes.clear();
    m_shader_cache.reset();
    if (m_device)
    {
        m_device->shutdown();
        m_device.reset();
    }
    m_initialized = false;
}

bool RenderExecuteSystem::registerMesh(u32 assetId, const void *vertexData, usize vertexBytes, u32 vertexStride,
                                       const VertexAttributeDesc *attrs, u32 attrCount, const u32 *indexData,
                                       u32 indexCount)
{
    if (!m_initialized)
        return false;

    BufferDesc vbDesc{};
    vbDesc.size = static_cast<u32>(vertexBytes);
    vbDesc.usage = BufferUsage::Vertex;
    vbDesc.vertexStride = vertexStride;
    vbDesc.vertexAttributes = attrs;
    vbDesc.vertexAttributeCount = attrCount;

    GpuMesh mesh{};
    mesh.vbo = m_device->createBuffer(vbDesc, vertexData);
    if (!mesh.vbo)
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: failed to create vertex buffer (mesh %u)", assetId);
        return false;
    }

    BufferDesc ibDesc{};
    ibDesc.size = indexCount * sizeof(u32);
    ibDesc.usage = BufferUsage::Index;

    mesh.ibo = m_device->createBuffer(ibDesc, indexData);
    if (!mesh.ibo)
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: failed to create index buffer (mesh %u)", assetId);
        return false;
    }
    mesh.index_count = indexCount;

    m_meshes.insert(assetId, std::move(mesh));
    return true;
}

bool RenderExecuteSystem::registerColorMaterial(u32 assetId, const Vec3 &color)
{
    if (!m_initialized)
        return false;

    // Parameter layout matches the shader uniforms.
    MaterialParamDesc params[] = {
        {"uMVP", MaterialParamType::Mat4},
        {"uColor", MaterialParamType::Vec3},
    };

    PipelineStateDesc pipelineDesc{};
    pipelineDesc.topology = PrimitiveTopology::Triangles;
    pipelineDesc.rasterizerState.cullMode = CullMode::Back;
    pipelineDesc.depthStencilState.depthTest = true;
    pipelineDesc.depthStencilState.depthWrite = true;

    Material material;
    if (!material.init(m_device.get(), m_shader_cache.get(), s_vertexShader, s_fragmentShader, params, 2, pipelineDesc))
    {
        LOG_ERROR(LogCategories::kEngine, "RenderExecuteSystem: failed to init material (material %u)", assetId);
        return false;
    }
    material.setVec3("uColor"_sid, color);

    m_materials.insert(assetId, std::move(material));
    return true;
}

void RenderExecuteSystem::drawItem(World &renderWorld, const ExtractedView &view, Entity renderEntity,
                                   IRHICommandList *cmdList)
{
    const RenderTransform *transform = renderWorld.getComponent<RenderTransform>(renderEntity);
    const RenderMesh *mesh = renderWorld.getComponent<RenderMesh>(renderEntity);
    if (!transform || !mesh)
    {
        ++m_stats.skipped_missing_mesh;
        return;
    }

    const GpuMesh *gpuMesh = m_meshes.find(mesh->mesh_asset_id);
    if (!gpuMesh)
    {
        ++m_stats.skipped_missing_mesh;
        return;
    }

    const RenderMaterial *materialRef = renderWorld.getComponent<RenderMaterial>(renderEntity);
    Material *material = materialRef ? m_materials.find(materialRef->material_asset_id) : nullptr;
    if (!material)
    {
        ++m_stats.skipped_missing_material;
        return;
    }

    material->setMat4("uMVP"_sid, view.proj_matrix * view.view_matrix * transform->world_matrix);
    material->bind(cmdList);

    cmdList->bindVertexBuffer(gpuMesh->vbo.get(), 0, 0);
    cmdList->bindIndexBuffer(gpuMesh->ibo.get(), 0);
    cmdList->drawIndexed(gpuMesh->index_count, 0, 0);
    ++m_stats.draw_calls;
}

void RenderExecuteSystem::run(World &renderWorld)
{
    m_stats = ExecuteStats{};
    if (!m_initialized)
        return;

    // Locate the single view entity.
    Entity viewEntity{0, 0};
    const ExtractedView *view = nullptr;
    ConstQuery<ExtractedView> viewQuery(renderWorld);
    for (auto [ve, ev] : viewQuery)
    {
        viewEntity = ve;
        view = ev;
        break;
    }
    if (!view)
        return;

    const ViewBinnedPhases *binned = renderWorld.getComponent<ViewBinnedPhases>(viewEntity);
    const ViewSortedPhases *sorted = renderWorld.getComponent<ViewSortedPhases>(viewEntity);
    if (!binned && !sorted)
        return;

    IRHICommandList *cmdList = m_device->createCommandList();

    if (binned)
    {
        for (const PhaseBin &bin : binned->opaque.getBins())
        {
            for (const PhaseItem &item : bin.items)
            {
                drawItem(renderWorld, *view, item.render_entity, cmdList);
            }
        }
        for (const PhaseBin &bin : binned->alpha_mask.getBins())
        {
            for (const PhaseItem &item : bin.items)
            {
                drawItem(renderWorld, *view, item.render_entity, cmdList);
            }
        }
    }
    if (sorted)
    {
        for (const PhaseItem &item : sorted->transparent.getItems())
        {
            drawItem(renderWorld, *view, item.render_entity, cmdList);
        }
        for (const PhaseItem &item : sorted->ui.getItems())
        {
            drawItem(renderWorld, *view, item.render_entity, cmdList);
        }
    }

    m_device->submit(cmdList);
}

void RenderExecuteSystem::endFrame()
{
    if (!m_initialized || !m_device)
        return;
    m_device->signalFrame();
    m_device->flushPendingDeletes();
}

} // namespace Entelechy
