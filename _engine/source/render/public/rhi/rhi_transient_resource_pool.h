#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include "render/rhi/rhi_types.h"
#include "render/rhi/rhi_resources.h"

namespace Entelechy
{

class IRHIDevice;

// ------------------------------------------------------------------
// Transient texture pool
//
// Pool of short-lived GPU textures whose lifecycle is known to be one
// frame (or a few frames). Acquire returns an existing texture whose
// previous GPU usage has completed; if none is available a new texture
// is created. Release marks the slot as recyclable after the fence of
// the frame in which it was last used.
//
// This is a simplified version of UE's RDG transient allocator and
// Bevy's TextureCache. It does not perform memory aliasing; it only
// reuses whole textures. Memory aliasing can be added later behind the
// same Acquire/Release interface.
// ------------------------------------------------------------------
class TransientTexturePool
{
public:
    TransientTexturePool() = default;
    ~TransientTexturePool() = default;

    // Not copyable/movable (holds device-specific resources)
    TransientTexturePool(const TransientTexturePool &) = delete;
    TransientTexturePool &operator=(const TransientTexturePool &) = delete;

    // Acquire a texture matching desc. currentFrame is the value returned
    // by IRHIDevice::signalFrame() for the frame that will use it.
    RHITextureRef acquire(IRHIDevice *device, const TextureDesc &desc, RHIFenceValue currentFrame);

    // Release a texture back to the pool. lastUseFrame is the frame fence
    // that must complete before the texture can be reused.
    void release(RHITexture *texture, const TextureDesc &desc, RHIFenceValue lastUseFrame);

    // Destroy all slots whose last-use frame has completed according to
    // device->getCompletedFenceValue(). This shrinks the pool after load
    // spikes. Call once per frame.
    void purgeCompleted(IRHIDevice *device);

    // Stats
    usize slotCount() const;

private:
    struct PoolSlot
    {
        RHITextureRef texture;
        RHIFenceValue lastUseFrame = 0;
        bool available = false;
    };

    struct PoolGroup
    {
        TextureDesc desc;
        DynamicArray<PoolSlot> slots;
    };

    static bool descMatches(const TextureDesc &a, const TextureDesc &b);

    DynamicArray<PoolGroup> m_groups;
};

} // namespace Entelechy
