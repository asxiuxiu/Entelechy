#pragma once
#include "core/foundation_types.h"
#include "core/string/string.h"
#include "core/allocator/allocator.h"
#include "render/rhi/rhi_types.h"
#include <atomic>
#include <memory>

namespace Entelechy
{

// Forward declaration
class IRHIDevice;

// ------------------------------------------------------------------
// GPUResource base class with reference counting
//
// Design note: Absorbs UE's FRHIResource ref-counting pattern.
// Release() does not immediately destroy; instead the resource enters
// a deferred-delete queue managed by the owning RHI device. The device
// waits until the GPU has finished all commands referencing the resource
// (tracked via fences) before calling internalDestroy().
// ------------------------------------------------------------------
class GPUResource
{
public:
    virtual ~GPUResource() = default;

    void addRef()
    {
        m_ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    // Release a reference. When the last reference is dropped the resource
    // is handed back to its owning IRHIDevice for deferred deletion.
    // If no device is set (e.g. emergency fallback) it is destroyed immediately.
    void release();

    u32 refCount() const
    {
        return m_ref_count.load(std::memory_order_relaxed);
    }

    // Owning device. Set by the backend when the resource is created.
    void setDevice(IRHIDevice *device)
    {
        m_device = device;
    }
    IRHIDevice *device() const
    {
        return m_device;
    }

    // Fence value recorded when the resource was queued for deletion.
    // Used by the device to decide when it is safe to free.
    void setDeletionFence(RHIFenceValue fence)
    {
        m_deletion_fence = fence;
    }
    RHIFenceValue deletionFence() const
    {
        return m_deletion_fence;
    }

    // Estimated GPU memory footprint in bytes. Backends override this
    // so the budget tracker can account for the resource.
    virtual u64 memorySizeBytes() const
    {
        return 0;
    }

    // Debug name for GPU debugging tools (RenderDoc, Nsight, PIX).
    // Backends map this to platform-specific object labeling.
    virtual void setDebugName(const String & /*name*/) {}

    // Internal: actually destroy the resource. Only the owning RHI device
    // may call this after confirming the GPU is no longer using it.
    void internalDestroy()
    {
        this->~GPUResource();
        DefaultAllocator::free(this);
    }

protected:
    virtual void onDestroy() {}

private:
    mutable std::atomic<u32> m_ref_count{1};
    IRHIDevice *m_device = nullptr;
    RHIFenceValue m_deletion_fence = 0;
};

// ------------------------------------------------------------------
// Typed smart handle (simplified TRefCountPtr)
// ------------------------------------------------------------------
template <typename T>
class RHIRef
{
public:
    RHIRef() = default;
    RHIRef(std::nullptr_t) {}

    explicit RHIRef(T *ptr) : m_ptr(ptr) {}

    RHIRef(const RHIRef &other) : m_ptr(other.m_ptr)
    {
        if (m_ptr)
            m_ptr->addRef();
    }

    RHIRef(RHIRef &&other) noexcept : m_ptr(other.m_ptr)
    {
        other.m_ptr = nullptr;
    }

    ~RHIRef()
    {
        if (m_ptr)
            m_ptr->release();
    }

    RHIRef &operator=(const RHIRef &other)
    {
        if (this != &other)
        {
            if (m_ptr)
                m_ptr->release();
            m_ptr = other.m_ptr;
            if (m_ptr)
                m_ptr->addRef();
        }
        return *this;
    }

    RHIRef &operator=(RHIRef &&other) noexcept
    {
        if (this != &other)
        {
            if (m_ptr)
                m_ptr->release();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T *get() const
    {
        return m_ptr;
    }
    T *operator->() const
    {
        return m_ptr;
    }
    T &operator*() const
    {
        return *m_ptr;
    }
    explicit operator bool() const
    {
        return m_ptr != nullptr;
    }

    void reset()
    {
        if (m_ptr)
        {
            m_ptr->release();
            m_ptr = nullptr;
        }
    }

    T *detach()
    {
        T *p = m_ptr;
        m_ptr = nullptr;
        return p;
    }

private:
    T *m_ptr = nullptr;
};

// ------------------------------------------------------------------
// Concrete resource types (opaque to RHI consumers)
// ------------------------------------------------------------------
class RHIBuffer : public GPUResource
{
public:
    virtual u32 getSize() const = 0;
    virtual BufferUsage getUsage() const = 0;
};

class RHITexture : public GPUResource
{
public:
    virtual const TextureDesc &getDesc() const = 0;
};

class RHIShader : public GPUResource
{
public:
    virtual ShaderStage getStage() const = 0;
};

class RHIPipelineState : public GPUResource
{
    // PSO is opaque; desc queried via concrete backend type if needed
};

// ------------------------------------------------------------------
// Type aliases
// ------------------------------------------------------------------
using RHIBufferRef = RHIRef<RHIBuffer>;
using RHITextureRef = RHIRef<RHITexture>;
using RHIShaderRef = RHIRef<RHIShader>;
using RHIPipelineStateRef = RHIRef<RHIPipelineState>;

} // namespace Entelechy
