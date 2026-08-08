#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include "render/rhi/rhi_resources.h"
#include "render/rhi/rhi_types.h"

namespace Entelechy
{

class IRHICommandList;

// ------------------------------------------------------------------
// BindGroupLayout / BindGroup — declarative binding description (6e).
//
// A BindGroupLayout declares the binding slots of a material: the constant
// buffers (b-registers) and textures (t-registers) with their stage
// visibility. A BindGroup instance attaches concrete resources and binds
// them through the RHI:
//   - GL:  bindConstantBuffer -> glBindBufferRange(GL_UNIFORM_BUFFER, ...),
//          bindTexture -> glActiveTexture(GL_TEXTURE0 + slot) + glBindTexture
//   - D3D12: bindConstantBuffer -> root CBV (slot resolved from the PSO's
//          reflected per-stage cbuffers), bindTexture -> SRV table slot
//
// Binding points are globally unique within a material across stages (the
// GL backend shares one GL_UNIFORM_BUFFER namespace), so `binding` alone
// addresses each resource.
//
// A BindGroup owns a copy of its layout (no raw layout pointers), so it is
// safe to move together with the material that owns it.
// ------------------------------------------------------------------

enum class BindResourceType : u8
{
    UniformBuffer,
    Texture,
};

struct BindGroupLayoutEntry
{
    u32 binding = 0;
    BindResourceType type = BindResourceType::UniformBuffer;
    ShaderStage stage = ShaderStage::Vertex | ShaderStage::Fragment;
};

class BindGroupLayout
{
public:
    BindGroupLayout() = default;
    BindGroupLayout(const BindGroupLayout &) = default;
    BindGroupLayout(BindGroupLayout &&) noexcept = default;
    BindGroupLayout &operator=(const BindGroupLayout &) = default;
    BindGroupLayout &operator=(BindGroupLayout &&) noexcept = default;

    void addEntry(const BindGroupLayoutEntry &entry)
    {
        m_entries.pushBack(entry);
    }

    usize entryCount() const
    {
        return m_entries.size();
    }
    const BindGroupLayoutEntry *entryAt(usize index) const
    {
        return index < m_entries.size() ? &m_entries[index] : nullptr;
    }

private:
    DynamicArray<BindGroupLayoutEntry> m_entries;
};

class BindGroup
{
public:
    BindGroup() = default;
    BindGroup(BindGroup &&) noexcept = default;
    BindGroup &operator=(BindGroup &&) noexcept = default;
    ~BindGroup();

    BindGroup(const BindGroup &) = delete;
    BindGroup &operator=(const BindGroup &) = delete;

    // Copies the layout into the group (the group stays self-contained).
    bool init(const BindGroupLayout *layout);
    void shutdown();

    void setBuffer(u32 binding, RHIBuffer *buffer, u32 offset, u32 size);
    void setTexture(u32 binding, RHITexture *texture);

    // Emits bindConstantBuffer / bindTexture for every bound entry.
    // Requires a render pass (GL) / recorded command list (D3D12).
    void bind(IRHICommandList *cmdList) const;

private:
    struct BufferBinding
    {
        RHIBuffer *buffer = nullptr;
        u32 offset = 0;
        u32 size = 0;
    };

    BindGroupLayout m_layout;                // owned copy
    DynamicArray<BufferBinding> m_buffers;   // parallel to layout entries
    DynamicArray<RHITexture *> m_textures;   // parallel to layout entries
    bool m_initialized = false;
};

} // namespace Entelechy
