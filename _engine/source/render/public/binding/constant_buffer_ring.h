#pragma once
#include "core/foundation_types.h"
#include "render/rhi/rhi_resources.h"
#include <cstddef>

namespace Entelechy
{

class IRHIDevice;

// ------------------------------------------------------------------
// ConstantBufferRing — per-frame ring buffer for constant data (6e).
//
// A single device-side buffer (GL: persistent-mapped GL_UNIFORM_BUFFER;
// D3D12: upload-heap CBV buffer; Vulkan: host-visible buffer) that the
// upper layer fills per draw with aligned blocks. Material::bind memcpys
// each cbuffer's CPU blob into a block and records
//   bindConstantBuffer(binding, ring.buffer(), offset, size).
//
// The cursor advances linearly and only wraps when the ring is full.
// That wrap is GPU-safe as long as capacity >> in-flight frames × per-frame
// usage (8 MB default vs ~100 KB/frame for Sponza's ~400 draws, vs 2 frames
// in flight), so the overwritten region was consumed many frames ago.
// ------------------------------------------------------------------
class ConstantBufferRing
{
public:
    static constexpr u32 kDefaultCapacity = 8 * 1024 * 1024;
    // D3D12 root CBVs require 256-byte alignment; GL's
    // GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT is at most 256 per spec.
    static constexpr u32 kAlignment = 256;

    ConstantBufferRing() = default;
    ~ConstantBufferRing();

    ConstantBufferRing(const ConstantBufferRing &) = delete;
    ConstantBufferRing &operator=(const ConstantBufferRing &) = delete;

    // Creates the backing buffer and maps it for CPU writes. The device
    // must be initialized (GL context current) and outlive the ring.
    bool init(IRHIDevice *device, u32 capacityBytes = kDefaultCapacity);
    void shutdown();

    // Allocate an aligned block, memcpy `data` into it and return its byte
    // offset within the buffer. Returns false when the request exceeds the
    // ring capacity (logged once).
    bool allocate(const void *data, u32 size, u32 &outOffset);

    RHIBuffer *buffer() const
    {
        return m_buffer.get();
    }

    u32 capacity() const
    {
        return m_capacity;
    }

private:
    IRHIDevice *m_device = nullptr;
    RHIBufferRef m_buffer;
    u8 *m_mapped = nullptr;
    u32 m_capacity = 0;
    u32 m_cursor = 0;
    bool m_overflow_logged = false;
};

} // namespace Entelechy
