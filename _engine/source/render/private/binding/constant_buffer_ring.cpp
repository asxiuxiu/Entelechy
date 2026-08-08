#include "render/binding/constant_buffer_ring.h"
#include "render/rhi/rhi_device.h"
#include "core/math/align.h"
#include "log/core/log_macros.h"
#include <cstring>

namespace Entelechy
{

namespace
{
constexpr LogCategory kLogRing("Render");
}

ConstantBufferRing::~ConstantBufferRing()
{
    shutdown();
}

bool ConstantBufferRing::init(IRHIDevice *device, u32 capacityBytes)
{
    shutdown();
    if (!device || capacityBytes == 0)
    {
        LOG_ERROR(kLogRing, "ConstantBufferRing::init: invalid arguments");
        return false;
    }

    BufferDesc desc{};
    desc.size = capacityBytes;
    desc.usage = BufferUsage::Uniform | BufferUsage::TransferSrc;
    desc.cpuAccessible = true;

    m_buffer = device->createBuffer(desc, nullptr);
    if (!m_buffer)
    {
        LOG_ERROR(kLogRing, "ConstantBufferRing::init: buffer creation failed (%u bytes)", capacityBytes);
        return false;
    }

    // Backend CPU-mapped pointer (GL: persistent map; D3D12: upload map).
    m_mapped = static_cast<u8 *>(m_buffer->getCpuMappedPointer());
    if (!m_mapped)
    {
        LOG_ERROR(kLogRing, "ConstantBufferRing::init: buffer is not CPU-mapped (backend support missing)");
        m_buffer.reset();
        return false;
    }

    m_device = device;
    m_capacity = capacityBytes;
    m_cursor = 0;
    m_overflow_logged = false;
    return true;
}

void ConstantBufferRing::shutdown()
{
    m_buffer.reset();
    m_mapped = nullptr;
    m_capacity = 0;
    m_cursor = 0;
    m_device = nullptr;
    m_overflow_logged = false;
}

bool ConstantBufferRing::allocate(const void *data, u32 size, u32 &outOffset)
{
    if (!m_mapped || size == 0)
        return false;

    const u32 alignedSize = static_cast<u32>(AlignUp(static_cast<usize>(size), 16));
    u32 offset = static_cast<u32>(AlignUp(static_cast<usize>(m_cursor), kAlignment));

    // Wrap when the current segment does not fit. Linear fill means the
    // wrapped-over region holds data from dozens of frames ago, long since
    // consumed by the GPU (see class comment).
    if (offset + alignedSize > m_capacity)
        offset = 0;

    if (offset + alignedSize > m_capacity)
    {
        if (!m_overflow_logged)
        {
            m_overflow_logged = true;
            LOG_ERROR(kLogRing, "ConstantBufferRing: allocation of %u bytes exceeds capacity %u; draw will use stale data",
                      alignedSize, m_capacity);
        }
        return false;
    }

    std::memcpy(m_mapped + offset, data, size);
    m_cursor = offset + alignedSize;
    outOffset = offset;
    return true;
}

} // namespace Entelechy
