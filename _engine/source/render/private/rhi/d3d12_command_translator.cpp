#include "render/rhi/d3d12_command_translator.h"
#include "core/math/align.h"
#include "log/core/log_macros.h"
#include <cstring>

namespace Entelechy
{

namespace
{
constexpr LogCategory kLogD3D12Cmd("Render");
constexpr u32 kUnmappedRootSlot = 0xFFFFFFFF;

// Root signature slot map (see d3d12_command_translator.h).
u32 rootSlotForCBuffer(bool vertexStage, u32 bindPoint)
{
    if (bindPoint > 2)
        return kUnmappedRootSlot;
    return bindPoint * 2 + (vertexStage ? 0 : 1);
}

D3D12_PRIMITIVE_TOPOLOGY getTopology(PrimitiveTopology topo)
{
    switch (topo)
    {
    case PrimitiveTopology::Triangles:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case PrimitiveTopology::TriangleStrip:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case PrimitiveTopology::Lines:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case PrimitiveTopology::LineStrip:
        return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case PrimitiveTopology::Points:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

D3D12_RESOURCE_STATES getResourceStates(ResourceState state)
{
    UINT result = D3D12_RESOURCE_STATE_COMMON;
    const UINT s = static_cast<UINT>(state);
    if (s & static_cast<UINT>(ResourceState::VertexBuffer))
        result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (s & static_cast<UINT>(ResourceState::IndexBuffer))
        result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (s & static_cast<UINT>(ResourceState::ConstantBuffer))
        result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (s & static_cast<UINT>(ResourceState::ShaderResource))
        result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (s & static_cast<UINT>(ResourceState::UnorderedAccess))
        result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (s & static_cast<UINT>(ResourceState::RenderTarget))
        result |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (s & static_cast<UINT>(ResourceState::DepthWrite))
        result |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if (s & static_cast<UINT>(ResourceState::DepthRead))
        result |= D3D12_RESOURCE_STATE_DEPTH_READ;
    if (s & static_cast<UINT>(ResourceState::CopySource))
        result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (s & static_cast<UINT>(ResourceState::CopyDest))
        result |= D3D12_RESOURCE_STATE_COPY_DEST;
    if (s & static_cast<UINT>(ResourceState::Present))
        result |= D3D12_RESOURCE_STATE_PRESENT;
    return static_cast<D3D12_RESOURCE_STATES>(result);
}

ID3D12Resource *getD3D12Resource(GPUResource *resource)
{
    if (auto *buffer = dynamic_cast<D3D12Buffer *>(resource))
        return buffer->resource();
    if (auto *texture = dynamic_cast<D3D12Texture *>(resource))
        return texture->resource();
    return nullptr;
}

} // namespace

D3D12CommandTranslator::D3D12CommandTranslator(D3D12RHIDevice &device) : m_device(device) {}

void D3D12CommandTranslator::resetState()
{
    m_current_pso = nullptr;
    m_pending_cbvs.clear();
    for (auto &tex : m_bound_textures)
    {
        tex = nullptr;
    }
    m_has_bound_textures = false;
    m_topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_pipeline_dirty = false;
}

// ------------------------------------------------------------------
// Pipeline binding
// ------------------------------------------------------------------
void D3D12CommandTranslator::bindPipeline(D3D12PipelineState *pso)
{
    m_current_pso = pso;
    m_pipeline_dirty = true;

    // Textures are reset on pipeline bind so each draw re-declares the ones
    // it actually uses (Material::bind binds its textures after the PSO).
    for (auto &tex : m_bound_textures)
    {
        tex = nullptr;
    }
    m_has_bound_textures = false;

    switch (pso->getDesc().topology)
    {
    default:
        m_topology = getTopology(pso->getDesc().topology);
        break;
    }
}

// ------------------------------------------------------------------
// Draw-time state flush
// ------------------------------------------------------------------
void D3D12CommandTranslator::flushStateForDraw(ID3D12GraphicsCommandList *list)
{
    if (!m_current_pso)
        return;

    if (m_pipeline_dirty)
    {
        list->SetGraphicsRootSignature(m_device.rootSignature());
        list->SetPipelineState(m_current_pso->pso());
        list->IASetPrimitiveTopology(m_topology);
        m_pipeline_dirty = false;
    }

    // Apply deferred root CBV bindings recorded by bindConstantBuffer.
    for (const PendingCBV &cbv : m_pending_cbvs)
    {
        if (cbv.rootSlot != kUnmappedRootSlot)
            list->SetGraphicsRootConstantBufferView(cbv.rootSlot, cbv.gpu);
    }
    m_pending_cbvs.clear();

    // Bind the SRV table for bound textures. Textures were bound by slot
    // (t-register); gaps are filled with the lowest bound slot's texture so
    // the descriptor range is always fully initialized.
    if (m_has_bound_textures)
    {
        u32 slotCount = 0;
        for (u32 i = 0; i < 8; ++i)
        {
            if (m_bound_textures[i])
                slotCount = i + 1;
        }
        if (slotCount > 0)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
            if (m_device.allocateSrvDescriptors(slotCount, cpuHandle, gpuHandle))
            {
                D3D12_CPU_DESCRIPTOR_HANDLE fallback{};
                bool hasFallback = false;
                for (u32 i = 0; i < slotCount; ++i)
                {
                    D3D12_CPU_DESCRIPTOR_HANDLE src{};
                    if (auto *tex = dynamic_cast<D3D12Texture *>(m_bound_textures[i]))
                    {
                        if (tex->hasSrv())
                            src = tex->srvCpuHandle();
                    }
                    if (src.ptr == 0)
                    {
                        src = fallback;
                    }
                    if (src.ptr == 0)
                        continue; // nothing bound anywhere yet — leave stale
                    if (!hasFallback)
                    {
                        fallback = src;
                        hasFallback = true;
                    }
                    D3D12_CPU_DESCRIPTOR_HANDLE dst = cpuHandle;
                    dst.ptr += static_cast<SIZE_T>(i) * m_device.srvDescriptorSize();
                    m_device.device()->CopyDescriptorsSimple(1, dst, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
                }
                list->SetGraphicsRootDescriptorTable(6, gpuHandle);
            }
        }
    }
}

// ------------------------------------------------------------------
// Main replay loop
// ------------------------------------------------------------------
void D3D12CommandTranslator::execute(ID3D12GraphicsCommandList *list, const RenderCommandBuffer &buffer)
{
    resetState();

    ID3D12DescriptorHeap *heaps[] = {m_device.srvShaderHeap()};
    list->SetDescriptorHeaps(1, heaps);

    const u8 *ptr = buffer.data();
    const u8 *end = ptr + buffer.size();

    while (ptr < end)
    {
        const auto *header = reinterpret_cast<const RenderCmdHeader *>(ptr);
        const u8 *payloadPtr = ptr + sizeof(RenderCmdHeader);

        switch (header->type)
        {
        case RenderCommandType::BeginRenderPass:
        {
            const auto *cmd = reinterpret_cast<const CmdBeginRenderPass *>(payloadPtr);
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_device.currentRtv();
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_device.dsv();
            list->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle);
            for (u32 i = 0; i < cmd->desc.colorAttachmentCount; ++i)
            {
                if (cmd->desc.colorAttachments[i].clear)
                {
                    list->ClearRenderTargetView(rtv, cmd->desc.colorAttachments[i].clearColor, 0, nullptr);
                }
            }
            break;
        }

        case RenderCommandType::EndRenderPass:
            break;

        case RenderCommandType::SetViewport:
        {
            const auto *cmd = reinterpret_cast<const CmdSetViewport *>(payloadPtr);
            D3D12_VIEWPORT vp{cmd->x, cmd->y, cmd->w, cmd->h, 0.0f, 1.0f};
            list->RSSetViewports(1, &vp);
            break;
        }

        case RenderCommandType::SetScissor:
        {
            const auto *cmd = reinterpret_cast<const CmdSetScissor *>(payloadPtr);
            D3D12_RECT rect{static_cast<LONG>(cmd->x), static_cast<LONG>(cmd->y),
                            static_cast<LONG>(cmd->x + cmd->w), static_cast<LONG>(cmd->y + cmd->h)};
            list->RSSetScissorRects(1, &rect);
            break;
        }

        case RenderCommandType::BindPipeline:
        {
            const auto *cmd = reinterpret_cast<const CmdBindPipeline *>(payloadPtr);
            if (auto *pso = dynamic_cast<D3D12PipelineState *>(cmd->pso))
            {
                bindPipeline(pso);
            }
            break;
        }

        case RenderCommandType::BindVertexBuffer:
        {
            const auto *cmd = reinterpret_cast<const CmdBindVertexBuffer *>(payloadPtr);
            if (auto *buffer = dynamic_cast<D3D12Buffer *>(cmd->buffer))
            {
                D3D12_VERTEX_BUFFER_VIEW vbv{};
                vbv.BufferLocation = buffer->gpuAddress() + cmd->offset;
                vbv.SizeInBytes = buffer->getSize() - cmd->offset;
                vbv.StrideInBytes = buffer->vertexStride();
                list->IASetVertexBuffers(cmd->slot, 1, &vbv);
            }
            break;
        }

        case RenderCommandType::BindIndexBuffer:
        {
            const auto *cmd = reinterpret_cast<const CmdBindIndexBuffer *>(payloadPtr);
            if (auto *buffer = dynamic_cast<D3D12Buffer *>(cmd->buffer))
            {
                D3D12_INDEX_BUFFER_VIEW ibv{};
                ibv.BufferLocation = buffer->gpuAddress() + cmd->offset;
                ibv.SizeInBytes = buffer->getSize() - cmd->offset;
                ibv.Format = DXGI_FORMAT_R32_UINT;
                list->IASetIndexBuffer(&ibv);
            }
            break;
        }

        case RenderCommandType::DrawIndexed:
        {
            const auto *cmd = reinterpret_cast<const CmdDrawIndexed *>(payloadPtr);
            flushStateForDraw(list);
            list->DrawIndexedInstanced(cmd->indexCount, 1, cmd->startIndex, cmd->baseVertex, 0);
            break;
        }

        case RenderCommandType::Draw:
        {
            const auto *cmd = reinterpret_cast<const CmdDraw *>(payloadPtr);
            flushStateForDraw(list);
            list->DrawInstanced(cmd->vertexCount, 1, cmd->startVertex, 0);
            break;
        }

        case RenderCommandType::ResourceBarrier:
        {
            const auto *cmd = reinterpret_cast<const CmdResourceBarrier *>(payloadPtr);
            const auto *barriers = reinterpret_cast<const BarrierDesc *>(payloadPtr + sizeof(CmdResourceBarrier));
            D3D12_RESOURCE_BARRIER native[16];
            UINT count = 0;
            for (u32 i = 0; i < cmd->count && count < 16; ++i)
            {
                if (barriers[i].beforeState == barriers[i].afterState)
                    continue;
                ID3D12Resource *resource = getD3D12Resource(barriers[i].resource);
                if (!resource)
                    continue;
                native[count].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                native[count].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                native[count].Transition.pResource = resource;
                native[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                native[count].Transition.StateBefore = getResourceStates(barriers[i].beforeState);
                native[count].Transition.StateAfter = getResourceStates(barriers[i].afterState);
                ++count;
            }
            if (count > 0)
                list->ResourceBarrier(count, native);
            break;
        }

        case RenderCommandType::ClearRenderTarget:
        {
            const auto *cmd = reinterpret_cast<const CmdClearRenderTarget *>(payloadPtr);
            list->ClearRenderTargetView(m_device.currentRtv(), cmd->color, 0, nullptr);
            break;
        }

        // Legacy immediate-uniform commands: the 6e path no longer emits
        // these (Material uploads cbuffers via ConstantBufferRing), so they
        // are dropped on D3D12. Kept in the command enum for serialization
        // compatibility with the command buffer tests.
        case RenderCommandType::SetUniformFloat:
        case RenderCommandType::SetUniformInt:
        case RenderCommandType::SetUniformVec2:
        case RenderCommandType::SetUniformVec3:
        case RenderCommandType::SetUniformVec4:
        case RenderCommandType::SetUniformMat3:
        case RenderCommandType::SetUniformMat4:
            break;

        case RenderCommandType::BindTexture:
        {
            const auto *cmd = reinterpret_cast<const CmdBindTexture *>(payloadPtr);
            if (cmd->slot < 8)
            {
                m_bound_textures[cmd->slot] = cmd->texture;
                if (cmd->texture)
                    m_has_bound_textures = true;
            }
            break;
        }

        case RenderCommandType::BindConstantBuffer:
        {
            // Resolve the binding to a root CBV slot via the PSO's reflected
            // per-stage cbuffer list (binding points are unique per stage
            // within a material), then defer the call to draw time.
            const auto *cmd = reinterpret_cast<const CmdBindConstantBuffer *>(payloadPtr);
            if (!m_current_pso || !cmd->buffer)
                break;
            auto *d3dBuffer = dynamic_cast<D3D12Buffer *>(cmd->buffer);
            if (!d3dBuffer)
                break;

            const D3D12_GPU_VIRTUAL_ADDRESS gpu = d3dBuffer->gpuAddress() + cmd->offset;
            bool matched = false;
            for (const D3D12CBufferInfo &info : m_current_pso->vsCBuffers())
            {
                if (info.bindPoint == cmd->binding)
                {
                    m_pending_cbvs.pushBack({rootSlotForCBuffer(true, cmd->binding), gpu});
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                for (const D3D12CBufferInfo &info : m_current_pso->psCBuffers())
                {
                    if (info.bindPoint == cmd->binding)
                    {
                        m_pending_cbvs.pushBack({rootSlotForCBuffer(false, cmd->binding), gpu});
                        matched = true;
                        break;
                    }
                }
            }
            if (!matched)
            {
                LOG_WARN(kLogD3D12Cmd,
                            "D3D12: bindConstantBuffer(%u) does not match any reflected cbuffer of the current PSO",
                            cmd->binding);
            }
            break;
        }

        case RenderCommandType::SetPushConstants:
            // Reserved for future root constants; not used by 6e.
            break;

        case RenderCommandType::PushDebugGroup:
        case RenderCommandType::PopDebugGroup:
        case RenderCommandType::InsertDebugMarker:
            // PIX event integration (WinPixEventRuntime) is not linked yet;
            // debug markers are silently dropped.
            break;
        }

        // Advance to next command
        const usize stride = sizeof(RenderCmdHeader) + AlignUp(header->payloadSize, alignof(std::max_align_t));
        ptr += stride;
    }
}

} // namespace Entelechy
