#pragma once
#include "core/foundation_types.h"

namespace Entelechy
{

// Forward declaration — BarrierDesc references GPUResource but the full
// definition lives in rhi_resources.h. We only need a pointer here.
class GPUResource;

// ------------------------------------------------------------------
// Backend type enumeration
// ------------------------------------------------------------------
enum class RenderBackendType : u32
{
    OpenGL,
    D3D12,
    Vulkan,
};

// ------------------------------------------------------------------
// Buffer usage flags
// ------------------------------------------------------------------
enum class BufferUsage : u32
{
    None = 0,
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};
inline BufferUsage operator|(BufferUsage a, BufferUsage b)
{
    return static_cast<BufferUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline BufferUsage operator&(BufferUsage a, BufferUsage b)
{
    return static_cast<BufferUsage>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool hasFlag(BufferUsage flags, BufferUsage flag)
{
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ------------------------------------------------------------------
// Texture format
// ------------------------------------------------------------------
enum class TextureFormat : u32
{
    Unknown,
    R8_UNORM,
    RG8_UNORM,
    RGBA8_UNORM,
    RGBA8_SRGB,
    BGRA8_UNORM,
    R16_FLOAT,
    RG16_FLOAT,
    RGBA16_FLOAT,
    R32_FLOAT,
    RG32_FLOAT,
    RGBA32_FLOAT,
    D24_UNORM_S8_UINT,
    D32_FLOAT,
};

// ------------------------------------------------------------------
// Texture usage flags
// ------------------------------------------------------------------
enum class TextureUsage : u32
{
    None = 0,
    Sampled = 1 << 0,
    ColorTarget = 1 << 1,
    DepthTarget = 1 << 2,
    Storage = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};
inline TextureUsage operator|(TextureUsage a, TextureUsage b)
{
    return static_cast<TextureUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline TextureUsage operator&(TextureUsage a, TextureUsage b)
{
    return static_cast<TextureUsage>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool hasFlag(TextureUsage flags, TextureUsage flag)
{
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ------------------------------------------------------------------
// Shader stage
// ------------------------------------------------------------------
enum class ShaderStage : u32
{
    None = 0,
    Vertex = 1 << 0,
    Fragment = 1 << 1,
    Compute = 1 << 2,
    Geometry = 1 << 3,
};
inline ShaderStage operator|(ShaderStage a, ShaderStage b)
{
    return static_cast<ShaderStage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline ShaderStage operator&(ShaderStage a, ShaderStage b)
{
    return static_cast<ShaderStage>(static_cast<u32>(a) & static_cast<u32>(b));
}

// ------------------------------------------------------------------
// Primitive topology
// ------------------------------------------------------------------
enum class PrimitiveTopology : u32
{
    Triangles,
    TriangleStrip,
    Lines,
    LineStrip,
    Points,
};

// ------------------------------------------------------------------
// Blend factor
// ------------------------------------------------------------------
enum class BlendFactor : u32
{
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstColor,
    OneMinusDstColor,
    DstAlpha,
    OneMinusDstAlpha,
};

// ------------------------------------------------------------------
// Blend operation
// ------------------------------------------------------------------
enum class BlendOp : u32
{
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

// ------------------------------------------------------------------
// Compare function
// ------------------------------------------------------------------
enum class CompareFunc : u32
{
    Never,
    Less,
    Equal,
    LessEqual,
    Greater,
    NotEqual,
    GreaterEqual,
    Always,
};

// ------------------------------------------------------------------
// Cull mode
// ------------------------------------------------------------------
enum class CullMode : u32
{
    None,
    Front,
    Back,
};

// ------------------------------------------------------------------
// Front face winding
// ------------------------------------------------------------------
enum class FrontFace : u32
{
    CounterClockwise,
    Clockwise,
};

// ------------------------------------------------------------------
// Fill mode
// ------------------------------------------------------------------
enum class FillMode : u32
{
    Solid,
    Wireframe,
};

// ------------------------------------------------------------------
// Vertex attribute description (for buffer creation)
// ------------------------------------------------------------------
struct VertexAttributeDesc
{
    u32 location = 0;
    u32 components = 3; // 1, 2, 3, or 4
    bool normalized = false;
    u32 offset = 0;
};

// ------------------------------------------------------------------
// Resource descriptors
// ------------------------------------------------------------------
struct BufferDesc
{
    u32 size = 0; // Bytes
    BufferUsage usage = BufferUsage::None;
    bool cpuAccessible = false;

    // Vertex layout info (only relevant when usage includes Vertex)
    u32 vertexStride = 0;
    const VertexAttributeDesc *vertexAttributes = nullptr;
    u32 vertexAttributeCount = 0;
};

struct TextureDesc
{
    u32 width = 1;
    u32 height = 1;
    u32 depth = 1; // For 3D textures
    u32 mipLevels = 1;
    u32 arrayLayers = 1;
    TextureFormat format = TextureFormat::RGBA8_UNORM;
    TextureUsage usage = TextureUsage::None;
};

struct BlendState
{
    bool enable = false;
    BlendFactor srcFactor = BlendFactor::One;
    BlendFactor dstFactor = BlendFactor::Zero;
    BlendOp op = BlendOp::Add;
    BlendFactor srcAlphaFactor = BlendFactor::One;
    BlendFactor dstAlphaFactor = BlendFactor::Zero;
    BlendOp alphaOp = BlendOp::Add;
};

struct DepthStencilState
{
    bool depthTest = false;
    bool depthWrite = false;
    CompareFunc depthFunc = CompareFunc::Less;
};

struct RasterizerState
{
    FillMode fillMode = FillMode::Solid;
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
};

// ------------------------------------------------------------------
// Render pass description (simplified)
// ------------------------------------------------------------------
struct RenderPassColorAttachment
{
    bool clear = true;
    f32 clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct RenderPassDesc
{
    RenderPassColorAttachment colorAttachments[8];
    u32 colorAttachmentCount = 1;
    // TODO: depth attachment
};

// ------------------------------------------------------------------
// Shader bytecode format (multi-backend)
// ------------------------------------------------------------------
enum class ShaderBytecodeFormat : u32
{
    GLSL,  // OpenGL: source text
    SPIRV, // Vulkan: SPIR-V binary
    DXIL,  // D3D12: DXIL binary (SM 6.0+)
};

// ------------------------------------------------------------------
// Shader bytecode descriptor
// ------------------------------------------------------------------
struct ShaderBytecode
{
    ShaderStage stage = ShaderStage::None;
    ShaderBytecodeFormat format = ShaderBytecodeFormat::GLSL;
    const void *data = nullptr;
    size_t size = 0;
    const char *entryPoint = "main";
};

// ------------------------------------------------------------------
// Resource state flags (for barrier transitions)
//
// Bitmask covering all states needed across D3D12 / Vulkan / OpenGL.
// OpenGL backend treats barriers as no-op (driver manages state).
// ------------------------------------------------------------------
enum class ResourceState : u32
{
    Common = 0,
    VertexBuffer = 1 << 0,
    IndexBuffer = 1 << 1,
    ConstantBuffer = 1 << 2,
    ShaderResource = 1 << 3,
    UnorderedAccess = 1 << 4,
    RenderTarget = 1 << 5,
    DepthWrite = 1 << 6,
    DepthRead = 1 << 7,
    CopySource = 1 << 8,
    CopyDest = 1 << 9,
    Present = 1 << 10,
};
inline ResourceState operator|(ResourceState a, ResourceState b)
{
    return static_cast<ResourceState>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline ResourceState operator&(ResourceState a, ResourceState b)
{
    return static_cast<ResourceState>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool hasFlag(ResourceState flags, ResourceState flag)
{
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ------------------------------------------------------------------
// Clear flags (bitmask)
// ------------------------------------------------------------------
enum class ClearFlags : u32
{
    None = 0,
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
};
inline ClearFlags operator|(ClearFlags a, ClearFlags b)
{
    return static_cast<ClearFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline ClearFlags operator&(ClearFlags a, ClearFlags b)
{
    return static_cast<ClearFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool hasFlag(ClearFlags flags, ClearFlags flag)
{
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ------------------------------------------------------------------
// Surface descriptor (window/surface association for device init)
// ------------------------------------------------------------------
struct SurfaceDesc
{
    void *nativeWindow = nullptr; // GLFWwindow* for GL, HWND for D3D12
    u32 width = 0;
    u32 height = 0;
    bool vsync = true;
};

// ------------------------------------------------------------------
// Resource barrier descriptor (multi-backend)
//
// Describes a state transition for a specific GPU resource. Each
// backend translates this to its native barrier API:
//   - D3D12: D3D12_RESOURCE_BARRIER
//   - Vulkan: VkImageMemoryBarrier / VkBufferMemoryBarrier
//   - OpenGL: no-op (driver manages state implicitly)
// ------------------------------------------------------------------
struct BarrierDesc
{
    GPUResource *resource = nullptr;
    ResourceState beforeState = ResourceState::Common;
    ResourceState afterState = ResourceState::Common;
    u32 mipLevel = ~0u;    // ~0u = all mip levels
    u32 arrayLayer = ~0u;  // ~0u = all layers
};

// ------------------------------------------------------------------
// Fence value alias
// ------------------------------------------------------------------
using RHIFenceValue = u64;

// ------------------------------------------------------------------
// GPU memory information (best-effort per backend)
// ------------------------------------------------------------------
struct RHIMemoryInfo
{
    u64 totalBytes = 0;     // Dedicated + shared GPU memory, 0 if unknown
    u64 availableBytes = 0; // Free GPU memory, 0 if unknown
    u64 budgetBytes = 0;    // OS/driver budget, 0 if unknown
};

// ------------------------------------------------------------------
// Unified error codes for cross-backend diagnostics
// ------------------------------------------------------------------
enum class RHIErrorCode : u32
{
    Success,
    InvalidArgument,
    OutOfMemory,
    DeviceLost,
    ShaderCompilationFailed,
    UnsupportedFeature,
};

} // namespace Entelechy
