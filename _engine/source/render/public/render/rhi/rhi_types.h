#pragma once
#include "core/foundation_types.h"

namespace Entelechy {

// ------------------------------------------------------------------
// Backend type enumeration
// ------------------------------------------------------------------
enum class RenderBackendType : u32 {
    OpenGL,
    // Future: D3D12, Vulkan, Metal, WebGPU
};

// ------------------------------------------------------------------
// Buffer usage flags
// ------------------------------------------------------------------
enum class BufferUsage : u32 {
    None        = 0,
    Vertex      = 1 << 0,
    Index       = 1 << 1,
    Uniform     = 1 << 2,
    Storage     = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};
inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool hasFlag(BufferUsage flags, BufferUsage flag) {
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ------------------------------------------------------------------
// Texture format
// ------------------------------------------------------------------
enum class TextureFormat : u32 {
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
enum class TextureUsage : u32 {
    None        = 0,
    Sampled     = 1 << 0,
    ColorTarget = 1 << 1,
    DepthTarget = 1 << 2,
    Storage     = 1 << 3,
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};
inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline TextureUsage operator&(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<u32>(a) & static_cast<u32>(b));
}
inline bool hasFlag(TextureUsage flags, TextureUsage flag) {
    return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

// ------------------------------------------------------------------
// Shader stage
// ------------------------------------------------------------------
enum class ShaderStage : u32 {
    None     = 0,
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
    Geometry = 1 << 3,
};
inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
    return static_cast<ShaderStage>(static_cast<u32>(a) | static_cast<u32>(b));
}
inline ShaderStage operator&(ShaderStage a, ShaderStage b) {
    return static_cast<ShaderStage>(static_cast<u32>(a) & static_cast<u32>(b));
}

// ------------------------------------------------------------------
// Primitive topology
// ------------------------------------------------------------------
enum class PrimitiveTopology : u32 {
    Triangles,
    TriangleStrip,
    Lines,
    LineStrip,
    Points,
};

// ------------------------------------------------------------------
// Blend factor
// ------------------------------------------------------------------
enum class BlendFactor : u32 {
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
enum class BlendOp : u32 {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

// ------------------------------------------------------------------
// Compare function
// ------------------------------------------------------------------
enum class CompareFunc : u32 {
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
enum class CullMode : u32 {
    None,
    Front,
    Back,
};

// ------------------------------------------------------------------
// Front face winding
// ------------------------------------------------------------------
enum class FrontFace : u32 {
    CounterClockwise,
    Clockwise,
};

// ------------------------------------------------------------------
// Fill mode
// ------------------------------------------------------------------
enum class FillMode : u32 {
    Solid,
    Wireframe,
};

// ------------------------------------------------------------------
// Vertex attribute description (for buffer creation)
// ------------------------------------------------------------------
struct VertexAttributeDesc {
    u32 location = 0;
    u32 components = 3;      // 1, 2, 3, or 4
    bool normalized = false;
    u32 offset = 0;
};

// ------------------------------------------------------------------
// Resource descriptors
// ------------------------------------------------------------------
struct BufferDesc {
    u32 size = 0;                     // Bytes
    BufferUsage usage = BufferUsage::None;
    bool cpuAccessible = false;

    // Vertex layout info (only relevant when usage includes Vertex)
    u32 vertexStride = 0;
    const VertexAttributeDesc* vertexAttributes = nullptr;
    u32 vertexAttributeCount = 0;
};

struct TextureDesc {
    u32 width = 1;
    u32 height = 1;
    u32 depth = 1;        // For 3D textures
    u32 mipLevels = 1;
    u32 arrayLayers = 1;
    TextureFormat format = TextureFormat::RGBA8_UNORM;
    TextureUsage usage = TextureUsage::None;
};

struct BlendState {
    bool enable = false;
    BlendFactor srcFactor = BlendFactor::One;
    BlendFactor dstFactor = BlendFactor::Zero;
    BlendOp op = BlendOp::Add;
    BlendFactor srcAlphaFactor = BlendFactor::One;
    BlendFactor dstAlphaFactor = BlendFactor::Zero;
    BlendOp alphaOp = BlendOp::Add;
};

struct DepthStencilState {
    bool depthTest = false;
    bool depthWrite = false;
    CompareFunc depthFunc = CompareFunc::Less;
};

struct RasterizerState {
    FillMode fillMode = FillMode::Solid;
    CullMode cullMode = CullMode::Back;
    FrontFace frontFace = FrontFace::CounterClockwise;
};

// ------------------------------------------------------------------
// Render pass description (simplified)
// ------------------------------------------------------------------
struct RenderPassColorAttachment {
    bool clear = true;
    f32 clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
};

struct RenderPassDesc {
    RenderPassColorAttachment colorAttachments[8];
    u32 colorAttachmentCount = 1;
    // TODO: depth attachment
};

// ------------------------------------------------------------------
// Resource barrier types (simplified)
// ------------------------------------------------------------------
enum class ResourceBarrierType : u32 {
    UndefinedToRenderTarget,
    RenderTargetToPresent,
    RenderTargetToShaderResource,
    ShaderResourceToRenderTarget,
};

struct BarrierDesc {
    ResourceBarrierType type;
};

} // namespace Entelechy
