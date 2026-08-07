#pragma once
// D3D12CommandTranslator — replays a RenderCommandBuffer by issuing
// equivalent D3D12 calls onto the device's per-frame graphics command
// list. Counterpart to GLCommandTranslator.
//
// Uniform model: the upper layer still speaks the legacy per-draw
// setUniform* API with SPIRV-Cross-flattened names ("type_PerDraw[3]").
// The translator parses those names back into (cbuffer, vec4 index),
// accumulates writes in CPU staging buffers laid out exactly like the
// HLSL cbuffers (recovered via D3DReflect at PSO creation), and at draw
// time uploads each dirty staging block into the device's per-frame
// upload ring and binds it as a root CBV. This keeps RenderExecuteSystem
// and Material untouched until 6e replaces the whole path with
// bindConstantBuffer/BindGroup.
//
// Global root signature (shared by all PSOs):
//   slot 0: CBV b0, VERTEX      slot 1: CBV b0, PIXEL
//   slot 2: CBV b1, VERTEX      slot 3: CBV b1, PIXEL
//   slot 4: CBV b2, VERTEX      slot 5: CBV b2, PIXEL
//   slot 6: descriptor table SRV t0..t2, PIXEL
//   static samplers s0..s2 (linear wrap, LOD clamped to mip 0), PIXEL
#include "render/rhi/render_command_buffer.h"
#include "render/rhi/d3d12_rhi_device.h"

namespace Entelechy
{

class D3D12CommandTranslator
{
public:
    explicit D3D12CommandTranslator(D3D12RHIDevice &device);

    // Replay all commands in the buffer onto the given command list.
    void execute(ID3D12GraphicsCommandList *list, const RenderCommandBuffer &buffer);

    // Reset cached state between frames (called before execute).
    void resetState();

private:
    // CPU staging mirror of one reflected cbuffer.
    struct StagedCBuffer
    {
        u32 rootSlot = 0; // 0xFFFFFFFF = unmapped, skip at draw
        std::vector<u8> data;
        bool dirty = false;
    };

    void bindPipeline(D3D12PipelineState *pso);
    void stageUniform(const StringId name, const void *payload, u32 payloadSize);
    void flushStateForDraw(ID3D12GraphicsCommandList *list);

    D3D12RHIDevice &m_device;

    D3D12PipelineState *m_current_pso = nullptr;
    // One staging entry per reflected cbuffer; parallel to the PSO's
    // vsCBuffers()/psCBuffers() arrays.
    std::vector<StagedCBuffer> m_vs_staging;
    std::vector<StagedCBuffer> m_ps_staging;

    RHITexture *m_bound_textures[8] = {};
    bool m_has_bound_textures = false;

    D3D12_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    bool m_pipeline_dirty = false;
};

} // namespace Entelechy
