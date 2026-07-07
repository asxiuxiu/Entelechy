#include "render/rhi/rhi_transient_resource_pool.h"
#include "render/rhi/rhi_device.h"
#include "log/core/log_macros.h"

namespace Entelechy
{

bool TransientTexturePool::descMatches(const TextureDesc &a, const TextureDesc &b)
{
    return a.width == b.width && a.height == b.height && a.depth == b.depth && a.mipLevels == b.mipLevels &&
           a.arrayLayers == b.arrayLayers && a.format == b.format && a.usage == b.usage;
}

RHITextureRef TransientTexturePool::acquire(IRHIDevice *device, const TextureDesc &desc, RHIFenceValue currentFrame)
{
    if (!device)
        return nullptr;

    const RHIFenceValue safe_frame = device->getCompletedFenceValue();

    // Find an existing group matching desc.
    for (auto &group : m_groups)
    {
        if (!descMatches(group.desc, desc))
            continue;

        // Return the first available slot whose previous GPU usage is done.
        for (auto &slot : group.slots)
        {
            if (slot.available && slot.lastUseFrame <= safe_frame)
            {
                slot.available = false;
                return slot.texture;
            }
        }
    }

    // No reusable slot: create a new texture and add it to the right group.
    RHITextureRef texture = device->createTexture(desc, nullptr);
    if (!texture)
        return nullptr;

    PoolSlot slot;
    slot.texture = texture;
    slot.available = false;

    for (auto &group : m_groups)
    {
        if (descMatches(group.desc, desc))
        {
            group.slots.pushBack(slot);
            return texture;
        }
    }

    PoolGroup new_group;
    new_group.desc = desc;
    new_group.slots.pushBack(slot);
    m_groups.pushBack(std::move(new_group));
    return texture;
}

void TransientTexturePool::release(RHITexture *texture, const TextureDesc &desc, RHIFenceValue lastUseFrame)
{
    if (!texture)
        return;

    for (auto &group : m_groups)
    {
        if (!descMatches(group.desc, desc))
            continue;
        for (auto &slot : group.slots)
        {
            if (slot.texture.get() == texture)
            {
                slot.available = true;
                slot.lastUseFrame = lastUseFrame;
                return;
            }
        }
    }

    // Texture not currently tracked by the pool: adopt it as a new available slot.
    PoolSlot slot;
    slot.texture = RHITextureRef(texture);
    slot.available = true;
    slot.lastUseFrame = lastUseFrame;

    for (auto &group : m_groups)
    {
        if (descMatches(group.desc, desc))
        {
            group.slots.pushBack(std::move(slot));
            return;
        }
    }

    PoolGroup new_group;
    new_group.desc = desc;
    new_group.slots.pushBack(std::move(slot));
    m_groups.pushBack(std::move(new_group));
}

void TransientTexturePool::purgeCompleted(IRHIDevice *device)
{
    if (!device)
        return;

    const RHIFenceValue safe_frame = device->getCompletedFenceValue();

    for (auto &group : m_groups)
    {
        usize keep = 0;
        for (usize i = 0; i < group.slots.size(); ++i)
        {
            PoolSlot &slot = group.slots[i];
            if (slot.available && slot.lastUseFrame <= safe_frame)
            {
                // Destroy the RHIRef; the texture will enter the device's
                // deferred-delete queue and be freed once GPU-safe.
                slot.texture.reset();
            }
            else
            {
                if (keep != i)
                {
                    group.slots[keep] = std::move(slot);
                }
                ++keep;
            }
        }

        if (keep != group.slots.size())
        {
            DynamicArray<PoolSlot> compacted;
            compacted.reserve(keep);
            for (usize i = 0; i < keep; ++i)
            {
                compacted.pushBack(std::move(group.slots[i]));
            }
            group.slots.swap(compacted);
        }
    }

    // Remove empty groups.
    usize keep = 0;
    for (usize i = 0; i < m_groups.size(); ++i)
    {
        if (!m_groups[i].slots.empty())
        {
            if (keep != i)
            {
                m_groups[keep] = std::move(m_groups[i]);
            }
            ++keep;
        }
    }
    if (keep != m_groups.size())
    {
        DynamicArray<PoolGroup> compacted;
        compacted.reserve(keep);
        for (usize i = 0; i < keep; ++i)
        {
            compacted.pushBack(std::move(m_groups[i]));
        }
        m_groups.swap(compacted);
    }
}

usize TransientTexturePool::slotCount() const
{
    usize count = 0;
    for (const auto &group : m_groups)
    {
        count += group.slots.size();
    }
    return count;
}

} // namespace Entelechy
