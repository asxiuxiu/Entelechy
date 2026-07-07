#include "core/memory/debug_allocator_wrapper.h"
#include "core/allocator/allocator.h"
#include <cstring>

namespace Entelechy
{

#if defined(_DEBUG) || defined(DEBUG)

DebugAllocatorWrapper::DebugAllocatorWrapper(IAllocator *inner) : m_inner(inner) {}

void *DebugAllocatorWrapper::alignForward(void *ptr, usize align)
{
    uintptr_t p = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t mask = static_cast<uintptr_t>(align) - 1;
    p = (p + mask) & ~mask;
    return reinterpret_cast<void *>(p);
}

void *DebugAllocatorWrapper::allocate(usize size, usize align)
{
    if (size == 0)
        return nullptr;

    // Ensure alignment is at least pointer-size so our Header is naturally aligned.
    if (align < alignof(void *))
        align = alignof(void *);

    usize extra = sizeof(Header) + align + TAIL_CANARY_SIZE;
    void *raw = m_inner->allocate(size + extra, 1);
    if (!raw)
        return nullptr;

    void *user = alignForward(static_cast<u8 *>(raw) + sizeof(Header), align);
    Header *h = reinterpret_cast<Header *>(static_cast<u8 *>(user) - sizeof(Header));
    h->canary = Header::CANARY;
    h->userSize = size;
    h->rawPtr = raw;

    u8 *userBytes = static_cast<u8 *>(user);
    std::memset(userBytes, POISON_ALLOC, size);

    u64 *tail = reinterpret_cast<u64 *>(userBytes + size);
    *tail = Header::CANARY;

    m_stats.totalAllocated += size;
    if (m_stats.totalAllocated > m_stats.peakAllocated)
    {
        m_stats.peakAllocated = m_stats.totalAllocated;
    }
    m_stats.allocationCount++;
    m_stats.activeCount++;

    return user;
}

void DebugAllocatorWrapper::free(void *ptr)
{
    if (!ptr)
        return;

    Header *h = reinterpret_cast<Header *>(static_cast<u8 *>(ptr) - sizeof(Header));
    CHECK(h->canary == Header::CANARY);

    u64 *tail = reinterpret_cast<u64 *>(static_cast<u8 *>(ptr) + h->userSize);
    CHECK(*tail == Header::CANARY);

    m_stats.totalAllocated -= h->userSize;
    m_stats.activeCount--;

    std::memset(ptr, POISON_FREE, h->userSize);
    h->canary = 0; // invalidate

    m_inner->free(h->rawPtr);
}

AllocatorStats DebugAllocatorWrapper::getStats() const
{
    AllocatorStats s;
    s.totalAllocated = m_stats.totalAllocated;
    s.peakAllocated = m_stats.peakAllocated;
    s.allocationCount = m_stats.allocationCount;
    s.activeCount = m_stats.activeCount;
    return s;
}

#endif // _DEBUG || DEBUG

// Global allocator factory --------------------------------------------------

#if defined(_DEBUG) || defined(DEBUG)
static DefaultAllocatorV g_defaultAllocV;
static DebugAllocatorWrapper g_debugAlloc(&g_defaultAllocV);
IAllocator *GetGlobalAllocator()
{
    return &g_debugAlloc;
}
#else
static DefaultAllocatorV g_defaultAllocV;
IAllocator *GetGlobalAllocator()
{
    return &g_defaultAllocV;
}
#endif

} // namespace Entelechy
