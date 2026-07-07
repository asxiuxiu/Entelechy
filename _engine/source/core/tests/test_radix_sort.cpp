#include "test/test_framework.h"
#include "core/algorithm/radix_sort.h"
#include "core/container/dynamic_array.h"

using namespace Entelechy;

namespace
{

struct Item
{
    u64 key;
    u32 value;
};

} // namespace

TEST(RadixSort64, EmptyIsNoOp)
{
    DynamicArray<Item> items;
    DynamicArray<Item> scratch;
    scratch.resize(items.size());
    radixSort64(items.data(), items.size(), [](const Item &item) -> u64 { return item.key; }, scratch.data());
    ASSERT_EQ(items.size(), 0u);
}

TEST(RadixSort64, SingleElement)
{
    DynamicArray<Item> items;
    items.pushBack({42u, 1u});

    DynamicArray<Item> scratch;
    scratch.resize(items.size());
    radixSort64(items.data(), items.size(), [](const Item &item) -> u64 { return item.key; }, scratch.data());

    ASSERT_EQ(items[0].key, 42u);
    ASSERT_EQ(items[0].value, 1u);
}

TEST(RadixSort64, SortsAscending)
{
    DynamicArray<Item> items;
    items.pushBack({5u, 0u});
    items.pushBack({3u, 1u});
    items.pushBack({9u, 2u});
    items.pushBack({1u, 3u});
    items.pushBack({4u, 4u});

    DynamicArray<Item> scratch;
    scratch.resize(items.size());
    radixSort64(items.data(), items.size(), [](const Item &item) -> u64 { return item.key; }, scratch.data());

    ASSERT_EQ(items[0].key, 1u);
    ASSERT_EQ(items[1].key, 3u);
    ASSERT_EQ(items[2].key, 4u);
    ASSERT_EQ(items[3].key, 5u);
    ASSERT_EQ(items[4].key, 9u);
}

TEST(RadixSort64, StableForDuplicateKeys)
{
    DynamicArray<Item> items;
    items.pushBack({100u, 0u});
    items.pushBack({50u, 1u});
    items.pushBack({100u, 2u});
    items.pushBack({50u, 3u});
    items.pushBack({100u, 4u});

    DynamicArray<Item> scratch;
    scratch.resize(items.size());
    radixSort64(items.data(), items.size(), [](const Item &item) -> u64 { return item.key; }, scratch.data());

    ASSERT_EQ(items[0].key, 50u);
    ASSERT_EQ(items[0].value, 1u);
    ASSERT_EQ(items[1].key, 50u);
    ASSERT_EQ(items[1].value, 3u);
    ASSERT_EQ(items[2].key, 100u);
    ASSERT_EQ(items[2].value, 0u);
    ASSERT_EQ(items[3].key, 100u);
    ASSERT_EQ(items[3].value, 2u);
    ASSERT_EQ(items[4].key, 100u);
    ASSERT_EQ(items[4].value, 4u);
}

TEST(RadixSort64, AllByteLanesExercised)
{
    DynamicArray<Item> items;
    items.pushBack({0x00000000000000FFu, 0u});
    items.pushBack({0x000000000000FF00u, 1u});
    items.pushBack({0x0000000000FF0000u, 2u});
    items.pushBack({0x00000000FF000000u, 3u});
    items.pushBack({0x000000FF00000000u, 4u});
    items.pushBack({0x0000FF0000000000u, 5u});
    items.pushBack({0x00FF000000000000u, 6u});
    items.pushBack({0xFF00000000000000u, 7u});

    DynamicArray<Item> scratch;
    scratch.resize(items.size());
    radixSort64(items.data(), items.size(), [](const Item &item) -> u64 { return item.key; }, scratch.data());

    for (usize i = 0; i < items.size(); ++i)
    {
        ASSERT_EQ(items[i].value, static_cast<u32>(i));
    }
}
