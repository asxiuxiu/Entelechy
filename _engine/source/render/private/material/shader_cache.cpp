#include "render/material/shader_cache.h"
#include "log/core/log_macros.h"

namespace Entelechy {

static u64 hashShaderSource(const char* source, usize length) {
    // FNV-1a hash
    u64 h = 0xcbf29ce484222325ULL;
    for (usize i = 0; i < length; ++i) {
        h ^= static_cast<u64>(static_cast<u8>(source[i]));
        h *= 0x100000001b3ULL;
    }
    return h;
}

RHIShaderRef ShaderCache::getOrCreateShader(IRHIDevice* device, ShaderStage stage,
                                             const char* source, usize length) {
    if (!device || !source || length == 0) return nullptr;

    u64 sourceHash = hashShaderSource(source, length);
    CacheKey key{stage, sourceHash};

    if (auto* existing = m_cache.find(key)) {
        return *existing;
    }

    auto shader = device->createShader(stage, source, length);
    if (shader) {
        m_cache.insert(key, shader);
    } else {
        LOG_ERROR(LogCategories::kEngine, "ShaderCache: failed to compile shader (stage=%u)", static_cast<u32>(stage));
    }
    return shader;
}

void ShaderCache::clear() {
    m_cache.clear();
}

} // namespace Entelechy
