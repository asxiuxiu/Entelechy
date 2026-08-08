#include "render/rhi/d3d12_rhi_device.h"
#include "render/rhi/render_command_buffer.h"
#include "render/rhi/deferred_command_list.h"
#include "render/rhi/d3d12_command_translator.h"
#include "log/core/log_macros.h"
#include "core/allocator/allocator.h"
#include <d3d12shader.h>
#include <dxcapi.h>
#include <cstring>

namespace Entelechy
{

namespace
{
template <typename T, typename... Args>
T *allocateResource(Args &&...args)
{
    void *mem = DefaultAllocator::alloc(sizeof(T), alignof(T));
    return new (mem) T(std::forward<Args>(args)...);
}

constexpr LogCategory kLogD3D12("Render");

// ==================================================================
// Enum mapping helpers
// ==================================================================

DXGI_FORMAT getDXGIFormat(TextureFormat fmt)
{
    switch (fmt)
    {
    case TextureFormat::R8_UNORM:
        return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::RG8_UNORM:
        return DXGI_FORMAT_R8G8_UNORM;
    case TextureFormat::RGBA8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::BGRA8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::R16_FLOAT:
        return DXGI_FORMAT_R16_FLOAT;
    case TextureFormat::RG16_FLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGBA16_FLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R32_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::RG32_FLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::RGBA32_FLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::D24_UNORM_S8_UINT:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TextureFormat::D32_FLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    default:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
}

u32 getFormatBytesPerPixel(TextureFormat fmt)
{
    switch (fmt)
    {
    case TextureFormat::R8_UNORM:
        return 1;
    case TextureFormat::RG8_UNORM:
    case TextureFormat::R16_FLOAT:
        return 2;
    case TextureFormat::RGBA8_UNORM:
    case TextureFormat::RGBA8_SRGB:
    case TextureFormat::BGRA8_UNORM:
    case TextureFormat::RG16_FLOAT:
    case TextureFormat::R32_FLOAT:
    case TextureFormat::D24_UNORM_S8_UINT:
    case TextureFormat::D32_FLOAT:
        return 4;
    case TextureFormat::RGBA16_FLOAT:
    case TextureFormat::RG32_FLOAT:
        return 8;
    case TextureFormat::RGBA32_FLOAT:
        return 16;
    default:
        return 4;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE getTopologyType(PrimitiveTopology topo)
{
    switch (topo)
    {
    case PrimitiveTopology::Triangles:
    case PrimitiveTopology::TriangleStrip:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case PrimitiveTopology::Lines:
    case PrimitiveTopology::LineStrip:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case PrimitiveTopology::Points:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    }
}

D3D12_CULL_MODE getCullMode(CullMode mode)
{
    switch (mode)
    {
    case CullMode::None:
        return D3D12_CULL_MODE_NONE;
    case CullMode::Front:
        return D3D12_CULL_MODE_FRONT;
    case CullMode::Back:
        return D3D12_CULL_MODE_BACK;
    default:
        return D3D12_CULL_MODE_BACK;
    }
}

D3D12_COMPARISON_FUNC getCompareFunc(CompareFunc fn)
{
    switch (fn)
    {
    case CompareFunc::Never:
        return D3D12_COMPARISON_FUNC_NEVER;
    case CompareFunc::Less:
        return D3D12_COMPARISON_FUNC_LESS;
    case CompareFunc::Equal:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case CompareFunc::LessEqual:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case CompareFunc::Greater:
        return D3D12_COMPARISON_FUNC_GREATER;
    case CompareFunc::NotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case CompareFunc::GreaterEqual:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case CompareFunc::Always:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    default:
        return D3D12_COMPARISON_FUNC_LESS;
    }
}

D3D12_BLEND getBlendFactor(BlendFactor f)
{
    switch (f)
    {
    case BlendFactor::Zero:
        return D3D12_BLEND_ZERO;
    case BlendFactor::One:
        return D3D12_BLEND_ONE;
    case BlendFactor::SrcColor:
        return D3D12_BLEND_SRC_COLOR;
    case BlendFactor::OneMinusSrcColor:
        return D3D12_BLEND_INV_SRC_COLOR;
    case BlendFactor::SrcAlpha:
        return D3D12_BLEND_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case BlendFactor::DstColor:
        return D3D12_BLEND_DEST_COLOR;
    case BlendFactor::OneMinusDstColor:
        return D3D12_BLEND_INV_DEST_COLOR;
    case BlendFactor::DstAlpha:
        return D3D12_BLEND_DEST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:
        return D3D12_BLEND_INV_DEST_ALPHA;
    default:
        return D3D12_BLEND_ONE;
    }
}

D3D12_BLEND_OP getBlendOp(BlendOp op)
{
    switch (op)
    {
    case BlendOp::Add:
        return D3D12_BLEND_OP_ADD;
    case BlendOp::Subtract:
        return D3D12_BLEND_OP_SUBTRACT;
    case BlendOp::ReverseSubtract:
        return D3D12_BLEND_OP_REV_SUBTRACT;
    case BlendOp::Min:
        return D3D12_BLEND_OP_MIN;
    case BlendOp::Max:
        return D3D12_BLEND_OP_MAX;
    default:
        return D3D12_BLEND_OP_ADD;
    }
}

D3D12_FILL_MODE getFillMode(FillMode mode)
{
    return mode == FillMode::Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
}

// Vertex attribute location -> HLSL semantic. The engine's interleaved
// mesh layout pins locations 0..3 (see prepare_assets_system s_meshAttrs);
// D3D12 input layouts need semantic names, so they are reconstructed here.
const char *getSemanticName(u32 location)
{
    switch (location)
    {
    case 0:
        return "POSITION";
    case 1:
        return "NORMAL";
    case 2:
        return "TEXCOORD";
    case 3:
        return "TANGENT";
    default:
        return "COLOR";
    }
}

DXGI_FORMAT getAttributeFormat(u32 components)
{
    switch (components)
    {
    case 1:
        return DXGI_FORMAT_R32_FLOAT;
    case 2:
        return DXGI_FORMAT_R32G32_FLOAT;
    case 3:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case 4:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    default:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    }
}

void setObjectName(ID3D12Object *obj, const String &name)
{
    if (!obj || name.empty())
        return;
    wchar_t wide[256];
    const int len = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, wide, 255);
    if (len > 0)
        obj->SetName(wide);
}

} // namespace

// ==================================================================
// D3D12Buffer
// ==================================================================
D3D12Buffer::D3D12Buffer(u32 size, BufferUsage usage, ComPtr<ID3D12Resource> resource, u32 vertexStride)
    : m_size(size), m_usage(usage), m_resource(std::move(resource)), m_vertex_stride(vertexStride)
{
}

void D3D12Buffer::setDebugName(const String &name)
{
    setObjectName(m_resource.Get(), name);
}

// ==================================================================
// D3D12Texture
// ==================================================================
D3D12Texture::D3D12Texture(const TextureDesc &desc, ComPtr<ID3D12Resource> resource)
    : m_desc(desc), m_resource(std::move(resource))
{
}

u64 D3D12Texture::memorySizeBytes() const
{
    const u64 bpp = getFormatBytesPerPixel(m_desc.format);
    return static_cast<u64>(m_desc.width) * m_desc.height * m_desc.depth * bpp * m_desc.arrayLayers;
}

void D3D12Texture::setDebugName(const String &name)
{
    setObjectName(m_resource.Get(), name);
}

// ==================================================================
// D3D12Shader
// ==================================================================
D3D12Shader::D3D12Shader(ShaderStage stage, const void *bytecode, size_t size) : m_stage(stage)
{
    m_bytecode.resize(size);
    std::memcpy(m_bytecode.data(), bytecode, size);
}

void D3D12Shader::setDebugName(const String & /*name*/)
{
    // Shader blobs are baked into PSOs; name the PSO instead.
}

// ==================================================================
// D3D12PipelineState
// ==================================================================
D3D12PipelineState::D3D12PipelineState(const PipelineStateDesc &desc, ComPtr<ID3D12PipelineState> pso)
    : m_desc(desc), m_pso(std::move(pso))
{
}

void D3D12PipelineState::setDebugName(const String &name)
{
    setObjectName(m_pso.Get(), name);
}

// ==================================================================
// D3D12Fence
// ==================================================================
D3D12Fence::D3D12Fence(ID3D12CommandQueue *queue, ComPtr<ID3D12Fence> fence, RHIFenceValue initialValue)
    : m_queue(queue), m_fence(std::move(fence))
{
    m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (initialValue > 0)
    {
        signal(initialValue);
    }
}

D3D12Fence::~D3D12Fence()
{
    if (m_event)
    {
        CloseHandle(m_event);
        m_event = nullptr;
    }
}

void D3D12Fence::signal(RHIFenceValue value)
{
    if (m_queue && m_fence)
    {
        m_queue->Signal(m_fence.Get(), value);
    }
}

bool D3D12Fence::wait(RHIFenceValue value, u64 timeoutNs)
{
    if (!m_fence)
        return false;
    if (m_fence->GetCompletedValue() >= value)
        return true;
    if (FAILED(m_fence->SetEventOnCompletion(value, m_event)))
        return false;
    const DWORD timeoutMs = (timeoutNs == UINT64_MAX) ? INFINITE : static_cast<DWORD>(timeoutNs / 1000000);
    return WaitForSingleObject(m_event, timeoutMs) == WAIT_OBJECT_0;
}

RHIFenceValue D3D12Fence::getCompletedValue() const
{
    return m_fence ? m_fence->GetCompletedValue() : 0;
}

bool D3D12Fence::isSignaled(RHIFenceValue value) const
{
    return m_fence && m_fence->GetCompletedValue() >= value;
}

void D3D12Fence::onDestroy()
{
    m_fence.Reset();
}

// ==================================================================
// D3D12RHIDevice::DeferredState
// ==================================================================
struct D3D12RHIDevice::DeferredState
{
    RenderCommandBuffer buffer;
    DeferredCommandList recorder;
    D3D12CommandTranslator translator;

    explicit DeferredState(D3D12RHIDevice &device, usize capacity)
        : buffer(capacity), recorder(buffer), translator(device)
    {
    }
};

// ==================================================================
// D3D12RHIDevice
// ==================================================================
D3D12RHIDevice::D3D12RHIDevice() = default;

D3D12RHIDevice::~D3D12RHIDevice()
{
    if (m_initialized)
    {
        shutdown();
    }
}

bool D3D12RHIDevice::initialize()
{
    if (m_initialized)
        return true;

#if defined(_DEBUG)
    // Enable the D3D12 debug layer in debug builds (validation errors go
    // to the debugger output; also visible in PIX).
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    if (FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateDXGIFactory2 failed");
        return false;
    }

    // Pick the first hardware adapter (highest-performance GPU first via
    // EnumAdapterByGpuPreference when available).
    ComPtr<IDXGIAdapter1> adapter1;
    ComPtr<IDXGIFactory6> factory6;
    HRESULT hr = E_FAIL;
    if (SUCCEEDED(m_factory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        hr = factory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                                  IID_PPV_ARGS(&adapter1));
    }
    else
    {
        hr = m_factory->EnumAdapters1(0, &adapter1);
    }
    if (FAILED(hr) || !adapter1)
    {
        LOG_ERROR(kLogD3D12, "D3D12: no DXGI adapter found");
        return false;
    }

    DXGI_ADAPTER_DESC1 adapterDesc{};
    adapter1->GetDesc1(&adapterDesc);
    if (adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
    {
        LOG_ERROR(kLogD3D12, "D3D12: only a software (WARP) adapter is available, refusing");
        return false;
    }

    if (FAILED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: D3D12CreateDevice failed");
        return false;
    }
    if (FAILED(adapter1.As(&m_adapter)))
    {
        LOG_ERROR(kLogD3D12, "D3D12: adapter does not support IDXGIAdapter3");
        return false;
    }

    {
        char name[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, adapterDesc.Description, -1, name, sizeof(name) - 1, nullptr, nullptr);
        LOG_INFO(kLogD3D12, "D3D12 device created on adapter: %s", name);
    }

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_queue))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateCommandQueue failed");
        return false;
    }

    // Frame fence timeline + wait event
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_frame_fence))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateFence failed");
        return false;
    }
    m_frame_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    // Command allocators + the single per-frame graphics command list
    for (u32 i = 0; i < kFrameCount; ++i)
    {
        if (FAILED(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_allocators[i]))))
        {
            LOG_ERROR(kLogD3D12, "D3D12: CreateCommandAllocator failed");
            return false;
        }
    }
    if (FAILED(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocators[0].Get(), nullptr,
                                           IID_PPV_ARGS(&m_cmd_list))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateCommandList failed");
        return false;
    }
    m_cmd_list->Close();

    // Descriptor heaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = kFrameCount;
    m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtv_heap));
    m_rtv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.NumDescriptors = 1;
    m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsv_heap));

    m_srv_descriptor_size = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC srvShaderDesc{};
    srvShaderDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvShaderDesc.NumDescriptors = kSrvShaderHeapSize;
    srvShaderDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_device->CreateDescriptorHeap(&srvShaderDesc, IID_PPV_ARGS(&m_srv_shader_heap));

    D3D12_DESCRIPTOR_HEAP_DESC srvPersistDesc{};
    srvPersistDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvPersistDesc.NumDescriptors = kSrvPersistentHeapSize;
    m_device->CreateDescriptorHeap(&srvPersistDesc, IID_PPV_ARGS(&m_srv_persistent_heap));
    for (u32 i = 0; i < kSrvPersistentHeapSize; ++i)
    {
        m_srv_persistent_free_list.pushBack(kSrvPersistentHeapSize - 1 - i);
    }

    // Per-frame upload ring (constant data, persistently mapped)
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC ringDesc{};
    ringDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ringDesc.Width = kUploadRingSize;
    ringDesc.Height = 1;
    ringDesc.DepthOrArraySize = 1;
    ringDesc.MipLevels = 1;
    ringDesc.SampleDesc.Count = 1;
    ringDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &ringDesc,
                                                 D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                 IID_PPV_ARGS(&m_upload_ring))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: upload ring creation failed");
        return false;
    }
    m_upload_ring->Map(0, nullptr, reinterpret_cast<void **>(&m_upload_ring_mapped));

    // Dedicated synchronous upload context for resource creation
    m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_upload_allocator));
    m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_upload_allocator.Get(), nullptr,
                                IID_PPV_ARGS(&m_upload_list));
    m_upload_list->Close();
    m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_upload_fence));
    m_upload_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    if (!createRootSignature())
    {
        return false;
    }

    m_deferred = std::make_unique<DeferredState>(*this, 4 * 1024 * 1024);

    m_initialized = true;
    LOG_INFO(kLogD3D12, "D3D12RHIDevice initialized");
    return true;
}

bool D3D12RHIDevice::createRootSignature()
{
    // Slot layout documented in d3d12_command_translator.h.
    D3D12_ROOT_PARAMETER params[7]{};
    auto setCbv = [&](u32 slot, u32 reg, D3D12_SHADER_VISIBILITY visibility) {
        params[slot].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        params[slot].Descriptor.ShaderRegister = reg;
        params[slot].Descriptor.RegisterSpace = 0;
        params[slot].ShaderVisibility = visibility;
    };
    setCbv(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
    setCbv(1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
    setCbv(2, 1, D3D12_SHADER_VISIBILITY_VERTEX);
    setCbv(3, 1, D3D12_SHADER_VISIBILITY_PIXEL);
    setCbv(4, 2, D3D12_SHADER_VISIBILITY_VERTEX);
    setCbv(5, 2, D3D12_SHADER_VISIBILITY_PIXEL);

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 3;
    srvRange.BaseShaderRegister = 0; // t0..t2
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;
    params[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[6].DescriptorTable.NumDescriptorRanges = 1;
    params[6].DescriptorTable.pDescriptorRanges = &srvRange;
    params[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static samplers s0..s2: linear wrap, LOD clamped to mip 0 (textures
    // are created single-mip until runtime mip generation lands).
    D3D12_STATIC_SAMPLER_DESC samplers[3]{};
    for (u32 i = 0; i < 3; ++i)
    {
        samplers[i].Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        samplers[i].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[i].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[i].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplers[i].MipLODBias = 0.0f;
        samplers[i].MaxAnisotropy = 1;
        samplers[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplers[i].MinLOD = 0.0f;
        samplers[i].MaxLOD = 0.0f;
        samplers[i].ShaderRegister = i;
        samplers[i].RegisterSpace = 0;
        samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    }

    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 7;
    rootDesc.pParameters = params;
    rootDesc.NumStaticSamplers = 3;
    rootDesc.pStaticSamplers = samplers;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    HRESULT hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (FAILED(hr))
    {
        LOG_ERROR(kLogD3D12, "D3D12: root signature serialize failed: %s",
                  errors ? static_cast<const char *>(errors->GetBufferPointer()) : "(no details)");
        return false;
    }
    hr = m_device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
                                       IID_PPV_ARGS(&m_root_signature));
    if (FAILED(hr))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateRootSignature failed");
        return false;
    }
    return true;
}

void D3D12RHIDevice::shutdown()
{
    if (!m_initialized)
        return;

    // Wait for the GPU to drain before releasing anything.
    if (m_queue && m_frame_fence)
    {
        const RHIFenceValue v = m_next_frame_value++;
        m_queue->Signal(m_frame_fence.Get(), v);
        m_frame_fence->SetEventOnCompletion(v, m_frame_event);
        WaitForSingleObject(m_frame_event, INFINITE);
    }

    m_pso_manager.clear();
    for (auto &pd : m_pending_deletes)
    {
        trackResourceDestroyed(pd.resource);
        pd.resource->internalDestroy();
    }
    m_pending_deletes.clear();

    releaseSwapChainResources();

    m_upload_ring.Reset();
    m_upload_ring_mapped = nullptr;
    m_upload_list.Reset();
    m_upload_allocator.Reset();
    m_upload_fence.Reset();
    if (m_upload_event)
    {
        CloseHandle(m_upload_event);
        m_upload_event = nullptr;
    }

    m_deferred.reset();
    m_cmd_list.Reset();
    for (auto &alloc : m_allocators)
    {
        alloc.Reset();
    }
    m_frame_fence.Reset();
    if (m_frame_event)
    {
        CloseHandle(m_frame_event);
        m_frame_event = nullptr;
    }
    m_srv_shader_heap.Reset();
    m_srv_persistent_heap.Reset();
    m_srv_persistent_free_list.clear();
    m_rtv_heap.Reset();
    m_dsv_heap.Reset();
    m_root_signature.Reset();
    m_swapchain.Reset();
    m_queue.Reset();
    m_device.Reset();
    m_adapter.Reset();
    m_factory.Reset();

    m_tracked_memory_bytes = 0;
    m_initialized = false;
    LOG_INFO(kLogD3D12, "D3D12RHIDevice shut down");
}

// -- Surface lifecycle -----------------------------------------------------

bool D3D12RHIDevice::initSurface(const SurfaceDesc &desc)
{
    if (!m_initialized)
    {
        LOG_ERROR(kLogD3D12, "D3D12::initSurface: device not initialized");
        return false;
    }
    if (!desc.nativeWindow)
    {
        LOG_ERROR(kLogD3D12, "D3D12::initSurface: SurfaceDesc.nativeWindow must be an HWND");
        return false;
    }
    m_hwnd = static_cast<HWND>(desc.nativeWindow);
    m_settings.vsync = desc.vsync;
    m_surface_width = desc.width > 0 ? desc.width : 1280;
    m_surface_height = desc.height > 0 ? desc.height : 720;

    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.BufferCount = kFrameCount;
    scDesc.Width = m_surface_width;
    scDesc.Height = m_surface_height;
    scDesc.Format = kBackbufferFormat;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapchain1;
    if (FAILED(m_factory->CreateSwapChainForHwnd(m_queue.Get(), m_hwnd, &scDesc, nullptr, nullptr, &swapchain1)))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateSwapChainForHwnd failed");
        return false;
    }
    m_factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(swapchain1->QueryInterface(IID_PPV_ARGS(&m_swapchain))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: swapchain QI for IDXGISwapChain3 failed");
        return false;
    }
    m_frame_index = m_swapchain->GetCurrentBackBufferIndex();

    if (!createSwapChainResources(m_surface_width, m_surface_height))
    {
        return false;
    }

    LOG_INFO(kLogD3D12, "D3D12 surface initialized (%ux%u, vsync=%d)", m_surface_width, m_surface_height,
             m_settings.vsync ? 1 : 0);
    return true;
}

bool D3D12RHIDevice::createSwapChainResources(u32 width, u32 height)
{
    // Backbuffer RTVs
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for (u32 i = 0; i < kFrameCount; ++i)
    {
        if (FAILED(m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_backbuffers[i]))))
        {
            LOG_ERROR(kLogD3D12, "D3D12: swapchain GetBuffer(%u) failed", i);
            return false;
        }
        m_device->CreateRenderTargetView(m_backbuffers[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += m_rtv_descriptor_size;
    }

    // Depth buffer (D32_FLOAT, matching the GL default framebuffer's depth)
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC depthDesc{};
    depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.DepthOrArraySize = 1;
    depthDesc.MipLevels = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &depthDesc,
                                                 D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                                 IID_PPV_ARGS(&m_depth_buffer))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: depth buffer creation failed");
        return false;
    }
    m_device->CreateDepthStencilView(m_depth_buffer.Get(), nullptr, m_dsv_heap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

void D3D12RHIDevice::releaseSwapChainResources()
{
    m_depth_buffer.Reset();
    for (auto &bb : m_backbuffers)
    {
        bb.Reset();
    }
}

void D3D12RHIDevice::shutdownSurface()
{
    if (!m_initialized)
        return;
    // Drain the GPU, then release swapchain-owned resources.
    const RHIFenceValue v = m_next_frame_value++;
    m_queue->Signal(m_frame_fence.Get(), v);
    m_frame_fence->SetEventOnCompletion(v, m_frame_event);
    WaitForSingleObject(m_frame_event, INFINITE);
    releaseSwapChainResources();
    m_swapchain.Reset();
    m_hwnd = nullptr;
}

void D3D12RHIDevice::waitForFrameSlot(u32 slot)
{
    const RHIFenceValue expected = m_frame_slot_values[slot];
    if (expected > 0 && m_frame_fence->GetCompletedValue() < expected)
    {
        m_frame_fence->SetEventOnCompletion(expected, m_frame_event);
        WaitForSingleObject(m_frame_event, INFINITE);
    }
}

void D3D12RHIDevice::beginFrame()
{
    if (!m_swapchain)
        return;

    // Wait until the frame that last used this slot's allocator finished.
    waitForFrameSlot(m_frame_index);

    m_allocators[m_frame_index]->Reset();
    m_cmd_list->Reset(m_allocators[m_frame_index].Get(), nullptr);
    m_cmd_list_open = true;

    // Backbuffer: PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_backbuffers[m_frame_index].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_cmd_list->ResourceBarrier(1, &barrier);

    // Per-frame allocators reset
    m_upload_ring_offset = static_cast<u64>(m_frame_index) * kUploadRingSegmentSize;
    m_srv_shader_cursor = m_frame_index * kSrvShaderSegmentSize;

    // Default viewport/scissor (mirrors GLRHIDevice::beginFrame's glViewport)
    D3D12_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = static_cast<f32>(m_surface_width);
    vp.Height = static_cast<f32>(m_surface_height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_cmd_list->RSSetViewports(1, &vp);
    D3D12_RECT scissor{0, 0, static_cast<LONG>(m_surface_width), static_cast<LONG>(m_surface_height)};
    m_cmd_list->RSSetScissorRects(1, &scissor);

    // Bind render targets for device-level clear() calls that follow.
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = currentRtv();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsv();
    m_cmd_list->OMSetRenderTargets(1, &rtv, FALSE, &dsvHandle);
}

void D3D12RHIDevice::endFrame()
{
    if (!m_swapchain)
        return;

    if (m_cmd_list_open)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_backbuffers[m_frame_index].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        m_cmd_list->ResourceBarrier(1, &barrier);
        m_cmd_list->Close();
        ID3D12CommandList *lists[] = {m_cmd_list.Get()};
        m_queue->ExecuteCommandLists(1, lists);
        m_cmd_list_open = false;
    }

    m_swapchain->Present(m_settings.vsync ? 1 : 0, 0);

    // Signal this frame's fence value for the slot, then advance.
    m_frame_slot_values[m_frame_index] = signalFrame();
    m_frame_index = m_swapchain->GetCurrentBackBufferIndex();
}

void D3D12RHIDevice::setClearColor(f32 r, f32 g, f32 b, f32 a)
{
    m_settings.clearColor[0] = r;
    m_settings.clearColor[1] = g;
    m_settings.clearColor[2] = b;
    m_settings.clearColor[3] = a;
}

void D3D12RHIDevice::clear(ClearFlags flags)
{
    if (!m_cmd_list_open)
        return;
    if (hasFlag(flags, ClearFlags::Color))
    {
        m_cmd_list->ClearRenderTargetView(currentRtv(), m_settings.clearColor, 0, nullptr);
    }
    if (hasFlag(flags, ClearFlags::Depth) || hasFlag(flags, ClearFlags::Stencil))
    {
        D3D12_CLEAR_FLAGS df = static_cast<D3D12_CLEAR_FLAGS>(0);
        if (hasFlag(flags, ClearFlags::Depth))
            df |= D3D12_CLEAR_FLAG_DEPTH;
        if (hasFlag(flags, ClearFlags::Stencil))
            df |= D3D12_CLEAR_FLAG_STENCIL;
        m_cmd_list->ClearDepthStencilView(dsv(), df, 1.0f, 0, 0, nullptr);
    }
}

void D3D12RHIDevice::resizeSurface(u32 width, u32 height)
{
    if (!m_swapchain || width == 0 || height == 0)
        return;
    if (width == m_surface_width && height == m_surface_height)
        return;

    // Drain the GPU before touching swapchain buffers.
    const RHIFenceValue v = m_next_frame_value++;
    m_queue->Signal(m_frame_fence.Get(), v);
    m_frame_fence->SetEventOnCompletion(v, m_frame_event);
    WaitForSingleObject(m_frame_event, INFINITE);
    for (auto &slotValue : m_frame_slot_values)
    {
        slotValue = 0;
    }

    releaseSwapChainResources();
    if (FAILED(m_swapchain->ResizeBuffers(kFrameCount, width, height, kBackbufferFormat, 0)))
    {
        LOG_ERROR(kLogD3D12, "D3D12: ResizeBuffers(%ux%u) failed", width, height);
        return;
    }
    m_surface_width = width;
    m_surface_height = height;
    m_frame_index = m_swapchain->GetCurrentBackBufferIndex();
    createSwapChainResources(width, height);
}

bool D3D12RHIDevice::readbackBackbuffer(std::vector<u8> &outPixelsRGBA8, u32 &outWidth, u32 &outHeight)
{
    if (!m_cmd_list_open || !m_backbuffers[m_frame_index])
        return false;

    ID3D12Resource *backbuffer = m_backbuffers[m_frame_index].Get();
    const D3D12_RESOURCE_DESC bbDesc = backbuffer->GetDesc();

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    u32 numRows = 0;
    u64 rowSize = 0;
    u64 requiredSize = 0;
    m_device->GetCopyableFootprints(&bbDesc, 0, 1, 0, &footprint, &numRows, &rowSize, &requiredSize);

    if (requiredSize > m_readback_size)
    {
        m_readback_buffer.Reset();
        D3D12_HEAP_PROPERTIES readbackHeap{};
        readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC bufDesc{};
        bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufDesc.Width = requiredSize;
        bufDesc.Height = 1;
        bufDesc.DepthOrArraySize = 1;
        bufDesc.MipLevels = 1;
        bufDesc.SampleDesc.Count = 1;
        bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (FAILED(m_device->CreateCommittedResource(&readbackHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                     IID_PPV_ARGS(&m_readback_buffer))))
        {
            LOG_ERROR(kLogD3D12, "D3D12: readback buffer creation failed");
            return false;
        }
        m_readback_size = requiredSize;
    }

    D3D12_RESOURCE_BARRIER barriers[2]{};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = backbuffer;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;

    barriers[1] = barriers[0];
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    m_cmd_list->ResourceBarrier(1, &barriers[0]);

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = m_readback_buffer.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLoc.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = backbuffer;
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLoc.SubresourceIndex = 0;
    m_cmd_list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    m_cmd_list->ResourceBarrier(1, &barriers[1]);
    m_cmd_list->Close();
    ID3D12CommandList *lists[] = {m_cmd_list.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    m_cmd_list_open = false;

    // Wait synchronously for the copy.
    const RHIFenceValue v = m_next_frame_value++;
    m_queue->Signal(m_frame_fence.Get(), v);
    m_frame_fence->SetEventOnCompletion(v, m_frame_event);
    WaitForSingleObject(m_frame_event, INFINITE);

    // Reopen the list so endFrame() can close it normally.
    m_allocators[m_frame_index]->Reset();
    m_cmd_list->Reset(m_allocators[m_frame_index].Get(), nullptr);
    m_cmd_list_open = true;

    // Copy pixels out, un-padding rows. D3D12 readback rows are top-down
    // already — no vertical flip (unlike GL).
    const u32 width = static_cast<u32>(bbDesc.Width);
    const u32 height = bbDesc.Height;
    outPixelsRGBA8.resize(static_cast<size_t>(width) * height * 4);

    void *mapped = nullptr;
    D3D12_RANGE readRange{0, requiredSize};
    if (FAILED(m_readback_buffer->Map(0, &readRange, &mapped)))
        return false;

    const u8 *src = static_cast<const u8 *>(mapped) + footprint.Offset;
    const u32 srcPitch = footprint.Footprint.RowPitch;
    const u32 dstPitch = width * 4;
    for (u32 row = 0; row < height; ++row)
    {
        std::memcpy(outPixelsRGBA8.data() + static_cast<size_t>(row) * dstPitch,
                    src + static_cast<size_t>(row) * srcPitch, dstPitch);
    }
    m_readback_buffer->Unmap(0, nullptr);

    outWidth = width;
    outHeight = height;
    return true;
}

// -- Resource creation -------------------------------------------------------

RHIBufferRef D3D12RHIDevice::createBuffer(const BufferDesc &desc, const void *initialData)
{
    if (desc.size == 0)
        return nullptr;

    D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = desc.size;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> resource;
    const bool cpuAccessible = desc.cpuAccessible || hasFlag(desc.usage, BufferUsage::Uniform);

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = cpuAccessible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT;
    const D3D12_RESOURCE_STATES initialState =
        cpuAccessible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;

    if (FAILED(m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initialState, nullptr,
                                                 IID_PPV_ARGS(&resource))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: buffer creation failed (%u bytes)", desc.size);
        return nullptr;
    }

    void *persistentMap = nullptr;
    if (cpuAccessible)
    {
        // Keep the upload-heap resource persistently mapped for the buffer's
        // lifetime (ConstantBufferRing memcpys into it every frame).
        if (FAILED(resource->Map(0, nullptr, &persistentMap)))
        {
            LOG_ERROR(kLogD3D12, "D3D12: buffer persistent map failed (%u bytes)", desc.size);
            return nullptr;
        }
    }

    if (initialData)
    {
        if (cpuAccessible)
        {
            std::memcpy(persistentMap, initialData, desc.size);
        }
        else
        {
            // Default-heap buffers stay in COMMON: implicit promotion covers
            // COPY_DEST (upload) and later VB/IB reads — no explicit barrier.
            D3D12_HEAP_PROPERTIES uploadHeap{};
            uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
            ComPtr<ID3D12Resource> staging;
            if (FAILED(m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                                                         D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                         IID_PPV_ARGS(&staging))))
            {
                return nullptr;
            }
            void *mapped = nullptr;
            staging->Map(0, nullptr, &mapped);
            std::memcpy(mapped, initialData, desc.size);
            staging->Unmap(0, nullptr);

            m_upload_allocator->Reset();
            m_upload_list->Reset(m_upload_allocator.Get(), nullptr);
            m_upload_list->CopyBufferRegion(resource.Get(), 0, staging.Get(), 0, desc.size);
            if (!uploadToResource(nullptr, nullptr, 0, 0)) // flush + wait (no texture barrier needed)
            {
                return nullptr;
            }
        }
    }

    auto *buffer = allocateResource<D3D12Buffer>(desc.size, desc.usage, std::move(resource), desc.vertexStride);
    buffer->setMappedPointer(persistentMap);
    buffer->setDevice(this);
    trackResourceCreated(buffer);
    return RHIBufferRef(buffer);
}

RHITextureRef D3D12RHIDevice::createTexture(const TextureDesc &desc, const void *initialData)
{
    if (desc.width == 0 || desc.height == 0)
        return nullptr;

    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = desc.depth > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = desc.width;
    texDesc.Height = desc.height;
    texDesc.DepthOrArraySize = static_cast<UINT16>(desc.depth > 1 ? desc.depth : desc.arrayLayers);
    // Single mip level: runtime mip generation is not implemented yet
    // (GL calls glGenerateMipmap; D3D12 needs an explicit downsample pass —
    // tracked in TODO.md). The static samplers clamp LOD to mip 0.
    texDesc.MipLevels = 1;
    texDesc.Format = getDXGIFormat(desc.format);
    texDesc.SampleDesc.Count = 1;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    ComPtr<ID3D12Resource> resource;
    if (FAILED(m_device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                                                 D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: texture creation failed (%ux%u)", desc.width, desc.height);
        return nullptr;
    }

    if (initialData)
    {
        // Footprint of the destination subresource.
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        u32 numRows = 0;
        u64 rowSize = 0;
        u64 requiredSize = 0;
        m_device->GetCopyableFootprints(&texDesc, 0, 1, 0, &footprint, &numRows, &rowSize, &requiredSize);

        D3D12_HEAP_PROPERTIES uploadHeap{};
        uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC uploadDesc{};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Width = requiredSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.SampleDesc.Count = 1;
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> staging;
        if (FAILED(m_device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
                                                     D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                     IID_PPV_ARGS(&staging))))
        {
            return nullptr;
        }

        // Copy rows with a vertical flip: cooked UVs are V-flipped for GL's
        // bottom-left texture origin; D3D12 textures are top-left origin, so
        // flipping the pixel rows restores the same sampled result.
        const u32 srcPitch = desc.width * getFormatBytesPerPixel(desc.format);
        void *mapped = nullptr;
        staging->Map(0, nullptr, &mapped);
        auto *dstBase = static_cast<u8 *>(mapped) + footprint.Offset;
        const auto *srcBase = static_cast<const u8 *>(initialData);
        for (u32 row = 0; row < numRows; ++row)
        {
            const u8 *srcRow = srcBase + static_cast<size_t>(numRows - 1 - row) * srcPitch;
            std::memcpy(dstBase + static_cast<size_t>(row) * footprint.Footprint.RowPitch, srcRow, srcPitch);
        }
        staging->Unmap(0, nullptr);

        m_upload_allocator->Reset();
        m_upload_list->Reset(m_upload_allocator.Get(), nullptr);

        D3D12_TEXTURE_COPY_LOCATION dstLoc{};
        dstLoc.pResource = resource.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dstLoc.SubresourceIndex = 0;
        D3D12_TEXTURE_COPY_LOCATION srcLoc{};
        srcLoc.pResource = staging.Get();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        srcLoc.PlacedFootprint = footprint;
        m_upload_list->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        if (!uploadToResource(resource.Get(), nullptr, 0, 0)) // flush + wait + PS-resource barrier
        {
            return nullptr;
        }
    }

    auto *texture = allocateResource<D3D12Texture>(desc, std::move(resource));

    // Persistent CPU-side SRV (copied into the shader-visible heap per draw)
    if (hasFlag(desc.usage, TextureUsage::Sampled))
    {
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle{};
        if (!allocatePersistentSrv(srvHandle))
        {
            LOG_ERROR(kLogD3D12, "D3D12: persistent SRV heap exhausted");
            texture->release();
            return nullptr;
        }
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = desc.depth > 1 ? D3D12_SRV_DIMENSION_TEXTURE3D : D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (desc.depth > 1)
        {
            srvDesc.Texture3D.MostDetailedMip = 0;
            srvDesc.Texture3D.MipLevels = 1;
        }
        else
        {
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
        }
        m_device->CreateShaderResourceView(texture->resource(), &srvDesc, srvHandle);
        texture->setSrvCpuHandle(srvHandle);
    }

    texture->setDevice(this);
    trackResourceCreated(texture);
    return RHITextureRef(texture);
}

// Flush the upload list and wait. If `textureForBarrier` is non-null, a
// COPY_DEST -> PIXEL_SHADER_RESOURCE transition is recorded before closing.
bool D3D12RHIDevice::uploadToResource(ID3D12Resource *textureForBarrier, const D3D12_SUBRESOURCE_DATA *, u32, u64)
{
    if (textureForBarrier)
    {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = textureForBarrier;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        m_upload_list->ResourceBarrier(1, &barrier);
    }

    if (FAILED(m_upload_list->Close()))
        return false;
    ID3D12CommandList *lists[] = {m_upload_list.Get()};
    m_queue->ExecuteCommandLists(1, lists);
    m_queue->Signal(m_upload_fence.Get(), ++m_upload_fence_value);
    m_upload_fence->SetEventOnCompletion(m_upload_fence_value, m_upload_event);
    WaitForSingleObject(m_upload_event, INFINITE);
    return true;
}

RHIShaderRef D3D12RHIDevice::createShader(const ShaderBytecode &bytecode)
{
    if (bytecode.format != ShaderBytecodeFormat::DXIL)
    {
        LOG_ERROR(kLogD3D12, "D3D12::createShader: unsupported format %u (only DXIL supported)",
                  static_cast<u32>(bytecode.format));
        return nullptr;
    }
    if (!bytecode.data || bytecode.size == 0)
    {
        LOG_ERROR(kLogD3D12, "D3D12::createShader: empty bytecode");
        return nullptr;
    }
    if (bytecode.stage != ShaderStage::Vertex && bytecode.stage != ShaderStage::Fragment)
    {
        LOG_ERROR(kLogD3D12, "D3D12::createShader: unsupported stage %u", static_cast<u32>(bytecode.stage));
        return nullptr;
    }

    auto *shader = allocateResource<D3D12Shader>(bytecode.stage, bytecode.data, bytecode.size);
    shader->setDevice(this);
    trackResourceCreated(shader);
    return RHIShaderRef(shader);
}

bool D3D12RHIDevice::reflectCBuffers(const D3D12Shader *shader, std::vector<D3D12CBufferInfo> &out)
{
    // Legacy D3DReflect (d3dcompiler_47.dll) does not understand DXIL
    // containers — reflection must go through DXC's container API
    // (requires dxcompiler.dll + dxil.dll next to the executable).
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcContainerReflection> container;
    if (FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))) ||
        FAILED(DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&container))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: DxcCreateInstance failed (dxcompiler.dll missing?)");
        return false;
    }
    ComPtr<IDxcBlobEncoding> blob;
    if (FAILED(utils->CreateBlob(shader->bytecode(), static_cast<UINT32>(shader->bytecodeSize()), DXC_CP_ACP,
                                 &blob)))
    {
        LOG_ERROR(kLogD3D12, "D3D12: DXC CreateBlob failed");
        return false;
    }
    if (FAILED(container->Load(blob.Get())))
    {
        LOG_ERROR(kLogD3D12, "D3D12: DXC container Load failed (not a DXIL container?)");
        return false;
    }

    ComPtr<ID3D12ShaderReflection> reflection;
    UINT32 partIndex = 0;
    if (FAILED(container->FindFirstPartKind(DXC_PART_REFLECTION_DATA, &partIndex)) ||
        FAILED(container->GetPartReflection(partIndex, IID_PPV_ARGS(&reflection))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: DXC GetPartReflection failed (dxil.dll missing?)");
        return false;
    }
    if (!reflection)
    {
        LOG_ERROR(kLogD3D12, "D3D12: no reflection data in shader blob");
        return false;
    }

    D3D12_SHADER_DESC shaderDesc{};
    reflection->GetDesc(&shaderDesc);
    for (u32 i = 0; i < shaderDesc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc{};
        reflection->GetResourceBindingDesc(i, &bindDesc);
        if (bindDesc.Type != D3D_SIT_CBUFFER)
            continue;

        D3D12CBufferInfo info{};
        std::strncpy(info.name, bindDesc.Name, sizeof(info.name) - 1);
        info.bindPoint = bindDesc.BindPoint;
        if (ID3D12ShaderReflectionConstantBuffer *cb = reflection->GetConstantBufferByName(bindDesc.Name))
        {
            D3D12_SHADER_BUFFER_DESC cbDesc{};
            cb->GetDesc(&cbDesc);
            info.size = cbDesc.Size;
        }
        out.push_back(info);
    }
    return true;
}

RHIPipelineStateRef D3D12RHIDevice::createPipelineState(const PipelineStateDesc &desc)
{
    // Same caching policy as the GL backend: identical descs share one PSO.
    if (RHIPipelineStateRef cached = m_pso_manager.find(desc))
        return cached;

    RHIPipelineStateRef pso = createPipelineStateUncached(desc);
    if (pso)
        m_pso_manager.insert(desc, pso);
    return pso;
}

RHIPipelineStateRef D3D12RHIDevice::createPipelineStateUncached(const PipelineStateDesc &desc)
{
    auto *vs = static_cast<D3D12Shader *>(desc.vertexShader);
    auto *ps = static_cast<D3D12Shader *>(desc.fragmentShader);
    if (!vs || !ps)
    {
        LOG_ERROR(kLogD3D12, "D3D12: PSO creation requires both vertex and pixel shaders");
        return nullptr;
    }

    // Input layout from the PSO desc's vertex attributes.
    D3D12_INPUT_ELEMENT_DESC elements[8]{};
    for (u32 i = 0; i < desc.vertexAttributeCount && i < 8; ++i)
    {
        const VertexAttributeDesc &attr = desc.vertexAttributes[i];
        elements[i].SemanticName = getSemanticName(attr.location);
        elements[i].SemanticIndex = 0;
        elements[i].Format = getAttributeFormat(attr.components);
        elements[i].InputSlot = 0;
        elements[i].AlignedByteOffset = attr.offset;
        elements[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elements[i].InstanceDataStepRate = 0;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_root_signature.Get();
    psoDesc.VS.pShaderBytecode = vs->bytecode();
    psoDesc.VS.BytecodeLength = vs->bytecodeSize();
    psoDesc.PS.pShaderBytecode = ps->bytecode();
    psoDesc.PS.BytecodeLength = ps->bytecodeSize();
    psoDesc.InputLayout.pInputElementDescs = elements;
    psoDesc.InputLayout.NumElements = desc.vertexAttributeCount;

    psoDesc.RasterizerState.FillMode = getFillMode(desc.rasterizerState.fillMode);
    psoDesc.RasterizerState.CullMode = getCullMode(desc.rasterizerState.cullMode);
    psoDesc.RasterizerState.FrontCounterClockwise = desc.rasterizerState.frontFace == FrontFace::CounterClockwise;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    psoDesc.BlendState.RenderTarget[0].BlendEnable = desc.blendState.enable ? TRUE : FALSE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = getBlendFactor(desc.blendState.srcFactor);
    psoDesc.BlendState.RenderTarget[0].DestBlend = getBlendFactor(desc.blendState.dstFactor);
    psoDesc.BlendState.RenderTarget[0].BlendOp = getBlendOp(desc.blendState.op);
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = getBlendFactor(desc.blendState.srcAlphaFactor);
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = getBlendFactor(desc.blendState.dstAlphaFactor);
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = getBlendOp(desc.blendState.alphaOp);
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState.DepthEnable = desc.depthStencilState.depthTest ? TRUE : FALSE;
    psoDesc.DepthStencilState.DepthWriteMask =
        desc.depthStencilState.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = getCompareFunc(desc.depthStencilState.depthFunc);
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    // Depth buffer is always D32_FLOAT; an Unknown desc format means
    // "no depth attachment" only when depth testing is off.
    const bool usesDepth = desc.depthStencilState.depthTest || desc.depthStencilState.depthWrite ||
                           desc.depthFormat != TextureFormat::Unknown;
    psoDesc.DSVFormat = usesDepth ? (desc.depthFormat != TextureFormat::Unknown ? getDXGIFormat(desc.depthFormat)
                                                                              : DXGI_FORMAT_D32_FLOAT)
                                  : DXGI_FORMAT_UNKNOWN;

    psoDesc.NumRenderTargets = desc.renderTargetCount;
    for (u32 i = 0; i < desc.renderTargetCount; ++i)
    {
        psoDesc.RTVFormats[i] = desc.renderTargetFormats[i] != TextureFormat::Unknown
                                    ? getDXGIFormat(desc.renderTargetFormats[i])
                                    : kBackbufferFormat;
    }

    psoDesc.PrimitiveTopologyType = getTopologyType(desc.topology);
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = UINT_MAX;

    ComPtr<ID3D12PipelineState> pso;
    if (FAILED(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateGraphicsPipelineState failed");
        return nullptr;
    }

    auto *psoObj = allocateResource<D3D12PipelineState>(desc, std::move(pso));

    // Reflect cbuffer bindings so the translator can map flattened uniform
    // names ("type_PerDraw[3]") back to (cbuffer, vec4 index) without any
    // hardcoded per-shader tables.
    std::vector<D3D12CBufferInfo> vsCBs, psCBs;
    if (!reflectCBuffers(vs, vsCBs) || !reflectCBuffers(ps, psCBs))
    {
        psoObj->release();
        return nullptr;
    }
    psoObj->setCBuffers(std::move(vsCBs), std::move(psCBs));

    psoObj->setDevice(this);
    trackResourceCreated(psoObj);
    return RHIPipelineStateRef(psoObj);
}

RHIFenceRef D3D12RHIDevice::createFence(RHIFenceValue initialValue)
{
    ComPtr<ID3D12Fence> fence;
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
    {
        LOG_ERROR(kLogD3D12, "D3D12: CreateFence failed");
        return nullptr;
    }
    auto *fenceObj = allocateResource<D3D12Fence>(m_queue.Get(), std::move(fence), initialValue);
    fenceObj->setDevice(this);
    trackResourceCreated(fenceObj);
    return RHIFenceRef(fenceObj);
}

// -- Command context / submission ---------------------------------------------

IRHICommandList *D3D12RHIDevice::createCommandList()
{
    m_deferred->buffer.reset();
    m_deferred->recorder.begin();
    return &m_deferred->recorder;
}

void D3D12RHIDevice::submit(IRHICommandList *cmdList)
{
    if (!cmdList || !m_cmd_list_open)
        return;
    cmdList->end();
    m_deferred->translator.execute(m_cmd_list.Get(), m_deferred->buffer);
}

// -- Frame fencing --------------------------------------------------------------

RHIFenceValue D3D12RHIDevice::signalFrame()
{
    const RHIFenceValue value = m_next_frame_value++;
    m_queue->Signal(m_frame_fence.Get(), value);
    return value;
}

RHIFenceValue D3D12RHIDevice::getCompletedFenceValue()
{
    return m_frame_fence ? m_frame_fence->GetCompletedValue() : 0;
}

// -- Resource lifecycle -----------------------------------------------------------

void D3D12RHIDevice::queueResourceForDelete(GPUResource *resource)
{
    if (!resource)
        return;
    // The next frame fence to be signaled is the earliest safe point; the
    // resource may still be referenced by in-flight frames, so require the
    // current frame's fence value (already signaled or about to be).
    resource->setDeletionFence(m_next_frame_value);
    PendingDelete pd;
    pd.resource = resource;
    pd.fence = m_next_frame_value;
    m_pending_deletes.pushBack(pd);
}

void D3D12RHIDevice::flushPendingDeletes()
{
    const RHIFenceValue safeFrame = getCompletedFenceValue();

    usize keep = 0;
    for (usize i = 0; i < m_pending_deletes.size(); ++i)
    {
        PendingDelete &pd = m_pending_deletes[i];
        if (pd.fence <= safeFrame)
        {
            trackResourceDestroyed(pd.resource);
            // Free the persistent SRV slot before destroying the texture.
            if (auto *tex = dynamic_cast<D3D12Texture *>(pd.resource))
            {
                if (tex->hasSrv())
                    freePersistentSrv(tex->srvCpuHandle());
            }
            pd.resource->internalDestroy();
        }
        else
        {
            if (keep != i)
            {
                m_pending_deletes[keep] = pd;
            }
            ++keep;
        }
    }

    if (keep != m_pending_deletes.size())
    {
        DynamicArray<PendingDelete> compacted;
        compacted.reserve(keep);
        for (usize i = 0; i < keep; ++i)
        {
            compacted.pushBack(m_pending_deletes[i]);
        }
        m_pending_deletes.swap(compacted);
    }
}

bool D3D12RHIDevice::allocatePersistentSrv(D3D12_CPU_DESCRIPTOR_HANDLE &out)
{
    if (m_srv_persistent_free_list.empty())
        return false;
    const u32 index = m_srv_persistent_free_list.back();
    m_srv_persistent_free_list.popBack();
    out = m_srv_persistent_heap->GetCPUDescriptorHandleForHeapStart();
    out.ptr += static_cast<SIZE_T>(index) * m_srv_descriptor_size;
    return true;
}

void D3D12RHIDevice::freePersistentSrv(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    if (!m_srv_persistent_heap)
        return;
    const SIZE_T base = m_srv_persistent_heap->GetCPUDescriptorHandleForHeapStart().ptr;
    const u32 index = static_cast<u32>((handle.ptr - base) / m_srv_descriptor_size);
    m_srv_persistent_free_list.pushBack(index);
}

// -- Memory budget ------------------------------------------------------------------

void D3D12RHIDevice::trackResourceCreated(const GPUResource *resource)
{
    if (resource)
    {
        m_tracked_memory_bytes += resource->memorySizeBytes();
    }
}

void D3D12RHIDevice::trackResourceDestroyed(const GPUResource *resource)
{
    if (resource)
    {
        const u64 size = resource->memorySizeBytes();
        m_tracked_memory_bytes = (size > m_tracked_memory_bytes) ? 0 : m_tracked_memory_bytes - size;
    }
}

RHIMemoryInfo D3D12RHIDevice::queryMemoryInfo() const
{
    RHIMemoryInfo info{};
    if (m_adapter)
    {
        DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
        if (SUCCEEDED(m_adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo)))
        {
            info.budgetBytes = memInfo.Budget;
            info.totalBytes = memInfo.Budget;
            info.availableBytes = memInfo.Budget > memInfo.CurrentUsage ? memInfo.Budget - memInfo.CurrentUsage : 0;
        }
    }
    return info;
}

// -- Translator-facing helpers --------------------------------------------------------

D3D12_CPU_DESCRIPTOR_HANDLE D3D12RHIDevice::currentRtv() const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtv_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(m_frame_index) * m_rtv_descriptor_size;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12RHIDevice::dsv() const
{
    return m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
}

D3D12RHIDevice::UploadAllocation D3D12RHIDevice::allocateUpload(u32 size)
{
    const u64 aligned = (static_cast<u64>(size) + 255) & ~static_cast<u64>(255);
    const u64 segmentEnd = (static_cast<u64>(m_frame_index) + 1) * kUploadRingSegmentSize;
    if (m_upload_ring_offset + aligned > segmentEnd)
    {
        LOG_ERROR(kLogD3D12, "D3D12: upload ring segment exhausted (%llu + %u > %llu)",
                  static_cast<unsigned long long>(m_upload_ring_offset), size,
                  static_cast<unsigned long long>(segmentEnd));
        return {};
    }
    UploadAllocation alloc;
    alloc.cpu = m_upload_ring_mapped + m_upload_ring_offset;
    alloc.gpu = m_upload_ring->GetGPUVirtualAddress() + m_upload_ring_offset;
    m_upload_ring_offset += aligned;
    return alloc;
}

bool D3D12RHIDevice::allocateSrvDescriptors(u32 count, D3D12_CPU_DESCRIPTOR_HANDLE &outCpu,
                                            D3D12_GPU_DESCRIPTOR_HANDLE &outGpu)
{
    const u32 segmentEnd = (m_frame_index + 1) * kSrvShaderSegmentSize;
    if (m_srv_shader_cursor + count > segmentEnd)
    {
        LOG_ERROR(kLogD3D12, "D3D12: shader-visible SRV heap segment exhausted");
        return false;
    }
    outCpu = m_srv_shader_heap->GetCPUDescriptorHandleForHeapStart();
    outCpu.ptr += static_cast<SIZE_T>(m_srv_shader_cursor) * m_srv_descriptor_size;
    outGpu = m_srv_shader_heap->GetGPUDescriptorHandleForHeapStart();
    outGpu.ptr += static_cast<UINT64>(m_srv_shader_cursor) * m_srv_descriptor_size;
    m_srv_shader_cursor += count;
    return true;
}

} // namespace Entelechy
