#include "test/test_framework.h"
#include "render/rhi/gl_rhi_device.h"
#include "render/rhi/rhi_transient_resource_pool.h"
#include "render/rhi/rhi_types.h"

using namespace Entelechy;

namespace
{

// Minimal mock device that only tracks reference-count releases and fence
// values. It does not require an OpenGL context, so it can run headless.
class MockRHIDevice : public IRHIDevice
{
public:
    RHIFenceValue currentFrame = 1;
    RHIFenceValue completedFrame = 0;
    u64 trackedMemory = 0;
    DynamicArray<GPUResource *> pendingDeletes;
    DynamicArray<RHITextureRef> liveTextures;

    ~MockRHIDevice() override
    {
        // Release any live textures so their resources don't outlive the device.
        liveTextures.clear();
        for (auto *res : pendingDeletes)
        {
            res->internalDestroy();
        }
        pendingDeletes.clear();
    }

    RHIBufferRef createBuffer(const BufferDesc & /*desc*/, const void * /*initialData*/) override
    {
        return nullptr;
    }
    RHITextureRef createTexture(const TextureDesc & /*desc*/, const void * /*initialData*/) override
    {
        return nullptr;
    }
    RHIShaderRef createShader(ShaderStage /*stage*/, const void * /*bytecode*/, size_t /*size*/) override
    {
        return nullptr;
    }
    RHIPipelineStateRef createPipelineState(const PipelineStateDesc & /*desc*/) override
    {
        return nullptr;
    }
    IRHICommandList *createCommandList() override
    {
        return nullptr;
    }
    void submit(IRHICommandList * /*cmdList*/) override {}
    void present() override {}

    RHIFenceValue signalFrame() override
    {
        return currentFrame++;
    }
    RHIFenceValue getCompletedFenceValue() override
    {
        return completedFrame;
    }

    void queueResourceForDelete(GPUResource *resource) override
    {
        resource->setDeletionFence(currentFrame);
        pendingDeletes.pushBack(resource);
    }

    void flushPendingDeletes() override
    {
        usize keep = 0;
        for (usize i = 0; i < pendingDeletes.size(); ++i)
        {
            GPUResource *res = pendingDeletes[i];
            if (res->deletionFence() <= completedFrame)
            {
                trackedMemory -= res->memorySizeBytes();
                res->internalDestroy();
            }
            else
            {
                if (keep != i)
                {
                    pendingDeletes[keep] = res;
                }
                ++keep;
            }
        }
        if (keep != pendingDeletes.size())
        {
            DynamicArray<GPUResource *> compacted;
            compacted.reserve(keep);
            for (usize i = 0; i < keep; ++i)
            {
                compacted.pushBack(pendingDeletes[i]);
            }
            pendingDeletes.swap(compacted);
        }
    }

    RHIMemoryInfo queryMemoryInfo() const override
    {
        return {};
    }
    u64 getTrackedMemoryUsage() const override
    {
        return trackedMemory;
    }
    RenderBackendType getBackendType() const override
    {
        return RenderBackendType::OpenGL;
    }
};

class MockTexture : public RHITexture
{
public:
    MockTexture(u64 size, const TextureDesc &desc) : m_size(size), m_desc(desc) {}
    ~MockTexture() override = default;

    const TextureDesc &getDesc() const override
    {
        return m_desc;
    }
    u64 memorySizeBytes() const override
    {
        return m_size;
    }

protected:
    void onDestroy() override {}

private:
    u64 m_size = 0;
    TextureDesc m_desc;
};

} // namespace

TEST(GPUResourceLifecycle, ReleaseEntersDeferredDeleteQueue)
{
    MockRHIDevice device;

    TextureDesc desc{};
    desc.width = 64;
    desc.height = 64;
    desc.format = TextureFormat::RGBA8_UNORM;

    void *mem = DefaultAllocator::alloc(sizeof(MockTexture), alignof(MockTexture));
    auto *texture = new (mem) MockTexture(64 * 64 * 4, desc);
    texture->setDevice(&device);
    device.trackedMemory += texture->memorySizeBytes();

    RHITextureRef ref(texture);
    ASSERT_EQ(device.pendingDeletes.size(), 0u);
    ref.reset();
    ASSERT_EQ(device.pendingDeletes.size(), 1u);

    // GPU hasn't advanced, so flush should keep it.
    device.flushPendingDeletes();
    ASSERT_EQ(device.pendingDeletes.size(), 1u);

    // Mark the frame as completed and flush again.
    device.completedFrame = device.currentFrame;
    device.flushPendingDeletes();
    ASSERT_EQ(device.pendingDeletes.size(), 0u);
    ASSERT_EQ(device.getTrackedMemoryUsage(), 0u);
}

TEST(GPUResourceLifecycle, RefCountedHandleSharesOwnership)
{
    MockRHIDevice device;

    TextureDesc desc{};
    void *mem = DefaultAllocator::alloc(sizeof(MockTexture), alignof(MockTexture));
    auto *texture = new (mem) MockTexture(1024, desc);
    texture->setDevice(&device);
    device.trackedMemory += texture->memorySizeBytes();

    {
        RHITextureRef a(texture);
        RHITextureRef b = a;
        ASSERT_EQ(texture->refCount(), 2u);
        {
            RHITextureRef c = b;
            ASSERT_EQ(texture->refCount(), 3u);
        }
        ASSERT_EQ(texture->refCount(), 2u);
    }

    ASSERT_EQ(device.pendingDeletes.size(), 1u);
}

TEST(TransientTexturePool, AcquireReturnsSameSlotAfterFrameCompletes)
{
    MockRHIDevice device;
    TransientTexturePool pool;

    TextureDesc desc{};
    desc.width = 32;
    desc.height = 32;
    desc.format = TextureFormat::RGBA8_UNORM;

    // We need the device to actually create textures, so use a real GL device
    // for this test. The Mock device can't create textures, so we skip the
    // GPU-backed portion and verify the API shape with the mock.
    // Instead we manually inject a texture into the pool and verify release/acquire.
    void *mem = DefaultAllocator::alloc(sizeof(MockTexture), alignof(MockTexture));
    auto *tex = new (mem) MockTexture(32 * 32 * 4, desc);
    tex->setDevice(&device);

    // Simulate: pool has one available slot from frame 1, now frame 2 is current
    // and frame 1 has completed.
    device.currentFrame = 2;
    device.completedFrame = 1;

    // The pool should see the slot as available if we release it with completed frame.
    pool.release(tex, desc, /*lastUseFrame=*/1);
    ASSERT_EQ(pool.slotCount(), 1u);

    RHITextureRef acquired = pool.acquire(&device, desc, /*currentFrame=*/2);
    ASSERT_TRUE(acquired.get() == tex);
    ASSERT_EQ(pool.slotCount(), 1u);

    // Clean up: release back to pool and purge.
    pool.release(acquired.get(), desc, /*lastUseFrame=*/2);
    device.completedFrame = 2;
    pool.purgeCompleted(&device);
    ASSERT_EQ(pool.slotCount(), 0u);
}

TEST(GLMemoryTracking, TextureSizeIncludesMipsAndArrayLayers)
{
    // Use the same static helper the GL device uses.
    TextureDesc desc{};
    desc.width = 64;
    desc.height = 64;
    desc.depth = 1;
    desc.mipLevels = 2;
    desc.arrayLayers = 1;
    desc.format = TextureFormat::RGBA8_UNORM; // 4 bpp

    // mip0: 64*64*4 = 16384
    // mip1: 32*32*4 = 4096
    // total = 20480
    const u64 expected = (64 * 64 + 32 * 32) * 4;
    const u64 actual = GLRHIDevice::textureMemorySizeBytes(desc);
    ASSERT_EQ(actual, expected);
}
