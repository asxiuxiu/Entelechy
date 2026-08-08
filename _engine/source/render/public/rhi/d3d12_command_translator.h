#pragma once
// D3D12CommandTranslator — replays a RenderCommandBuffer by issuing
// equivalent D3D12 calls onto the device's per-frame graphics command
// list. Counterpart to GLCommandTranslator.
//
// Binding model (6e): the upper layer uploads constant data into a
// ConstantBufferRing and records bindConstantBuffer(binding, ring, offset,
// size) per cbuffer — no more setUniform*/SPIRV-Cross-flattened names.
// The translator resolves each binding to a root-signature CBV slot by
// looking up the current PSO's reflected per-stage cbuffer list (the same
// HLSL register() the shader was compiled with), defers the
// SetGraphicsRootConstantBufferView call to draw time, and rebinds the SRV
// table for the textures bound via bindTexture.
//
// Global root signature (shared by all PSOs):
//   slot 0: CBV b0, VERTEX      slot 1: CBV b0, PIXEL
//   slot 2: CBV b1, VERTEX      slot 3: CBV b1, PIXEL
//   slot 4: CBV b2, VERTEX      slot 5: CBV b2, PIXEL
//   slot 6: descriptor table SRV t0..t2, PIXEL
//   static samplers s0..s2 (linear wrap, LOD clamped to mip 0), PIXEL
#include "render/rhi/render_command_buffer.h"
#include "render/rhi/d3d12_rhi_device.h"
#include "core/container/dynamic_array.h"

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
    // A deferred root CBV binding: applied at draw time, after the root
    // signature and PSO are set.
    struct PendingCBV
    {
        u32 rootSlot = 0;
        D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
    };

    void bindPipeline(D3D12PipelineState *pso);
    void flushStateForDraw(ID3D12GraphicsCommandList *list);

    D3D12RHIDevice &m_device;

    D3D12PipelineState *m_current_pso = nullptr;
    DynamicArray<PendingCBV> m_pending_cbvs;

    RHITexture *m_bound_textures[8] = {};
    bool m_has_bound_textures = false;

    D3D12_PRIMITIVE_TOPOLOGY m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    bool m_pipeline_dirty = false;
};

} // namespace Entelechy
