#include "test/test_framework.h"
#include "render/binding/bind_group.h"
#include "render/rhi/render_command_buffer.h"
#include "render/rhi/deferred_command_list.h"
#include "core/math/align.h"

using namespace Entelechy;

namespace
{

// Minimal RHIBuffer/RHITexture doubles for binding tests. BindGroup stores
// raw pointers only — the test owns these and deletes them directly (never
// through release(), so no device is involved).
class TestBuffer : public RHIBuffer
{
public:
    explicit TestBuffer(u32 size) : m_size(size) {}
    u32 getSize() const override
    {
        return m_size;
    }
    BufferUsage getUsage() const override
    {
        return BufferUsage::Uniform;
    }

private:
    u32 m_size = 0;
};

class TestTexture : public RHITexture
{
public:
    const TextureDesc &getDesc() const override
    {
        static const TextureDesc desc{};
        return desc;
    }
};

struct CommandRecord
{
    RenderCommandType type;
    const u8 *payload = nullptr;
    u32 payloadSize = 0;
};

DynamicArray<CommandRecord> collectCommands(const RenderCommandBuffer &buffer)
{
    DynamicArray<CommandRecord> records;
    const u8 *ptr = buffer.data();
    const u8 *end = ptr + buffer.size();

    while (ptr < end)
    {
        const auto *header = reinterpret_cast<const RenderCmdHeader *>(ptr);
        CommandRecord rec;
        rec.type = header->type;
        rec.payload = ptr + sizeof(RenderCmdHeader);
        rec.payloadSize = header->payloadSize;
        records.pushBack(rec);

        const usize stride = sizeof(RenderCmdHeader) + AlignUp(header->payloadSize, alignof(std::max_align_t));
        ptr += stride;
    }
    return records;
}

} // namespace

// ------------------------------------------------------------------
// Test: layout entry storage
// ------------------------------------------------------------------
TEST(BindGroup, LayoutEntries)
{
    BindGroupLayout layout;
    layout.addEntry({0, BindResourceType::UniformBuffer, ShaderStage::Fragment});
    layout.addEntry({1, BindResourceType::UniformBuffer, ShaderStage::Fragment});
    layout.addEntry({2, BindResourceType::UniformBuffer, ShaderStage::Vertex});
    layout.addEntry({0, BindResourceType::Texture, ShaderStage::Fragment});

    ASSERT_EQ(layout.entryCount(), 4u);
    const BindGroupLayoutEntry *e0 = layout.entryAt(0);
    ASSERT_TRUE(e0 != nullptr);
    ASSERT_EQ(e0->binding, 0u);
    ASSERT_EQ(static_cast<u32>(e0->type), static_cast<u32>(BindResourceType::UniformBuffer));
    ASSERT_EQ(static_cast<u32>(e0->stage), static_cast<u32>(ShaderStage::Fragment));
    ASSERT_EQ(layout.entryAt(3)->type, BindResourceType::Texture);
    ASSERT_TRUE(layout.entryAt(4) == nullptr);
}

// ------------------------------------------------------------------
// Test: bind() emits bindConstantBuffer + bindTexture commands
// ------------------------------------------------------------------
TEST(BindGroup, BindEmitsCommands)
{
    BindGroupLayout layout;
    layout.addEntry({0, BindResourceType::UniformBuffer, ShaderStage::Fragment});
    layout.addEntry({1, BindResourceType::UniformBuffer, ShaderStage::Fragment});
    layout.addEntry({0, BindResourceType::Texture, ShaderStage::Fragment});

    BindGroup group;
    ASSERT_TRUE(group.init(&layout));

    TestBuffer buffer(1024);
    TestBuffer materialBuffer(64);
    TestTexture texture;

    group.setBuffer(0, &buffer, 256, 48);
    group.setBuffer(1, &materialBuffer, 0, 32);
    group.setTexture(0, &texture);

    RenderCommandBuffer cmdBuffer(4096);
    DeferredCommandList cmdList(cmdBuffer);
    cmdList.begin();
    group.bind(&cmdList);
    cmdList.end();

    auto records = collectCommands(cmdBuffer);
    ASSERT_EQ(records.size(), 3u);

    // Entry 0: uniform buffer at binding 0
    ASSERT_EQ(static_cast<u32>(records[0].type), static_cast<u32>(RenderCommandType::BindConstantBuffer));
    const auto *cb0 = reinterpret_cast<const CmdBindConstantBuffer *>(records[0].payload);
    ASSERT_EQ(cb0->binding, 0u);
    ASSERT_EQ(cb0->buffer, &buffer);
    ASSERT_EQ(cb0->offset, 256u);
    ASSERT_EQ(cb0->size, 48u);

    // Entry 1: uniform buffer at binding 1
    ASSERT_EQ(static_cast<u32>(records[1].type), static_cast<u32>(RenderCommandType::BindConstantBuffer));
    const auto *cb1 = reinterpret_cast<const CmdBindConstantBuffer *>(records[1].payload);
    ASSERT_EQ(cb1->binding, 1u);
    ASSERT_EQ(cb1->buffer, &materialBuffer);
    ASSERT_EQ(cb1->offset, 0u);
    ASSERT_EQ(cb1->size, 32u);

    // Entry 2: texture at binding 0
    ASSERT_EQ(static_cast<u32>(records[2].type), static_cast<u32>(RenderCommandType::BindTexture));
    const auto *tex = reinterpret_cast<const CmdBindTexture *>(records[2].payload);
    ASSERT_EQ(tex->slot, 0u);
    ASSERT_EQ(tex->texture, &texture);
}

// ------------------------------------------------------------------
// Test: updating a buffer binding changes the emitted payload
// ------------------------------------------------------------------
TEST(BindGroup, UpdateBufferOffset)
{
    BindGroupLayout layout;
    layout.addEntry({0, BindResourceType::UniformBuffer, ShaderStage::Vertex});

    BindGroup group;
    ASSERT_TRUE(group.init(&layout));

    TestBuffer buffer(4096);
    group.setBuffer(0, &buffer, 128, 64);

    RenderCommandBuffer cmdBuffer(4096);
    DeferredCommandList cmdList(cmdBuffer);
    cmdList.begin();
    group.bind(&cmdList);
    cmdList.end();

    auto records = collectCommands(cmdBuffer);
    ASSERT_EQ(records.size(), 1u);
    const auto *cb = reinterpret_cast<const CmdBindConstantBuffer *>(records[0].payload);
    ASSERT_EQ(cb->offset, 128u);

    // Re-point the same binding at a new offset and rebind.
    group.setBuffer(0, &buffer, 512, 64);
    cmdBuffer.reset();
    cmdList.begin();
    group.bind(&cmdList);
    cmdList.end();

    records = collectCommands(cmdBuffer);
    ASSERT_EQ(records.size(), 1u);
    cb = reinterpret_cast<const CmdBindConstantBuffer *>(records[0].payload);
    ASSERT_EQ(cb->offset, 512u);
}

// ------------------------------------------------------------------
// Test: entries without a bound resource emit no command
// ------------------------------------------------------------------
TEST(BindGroup, UnboundEntrySkipped)
{
    BindGroupLayout layout;
    layout.addEntry({0, BindResourceType::UniformBuffer, ShaderStage::Fragment});
    layout.addEntry({1, BindResourceType::UniformBuffer, ShaderStage::Fragment});

    BindGroup group;
    ASSERT_TRUE(group.init(&layout));

    // Only binding 0 gets a resource; binding 1 stays unbound.
    TestBuffer buffer(256);
    group.setBuffer(0, &buffer, 0, 32);

    RenderCommandBuffer cmdBuffer(4096);
    DeferredCommandList cmdList(cmdBuffer);
    cmdList.begin();
    group.bind(&cmdList);
    cmdList.end();

    auto records = collectCommands(cmdBuffer);
    ASSERT_EQ(records.size(), 1u);
    const auto *cb = reinterpret_cast<const CmdBindConstantBuffer *>(records[0].payload);
    ASSERT_EQ(cb->binding, 0u);
}
