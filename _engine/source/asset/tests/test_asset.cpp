#include "test/test_framework.h"
#include "asset/handle/asset_handle.h"
#include "asset/handle/handle_table.h"
#include "asset/type/assets.h"
#include "asset/loader/asset_server.h"
#include <cstring>

// ------------------------------------------------------------------
// Test helpers
// ------------------------------------------------------------------
struct DummyAsset
{
    int value = 0;
    DummyAsset() = default;
    explicit DummyAsset(int v) : value(v) {}
};

class DummyLoader : public Entelechy::IAssetLoader<DummyAsset>
{
public:
    DummyAsset load(const Entelechy::FileData &data, const Entelechy::Path &path) override
    {
        (void)data;
        (void)path;
        return DummyAsset{42};
    }
};

// ------------------------------------------------------------------
// Handle tests
// ------------------------------------------------------------------
TEST(Asset, HandleDefaultInvalid)
{
    Entelechy::Handle<DummyAsset> h;
    ASSERT_FALSE(h.valid());
}

TEST(Asset, HandleEquality)
{
    Entelechy::Handle<DummyAsset> a{1, 2};
    Entelechy::Handle<DummyAsset> b{1, 2};
    Entelechy::Handle<DummyAsset> c{1, 3};
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a == c);
}

// ------------------------------------------------------------------
// HandleTable tests
// ------------------------------------------------------------------
TEST(Asset, HandleTableAllocateAndGet)
{
    Entelechy::HandleTable<DummyAsset> table;
    auto h = table.allocate();
    ASSERT_TRUE(h.valid());
    ASSERT_FALSE(table.isOccupied(h));
    ASSERT_TRUE(table.tryGet(h) == nullptr);

    table.fill(h, DummyAsset{100});
    auto *ptr = table.tryGet(h);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(ptr->value, 100);
}

TEST(Asset, HandleTableABAProtection)
{
    Entelechy::HandleTable<DummyAsset> table;
    auto h1 = table.allocate();
    table.fill(h1, DummyAsset{1});
    table.release(h1);

    // Old handle is now stale
    ASSERT_FALSE(table.isOccupied(h1));
    ASSERT_TRUE(table.tryGet(h1) == nullptr);

    // New allocation may reuse the same index
    auto h2 = table.allocate();
    ASSERT_TRUE(h2.index == h1.index);
    ASSERT_TRUE(h2.generation != h1.generation);
    ASSERT_FALSE(table.isOccupied(h2));
    ASSERT_TRUE(table.tryGet(h2) == nullptr);
}

TEST(Asset, HandleTableRefCount)
{
    Entelechy::HandleTable<DummyAsset> table;
    auto h = table.allocate();
    table.fill(h, DummyAsset{5});

    ASSERT_EQ(table.refCount(h), 0u);
    table.incrementRef(h);
    ASSERT_EQ(table.refCount(h), 1u);
    table.incrementRef(h);
    ASSERT_EQ(table.refCount(h), 2u);
    table.decrementRef(h);
    ASSERT_EQ(table.refCount(h), 1u);
}

TEST(Asset, HandleTableFreeListReuse)
{
    Entelechy::HandleTable<DummyAsset> table;
    auto a = table.allocate();
    auto b = table.allocate();
    auto c = table.allocate();

    table.release(b);
    auto d = table.allocate();
    // d should reuse b's index
    ASSERT_EQ(d.index, b.index);
    ASSERT_TRUE(d.generation != b.generation);
}

// ------------------------------------------------------------------
// Assets<T> tests
// ------------------------------------------------------------------
TEST(Asset, AssetsInsertGetRemove)
{
    Entelechy::Assets<DummyAsset> assets;
    auto h = assets.insert(DummyAsset{77});
    ASSERT_TRUE(h.valid());

    auto *ptr = assets.get(h);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(ptr->value, 77);

    assets.remove(h);
    ASSERT_TRUE(assets.get(h) == nullptr);
    ASSERT_EQ(assets.count(), 0u);
}

TEST(Asset, AssetsAllocateEmptyThenFill)
{
    Entelechy::Assets<DummyAsset> assets;
    auto h = assets.allocateEmpty();
    ASSERT_TRUE(h.valid());
    ASSERT_TRUE(assets.get(h) == nullptr);

    assets.fill(h, DummyAsset{99});
    auto *ptr = assets.get(h);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(ptr->value, 99);
}

// ------------------------------------------------------------------
// AssetServer sync load tests
// ------------------------------------------------------------------
TEST(Asset, AssetServerSyncLoad)
{
    Entelechy::Assets<DummyAsset> assets;
    DummyLoader loader;
    Entelechy::AssetServer server(nullptr);

    auto h = server.loadSync(Entelechy::Path("dummy.path"), loader, assets);
    ASSERT_TRUE(h.valid());

    auto *ptr = assets.get(h);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(ptr->value, 42);
}

TEST(Asset, AssetServerUnload)
{
    Entelechy::Assets<DummyAsset> assets;
    DummyLoader loader;
    Entelechy::AssetServer server(nullptr);

    auto h = server.loadSync(Entelechy::Path("dummy.path"), loader, assets);
    ASSERT_TRUE(assets.get(h) != nullptr);

    server.unload(h, assets);
    ASSERT_TRUE(assets.get(h) == nullptr);
}

// ------------------------------------------------------------------
// AssetServer async load tests
// ------------------------------------------------------------------
TEST(Asset, AssetServerAsyncLoad)
{
    Entelechy::Assets<DummyAsset> assets;
    DummyLoader loader;
    Entelechy::AssetServer server(nullptr);

    auto h = server.loadAsync(Entelechy::Path("dummy.path"), loader, assets);
    ASSERT_TRUE(h.valid());

    // Immediately after async request, data is not yet available
    ASSERT_TRUE(assets.get(h) == nullptr);

    // Poll until the load completes (with a safety timeout)
    for (int i = 0; i < 1000 && assets.get(h) == nullptr; ++i)
    {
        server.processEvents();
        std::this_thread::yield();
    }

    auto *ptr = assets.get(h);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(ptr->value, 42);
}

TEST(Asset, AssetServerReloadPreservesHandle)
{
    Entelechy::Assets<DummyAsset> assets;
    DummyLoader loader;
    Entelechy::AssetServer server(nullptr);

    auto h = server.loadSync(Entelechy::Path("dummy.path"), loader, assets);
    ASSERT_EQ(assets.get(h)->value, 42);

    // Create a loader that returns a different value
    class OtherLoader : public Entelechy::IAssetLoader<DummyAsset>
    {
    public:
        DummyAsset load(const Entelechy::FileData &data, const Entelechy::Path &path) override
        {
            (void)data;
            (void)path;
            return DummyAsset{123};
        }
    };
    OtherLoader otherLoader;

    server.reload(h, Entelechy::Path("dummy.path"), otherLoader, assets);
    auto *ptr = assets.get(h);
    ASSERT_TRUE(ptr != nullptr);
    ASSERT_EQ(ptr->value, 123);
    ASSERT_EQ(h.index, h.index); // same handle
}
