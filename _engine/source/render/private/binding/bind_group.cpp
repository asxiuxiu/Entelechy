#include "render/binding/bind_group.h"
#include "render/rhi/rhi_device.h"

namespace Entelechy
{

BindGroup::~BindGroup()
{
    shutdown();
}

bool BindGroup::init(const BindGroupLayout *layout)
{
    shutdown();
    if (!layout)
        return false;

    m_layout = *layout; // copy — the group must not hold a dangling pointer
    const usize count = m_layout.entryCount();
    m_buffers.resize(count);
    m_textures.resize(count);
    m_initialized = true;
    return true;
}

void BindGroup::shutdown()
{
    m_layout = BindGroupLayout{};
    m_buffers.clear();
    m_textures.clear();
    m_initialized = false;
}

void BindGroup::setBuffer(u32 binding, RHIBuffer *buffer, u32 offset, u32 size)
{
    if (!m_initialized)
        return;
    for (usize i = 0; i < m_layout.entryCount(); ++i)
    {
        const BindGroupLayoutEntry *entry = m_layout.entryAt(i);
        if (entry && entry->binding == binding && entry->type == BindResourceType::UniformBuffer)
        {
            m_buffers[i] = {buffer, offset, size};
            return;
        }
    }
}

void BindGroup::setTexture(u32 binding, RHITexture *texture)
{
    if (!m_initialized)
        return;
    for (usize i = 0; i < m_layout.entryCount(); ++i)
    {
        const BindGroupLayoutEntry *entry = m_layout.entryAt(i);
        if (entry && entry->binding == binding && entry->type == BindResourceType::Texture)
        {
            m_textures[i] = texture;
            return;
        }
    }
}

void BindGroup::bind(IRHICommandList *cmdList) const
{
    if (!m_initialized || !cmdList)
        return;

    for (usize i = 0; i < m_layout.entryCount(); ++i)
    {
        const BindGroupLayoutEntry *entry = m_layout.entryAt(i);
        if (!entry)
            continue;

        if (entry->type == BindResourceType::UniformBuffer)
        {
            const BufferBinding &bb = m_buffers[i];
            if (bb.buffer)
                cmdList->bindConstantBuffer(entry->binding, bb.buffer, bb.offset, bb.size);
        }
        else if (entry->type == BindResourceType::Texture)
        {
            if (m_textures[i])
                cmdList->bindTexture(entry->binding, m_textures[i]);
        }
    }
}

} // namespace Entelechy
