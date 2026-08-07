// D3D12 backend smoke tests (6d).
//
// These exercise device creation and basic resource paths without a
// window/swapchain. They require a hardware D3D12 adapter; on machines
// without one createRHIDevice returns nullptr and the tests skip.
#include "test/test_framework.h"
#include "render/rhi/rhi_device_factory.h"
#include "render/rhi/rhi_device.h"
#include "log/core/log_macros.h"
#include <cstring>

using namespace Entelechy;

namespace
{
constexpr u32 kStride = 48; // interleaved mesh vertex stride (pos/nrm/uv/tan)
}

TEST(D3D12Backend, DeviceCreation)
{
    auto device = createRHIDevice(RenderBackendType::D3D12);
    if (!device)
    {
        LOG_WARN(LogCategories::kEngine, "D3D12 backend unavailable on this machine, skipping");
        return;
    }
    ASSERT_TRUE(device->getBackendType() == RenderBackendType::D3D12);
}

TEST(D3D12Backend, BufferCreation)
{
    auto device = createRHIDevice(RenderBackendType::D3D12);
    if (!device)
        return;

    const f32 vertices[] = {0.0f, 1.0f, 2.0f, 3.0f};
    BufferDesc desc{};
    desc.size = sizeof(vertices);
    desc.usage = BufferUsage::Vertex;
    desc.vertexStride = kStride;
    RHIBufferRef buffer = device->createBuffer(desc, vertices);
    ASSERT_TRUE(buffer.get() != nullptr);
    ASSERT_EQ(buffer->getSize(), static_cast<u32>(sizeof(vertices)));

    const u32 indices[] = {0, 1, 2};
    BufferDesc ibDesc{};
    ibDesc.size = sizeof(indices);
    ibDesc.usage = BufferUsage::Index;
    RHIBufferRef ib = device->createBuffer(ibDesc, indices);
    ASSERT_TRUE(ib.get() != nullptr);
}

TEST(D3D12Backend, TextureCreation)
{
    auto device = createRHIDevice(RenderBackendType::D3D12);
    if (!device)
        return;

    const u8 pixels[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    TextureDesc desc{};
    desc.width = 2;
    desc.height = 2;
    desc.format = TextureFormat::RGBA8_UNORM;
    desc.usage = TextureUsage::Sampled;
    RHITextureRef texture = device->createTexture(desc, pixels);
    ASSERT_TRUE(texture.get() != nullptr);
    ASSERT_EQ(texture->getDesc().width, 2u);
}

TEST(D3D12Backend, FenceSignalWait)
{
    auto device = createRHIDevice(RenderBackendType::D3D12);
    if (!device)
        return;

    RHIFenceRef fence = device->createFence();
    ASSERT_TRUE(fence.get() != nullptr);
    fence->signal(1);
    ASSERT_TRUE(fence->wait(1, 5000000000ull)); // 5 s timeout
    ASSERT_TRUE(fence->isSignaled(1));
    ASSERT_TRUE(fence->getCompletedValue() >= 1);
}

TEST(D3D12Backend, MemoryInfoQuery)
{
    auto device = createRHIDevice(RenderBackendType::D3D12);
    if (!device)
        return;

    RHIMemoryInfo info = device->queryMemoryInfo();
    ASSERT_TRUE(info.budgetBytes > 0);

    // Tracked usage grows when resources are created.
    const u64 before = device->getTrackedMemoryUsage();
    BufferDesc desc{};
    desc.size = 4096;
    desc.usage = BufferUsage::Vertex;
    RHIBufferRef buffer = device->createBuffer(desc, nullptr);
    ASSERT_TRUE(device->getTrackedMemoryUsage() >= before + 4096);
}
