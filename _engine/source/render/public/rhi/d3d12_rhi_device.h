#pragma once
// D3D12 backend for the RHI layer (6d).
//
// Mirrors the GL backend structure: resource wrappers, a fence type, and
// D3D12RHIDevice implementing IRHIDevice. Commands are recorded through
// the shared deferred command buffer (RenderCommandBuffer +
// DeferredCommandList) and replayed by D3D12CommandTranslator onto a
// per-frame ID3D12GraphicsCommandList.
//
// Scope notes (functional parity with the GL backend, not more):
// - 2 frames in flight, flip-model swapchain (RGBA8)
// - One global root signature: CBV b0..b2 split by stage visibility,
//   one SRV table t0..t2, 3 static samplers (see d3d12_command_translator)
// - Textures are created with a single mip level (no runtime mip
//   generation yet — see TODO.md), pixels are row-flipped on upload so
//   cooked UVs (V-flipped for GL's bottom-left origin) sample correctly
// - createBuffer/createTexture upload synchronously via a dedicated
//   upload command list + fence wait (assets stream in async anyway)
#include "render/rhi/rhi_device.h"
#include "render/rhi/rhi_pipeline.h"
#include "core/container/dynamic_array.h"
#include "core/string/string.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace Entelechy
{

using Microsoft::WRL::ComPtr;

class RenderCommandBuffer;
class DeferredCommandList;
class D3D12CommandTranslator;

// ==================================================================
// D3D12 resource implementations
// ==================================================================

class D3D12Buffer : public RHIBuffer
{
public:
    D3D12Buffer(u32 size, BufferUsage usage, ComPtr<ID3D12Resource> resource, u32 vertexStride);

    u32 getSize() const override
    {
        return m_size;
    }
    BufferUsage getUsage() const override
    {
        return m_usage;
    }

    ID3D12Resource *resource() const
    {
        return m_resource.Get();
    }
    u32 vertexStride() const
    {
        return m_vertex_stride;
    }
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress() const
    {
        return m_resource ? m_resource->GetGPUVirtualAddress() : 0;
    }

    // Persistent CPU map for cpuAccessible (upload) buffers; valid until
    // the buffer is destroyed. Nullptr for default-heap buffers.
    void *getCpuMappedPointer() override
    {
        return m_mapped;
    }
    void setMappedPointer(void *mapped)
    {
        m_mapped = mapped;
    }

    u64 memorySizeBytes() const override
    {
        return static_cast<u64>(m_size);
    }
    void setDebugName(const String &name) override;

protected:
    void onDestroy() override
    {
        if (m_mapped)
        {
            m_resource->Unmap(0, nullptr);
            m_mapped = nullptr;
        }
        m_resource.Reset();
    }

private:
    u32 m_size = 0;
    BufferUsage m_usage = BufferUsage::None;
    ComPtr<ID3D12Resource> m_resource;
    u32 m_vertex_stride = 0;
    void *m_mapped = nullptr;
};

class D3D12Texture : public RHITexture
{
public:
    D3D12Texture(const TextureDesc &desc, ComPtr<ID3D12Resource> resource);

    const TextureDesc &getDesc() const override
    {
        return m_desc;
    }

    ID3D12Resource *resource() const
    {
        return m_resource.Get();
    }

    // CPU-side descriptor handle of this texture's SRV in the device's
    // persistent (non-shader-visible) heap. Valid only when hasSrv().
    bool hasSrv() const
    {
        return m_has_srv;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle() const
    {
        return m_srv_cpu;
    }
    void setSrvCpuHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
    {
        m_srv_cpu = handle;
        m_has_srv = true;
    }

    u64 memorySizeBytes() const override;
    void setDebugName(const String &name) override;

protected:
    void onDestroy() override
    {
        m_resource.Reset();
    }

private:
    TextureDesc m_desc;
    ComPtr<ID3D12Resource> m_resource;
    D3D12_CPU_DESCRIPTOR_HANDLE m_srv_cpu = {};
    bool m_has_srv = false;
};

class D3D12Shader : public RHIShader
{
public:
    D3D12Shader(ShaderStage stage, const void *bytecode, size_t size);

    ShaderStage getStage() const override
    {
        return m_stage;
    }
    const void *bytecode() const
    {
        return m_bytecode.data();
    }
    size_t bytecodeSize() const
    {
        return m_bytecode.size();
    }

    void setDebugName(const String &name) override;

protected:
    void onDestroy() override {}

private:
    ShaderStage m_stage = ShaderStage::None;
    std::vector<u8> m_bytecode;
};

// Reflected constant buffer binding (from D3DReflect on the DXIL blob).
struct D3D12CBufferInfo
{
    char name[32] = {};
    u32 bindPoint = 0; // b-register
    u32 size = 0;      // bytes
};

class D3D12PipelineState : public RHIPipelineState
{
public:
    D3D12PipelineState(const PipelineStateDesc &desc, ComPtr<ID3D12PipelineState> pso);

    ID3D12PipelineState *pso() const
    {
        return m_pso.Get();
    }
    const PipelineStateDesc &getDesc() const
    {
        return m_desc;
    }

    const std::vector<D3D12CBufferInfo> &vsCBuffers() const
    {
        return m_vs_cbuffers;
    }
    const std::vector<D3D12CBufferInfo> &psCBuffers() const
    {
        return m_ps_cbuffers;
    }
    void setCBuffers(std::vector<D3D12CBufferInfo> vs, std::vector<D3D12CBufferInfo> ps)
    {
        m_vs_cbuffers = std::move(vs);
        m_ps_cbuffers = std::move(ps);
    }

    void setDebugName(const String &name) override;

protected:
    void onDestroy() override
    {
        m_pso.Reset();
    }

private:
    PipelineStateDesc m_desc;
    ComPtr<ID3D12PipelineState> m_pso;
    std::vector<D3D12CBufferInfo> m_vs_cbuffers;
    std::vector<D3D12CBufferInfo> m_ps_cbuffers;
};

// ==================================================================
// D3D12 Fence implementation
// ==================================================================

class D3D12Fence : public IRHIFence
{
public:
    // The queue pointer is borrowed from the owning device; the fence
    // never outlives the device.
    D3D12Fence(ID3D12CommandQueue *queue, ComPtr<ID3D12Fence> fence, RHIFenceValue initialValue);
    ~D3D12Fence() override;

    void signal(RHIFenceValue value) override;
    bool wait(RHIFenceValue value, u64 timeoutNs = UINT64_MAX) override;
    RHIFenceValue getCompletedValue() const override;
    bool isSignaled(RHIFenceValue value) const override;

protected:
    void onDestroy() override;

private:
    ID3D12CommandQueue *m_queue = nullptr;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_event = nullptr;
};

// ==================================================================
// D3D12 RHI Device
// ==================================================================

class D3D12RHIDevice : public IRHIDevice
{
public:
    D3D12RHIDevice();
    ~D3D12RHIDevice() override;

    bool initialize();
    void shutdown();

    // -- Surface lifecycle (IRHIDevice) ------------------------------------
    bool initSurface(const SurfaceDesc &desc) override;
    void shutdownSurface() override;
    void beginFrame() override;
    void endFrame() override;
    void setClearColor(f32 r, f32 g, f32 b, f32 a) override;
    void clear(ClearFlags flags) override;
    void resizeSurface(u32 width, u32 height) override;
    bool readbackBackbuffer(std::vector<u8> &outPixelsRGBA8, u32 &outWidth, u32 &outHeight) override;

    // -- Resource creation (IRHIDevice) ------------------------------------
    RHIBufferRef createBuffer(const BufferDesc &desc, const void *initialData) override;
    RHITextureRef createTexture(const TextureDesc &desc, const void *initialData) override;
    RHIShaderRef createShader(const ShaderBytecode &bytecode) override;
    RHIPipelineStateRef createPipelineState(const PipelineStateDesc &desc) override;
    RHIFenceRef createFence(RHIFenceValue initialValue = 0) override;

    IRHICommandList *createCommandList() override;

    void submit(IRHICommandList *cmdList) override;

    RHIFenceValue signalFrame() override;
    RHIFenceValue getCompletedFenceValue() override;

    void queueResourceForDelete(GPUResource *resource) override;
    void flushPendingDeletes() override;

    RHIMemoryInfo queryMemoryInfo() const override;
    u64 getTrackedMemoryUsage() const override
    {
        return m_tracked_memory_bytes;
    }

    RenderBackendType getBackendType() const override
    {
        return RenderBackendType::D3D12;
    }

    PSOManager &getPSOManager()
    {
        return m_pso_manager;
    }

    // -- Internals used by D3D12CommandTranslator ---------------------------
    ID3D12Device *device() const
    {
        return m_device.Get();
    }
    ID3D12GraphicsCommandList *commandList() const
    {
        return m_cmd_list.Get();
    }
    ID3D12RootSignature *rootSignature() const
    {
        return m_root_signature.Get();
    }
    D3D12_CPU_DESCRIPTOR_HANDLE currentRtv() const;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv() const;

    // Allocate a 256-byte-aligned block in this frame's upload ring buffer.
    // Returns CPU pointer + GPU address. Null {nullptr,0} on exhaustion.
    struct UploadAllocation
    {
        void *cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
    };
    UploadAllocation allocateUpload(u32 size);

    // Allocate `count` consecutive descriptors in this frame's
    // shader-visible SRV heap; returns CPU (for copying into) and GPU
    // (for binding) handles of the first slot. False on exhaustion.
    bool allocateSrvDescriptors(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE &outCpu,
                                D3D12_GPU_DESCRIPTOR_HANDLE &outGpu);

    u32 srvDescriptorSize() const
    {
        return m_srv_descriptor_size;
    }

    ID3D12DescriptorHeap *srvShaderHeap() const
    {
        return m_srv_shader_heap.Get();
    }

private:
    static constexpr u32 kFrameCount = 2;
    static constexpr u32 kUploadRingSize = 8 * 1024 * 1024;
    static constexpr u32 kSrvShaderHeapSize = 4096;
    static constexpr u32 kSrvPersistentHeapSize = 256;

    // The upload ring and the shader-visible SRV heap are split into one
    // segment per in-flight backbuffer slot. beginFrame() only rewinds the
    // segment owned by the slot it just waited on, so a frame still running
    // on the GPU never has its constant data or texture descriptors
    // overwritten by the next frame — a single shared region got rewritten
    // from offset 0 every frame while only the N-2 fence had been waited on,
    // corrupting CBV payloads of the still-executing N-1 frame (visible as
    // flicker whenever the camera moved and the per-frame constants changed).
    static constexpr u64 kUploadRingSegmentSize = kUploadRingSize / kFrameCount;
    static constexpr u32 kSrvShaderSegmentSize = kSrvShaderHeapSize / kFrameCount;
    static constexpr DXGI_FORMAT kBackbufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    struct PendingDelete
    {
        GPUResource *resource = nullptr;
        RHIFenceValue fence = 0;
    };

    bool createRootSignature();
    bool createSwapChainResources(u32 width, u32 height);
    void releaseSwapChainResources();

    // Synchronous resource upload via the dedicated upload list.
    bool uploadToResource(ID3D12Resource *dst, const D3D12_SUBRESOURCE_DATA *subresources, u32 subresourceCount,
                          u64 requiredSize);

    // Reflect constant buffers of a shader blob into `out`.
    static bool reflectCBuffers(const D3D12Shader *shader, std::vector<D3D12CBufferInfo> &out);

    // Allocate a persistent SRV slot (CPU-only heap) for a texture.
    bool allocatePersistentSrv(D3D12_CPU_DESCRIPTOR_HANDLE &out);
    void freePersistentSrv(D3D12_CPU_DESCRIPTOR_HANDLE handle);

    void trackResourceCreated(const GPUResource *resource);
    void trackResourceDestroyed(const GPUResource *resource);

    RHIPipelineStateRef createPipelineStateUncached(const PipelineStateDesc &desc);

    void executeCommandListAndWait();
    void waitForFrameSlot(u32 slot);

    RenderSettings m_settings;
    PSOManager m_pso_manager;

    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGIAdapter3> m_adapter;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<IDXGISwapChain3> m_swapchain;
    HWND m_hwnd = nullptr;

    ComPtr<ID3D12RootSignature> m_root_signature;

    // Render targets
    ComPtr<ID3D12DescriptorHeap> m_rtv_heap;
    ComPtr<ID3D12Resource> m_backbuffers[kFrameCount];
    u32 m_rtv_descriptor_size = 0;
    ComPtr<ID3D12DescriptorHeap> m_dsv_heap;
    ComPtr<ID3D12Resource> m_depth_buffer;
    u32 m_surface_width = 0;
    u32 m_surface_height = 0;

    // Per-frame command recording
    ComPtr<ID3D12CommandAllocator> m_allocators[kFrameCount];
    ComPtr<ID3D12GraphicsCommandList> m_cmd_list;
    u32 m_frame_index = 0;
    bool m_cmd_list_open = false;

    // Frame fence timeline
    ComPtr<ID3D12Fence> m_frame_fence;
    HANDLE m_frame_event = nullptr;
    RHIFenceValue m_next_frame_value = 1;
    RHIFenceValue m_frame_slot_values[kFrameCount] = {};

    // Per-frame upload ring buffer (constant data)
    ComPtr<ID3D12Resource> m_upload_ring;
    u8 *m_upload_ring_mapped = nullptr;
    u64 m_upload_ring_offset = 0;

    // Per-frame shader-visible SRV heap (bump allocator)
    ComPtr<ID3D12DescriptorHeap> m_srv_shader_heap;
    u32 m_srv_shader_cursor = 0;
    u32 m_srv_descriptor_size = 0;

    // Persistent CPU-only SRV heap (texture descriptors, free-list)
    ComPtr<ID3D12DescriptorHeap> m_srv_persistent_heap;
    DynamicArray<u32> m_srv_persistent_free_list;

    // Dedicated synchronous upload context (buffer/texture creation)
    ComPtr<ID3D12CommandAllocator> m_upload_allocator;
    ComPtr<ID3D12GraphicsCommandList> m_upload_list;
    ComPtr<ID3D12Fence> m_upload_fence;
    HANDLE m_upload_event = nullptr;
    u64 m_upload_fence_value = 0;

    // Readback
    ComPtr<ID3D12Resource> m_readback_buffer;
    u64 m_readback_size = 0;

    // Deferred command buffer (pimpl; full types only in the .cpp)
    struct DeferredState;
    std::unique_ptr<DeferredState> m_deferred;

    bool m_initialized = false;

    DynamicArray<PendingDelete> m_pending_deletes;
    u64 m_tracked_memory_bytes = 0;
};

} // namespace Entelechy
