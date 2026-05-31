#pragma once
#include "core/foundation_types.h"
#include "core/container/hash_map.h"
#include "render/rhi/rhi_types.h"
#include "render/rhi/rhi_resources.h"

namespace Entelechy {

// Forward declaration
class IRHIDevice;

// ------------------------------------------------------------------
// Pipeline State Description
//
// All fields that affect PSO compilation must be included here.
// This desc is used as the hash key for PSOManager caching.
// ------------------------------------------------------------------
struct PipelineStateDesc {
    RHIShader* vertexShader = nullptr;
    RHIShader* fragmentShader = nullptr;
    // TODO: vertex layout handle when we have a dedicated type
    BlendState blendState;
    DepthStencilState depthStencilState;
    RasterizerState rasterizerState;
    PrimitiveTopology topology = PrimitiveTopology::Triangles;
    TextureFormat renderTargetFormats[8] = {TextureFormat::RGBA8_UNORM};
    u32 renderTargetCount = 1;
    TextureFormat depthFormat = TextureFormat::Unknown;

    bool operator==(const PipelineStateDesc& other) const {
        if (vertexShader != other.vertexShader) return false;
        if (fragmentShader != other.fragmentShader) return false;
        if (topology != other.topology) return false;
        if (renderTargetCount != other.renderTargetCount) return false;
        if (depthFormat != other.depthFormat) return false;
        for (u32 i = 0; i < renderTargetCount; ++i) {
            if (renderTargetFormats[i] != other.renderTargetFormats[i]) return false;
        }
        // Compare state blocks
        if (blendState.enable != other.blendState.enable) return false;
        if (blendState.srcFactor != other.blendState.srcFactor) return false;
        if (blendState.dstFactor != other.blendState.dstFactor) return false;
        if (blendState.op != other.blendState.op) return false;
        if (blendState.srcAlphaFactor != other.blendState.srcAlphaFactor) return false;
        if (blendState.dstAlphaFactor != other.blendState.dstAlphaFactor) return false;
        if (blendState.alphaOp != other.blendState.alphaOp) return false;
        if (depthStencilState.depthTest != other.depthStencilState.depthTest) return false;
        if (depthStencilState.depthWrite != other.depthStencilState.depthWrite) return false;
        if (depthStencilState.depthFunc != other.depthStencilState.depthFunc) return false;
        if (rasterizerState.fillMode != other.rasterizerState.fillMode) return false;
        if (rasterizerState.cullMode != other.rasterizerState.cullMode) return false;
        if (rasterizerState.frontFace != other.rasterizerState.frontFace) return false;
        return true;
    }
    bool operator!=(const PipelineStateDesc& other) const { return !(*this == other); }
};

// ------------------------------------------------------------------
// Hash support for PipelineStateDesc
// ------------------------------------------------------------------
inline u64 hashCombine(u64 h, u64 v) {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

struct PipelineStateDescHash {
    u64 operator()(const PipelineStateDesc& desc) const {
        u64 h = reinterpret_cast<u64>(desc.vertexShader);
        h = hashCombine(h, reinterpret_cast<u64>(desc.fragmentShader));
        h = hashCombine(h, static_cast<u64>(desc.topology));
        h = hashCombine(h, static_cast<u64>(desc.renderTargetCount));
        for (u32 i = 0; i < desc.renderTargetCount; ++i) {
            h = hashCombine(h, static_cast<u64>(desc.renderTargetFormats[i]));
        }
        h = hashCombine(h, static_cast<u64>(desc.depthFormat));
        h = hashCombine(h, desc.blendState.enable ? 1ULL : 0ULL);
        h = hashCombine(h, static_cast<u64>(desc.blendState.srcFactor));
        h = hashCombine(h, static_cast<u64>(desc.blendState.dstFactor));
        h = hashCombine(h, static_cast<u64>(desc.depthStencilState.depthTest));
        h = hashCombine(h, static_cast<u64>(desc.depthStencilState.depthWrite));
        h = hashCombine(h, static_cast<u64>(desc.depthStencilState.depthFunc));
        h = hashCombine(h, static_cast<u64>(desc.rasterizerState.fillMode));
        h = hashCombine(h, static_cast<u64>(desc.rasterizerState.cullMode));
        h = hashCombine(h, static_cast<u64>(desc.rasterizerState.frontFace));
        return h;
    }
};

// ------------------------------------------------------------------
// PSO Manager
//
// Phase 1 (skeleton): synchronous GetOrCreate with global hash cache.
// Phase 2 (future): async compilation with placeholder fallback.
// ------------------------------------------------------------------
class PSOManager {
public:
    PSOManager() = default;
    ~PSOManager() = default;

    // PSOManager is not copyable or movable (contains HashMap)
    PSOManager(const PSOManager&) = delete;
    PSOManager& operator=(const PSOManager&) = delete;
    PSOManager(PSOManager&&) = delete;
    PSOManager& operator=(PSOManager&&) = delete;

    // Get existing PSO or create new one via device.
    // Returns nullptr if desc is invalid or creation fails.
    RHIPipelineStateRef getOrCreate(const PipelineStateDesc& desc, IRHIDevice* device);

    // Check if a PSO exists in cache.
    bool contains(const PipelineStateDesc& desc) const;

    // Clear all cached PSOs.
    void clear();

    // Stats
    usize getCacheSize() const;

private:
    HashMap<PipelineStateDesc, RHIPipelineStateRef, PipelineStateDescHash> m_cache;
};

} // namespace Entelechy
