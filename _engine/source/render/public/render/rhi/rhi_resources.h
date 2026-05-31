#pragma once
#include "core/foundation_types.h"
#include "render/rhi/rhi_types.h"
#include <atomic>

namespace Entelechy {

// ------------------------------------------------------------------
// GPUResource base class with reference counting
//
// Design note: Absorbs UE's FRHIResource ref-counting pattern.
// Release() does not immediately destroy; instead the resource enters
// a deferred-delete queue managed by the RHI device (future Phase 2).
// For Phase 1 (single-threaded, immediate GL), deletion is immediate.
// ------------------------------------------------------------------
class GPUResource {
public:
    virtual ~GPUResource() = default;

    void addRef() {
        m_ref_count.fetch_add(1, std::memory_order_relaxed);
    }

    void release() {
        if (m_ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            onDestroy();
            delete this;
        }
    }

    u32 refCount() const {
        return m_ref_count.load(std::memory_order_relaxed);
    }

protected:
    virtual void onDestroy() {}

private:
    mutable std::atomic<u32> m_ref_count{1};
};

// ------------------------------------------------------------------
// Typed smart handle (simplified TRefCountPtr)
// ------------------------------------------------------------------
template<typename T>
class RHIRef {
public:
    RHIRef() = default;
    RHIRef(std::nullptr_t) {}

    explicit RHIRef(T* ptr) : m_ptr(ptr) {}

    RHIRef(const RHIRef& other) : m_ptr(other.m_ptr) {
        if (m_ptr) m_ptr->addRef();
    }

    RHIRef(RHIRef&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_ptr = nullptr;
    }

    ~RHIRef() {
        if (m_ptr) m_ptr->release();
    }

    RHIRef& operator=(const RHIRef& other) {
        if (this != &other) {
            if (m_ptr) m_ptr->release();
            m_ptr = other.m_ptr;
            if (m_ptr) m_ptr->addRef();
        }
        return *this;
    }

    RHIRef& operator=(RHIRef&& other) noexcept {
        if (this != &other) {
            if (m_ptr) m_ptr->release();
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;
        }
        return *this;
    }

    T* get() const { return m_ptr; }
    T* operator->() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    explicit operator bool() const { return m_ptr != nullptr; }

    void reset() {
        if (m_ptr) {
            m_ptr->release();
            m_ptr = nullptr;
        }
    }

    T* detach() {
        T* p = m_ptr;
        m_ptr = nullptr;
        return p;
    }

private:
    T* m_ptr = nullptr;
};

// ------------------------------------------------------------------
// Concrete resource types (opaque to RHI consumers)
// ------------------------------------------------------------------
class RHIBuffer : public GPUResource {
public:
    virtual u32 getSize() const = 0;
    virtual BufferUsage getUsage() const = 0;
};

class RHITexture : public GPUResource {
public:
    virtual const TextureDesc& getDesc() const = 0;
};

class RHIShader : public GPUResource {
public:
    virtual ShaderStage getStage() const = 0;
};

class RHIPipelineState : public GPUResource {
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
