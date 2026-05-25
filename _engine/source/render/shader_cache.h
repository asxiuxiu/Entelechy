#pragma once
#include "foundation_types.h"
#include "rhi_device.h"
#include "rhi_resources.h"
#include "hash_map.h"
#include "string_id.h"

namespace Entelechy {

// ------------------------------------------------------------------
// ShaderCache
//
// Compile-time deduplication of shader modules by (stage, sourceHash).
// Phase 1: in-memory cache only. No async compilation, no DDC.
// Future: add file-system derived data cache and async build queue.
// ------------------------------------------------------------------
class ShaderCache {
public:
    ShaderCache() = default;
    ~ShaderCache() = default;

    ShaderCache(const ShaderCache&) = delete;
    ShaderCache& operator=(const ShaderCache&) = delete;
    ShaderCache(ShaderCache&&) = delete;
    ShaderCache& operator=(ShaderCache&&) = delete;

    // Get cached shader or compile new one. Returns nullptr on failure.
    RHIShaderRef getOrCreateShader(IRHIDevice* device, ShaderStage stage,
                                    const char* source, usize length);

    void clear();

private:
    struct CacheKey {
        ShaderStage stage;
        u64 sourceHash;
        bool operator==(const CacheKey& other) const {
            return stage == other.stage && sourceHash == other.sourceHash;
        }
    };
    struct CacheKeyHash {
        u64 operator()(const CacheKey& key) const {
            u64 h = static_cast<u64>(key.stage);
            h = hashCombine(h, key.sourceHash);
            return h;
        }
    };

    HashMap<CacheKey, RHIShaderRef, CacheKeyHash> m_cache;
};

} // namespace Entelechy
