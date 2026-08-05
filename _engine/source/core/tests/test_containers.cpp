#include "test/test_framework.h"
#include "core/container/hash_map.h"
#include "core/container/dynamic_array.h"

namespace
{

// RAII probe that tracks live instances; any double destruction (or a
// missed one) shows up as a non-zero balance. The payload makes double
// destruction fatal in practice (double free), not just miscounted.
struct RaiiProbe
{
    static int s_live;
    Entelechy::DynamicArray<u8> payload;

    RaiiProbe()
    {
        ++s_live;
    }
    RaiiProbe(const RaiiProbe &other) : payload(other.payload)
    {
        ++s_live;
    }
    RaiiProbe(RaiiProbe &&other) noexcept : payload(std::move(other.payload))
    {
        ++s_live;
    }
    ~RaiiProbe()
    {
        --s_live;
    }
    RaiiProbe &operator=(const RaiiProbe &) = default;
    RaiiProbe &operator=(RaiiProbe &&) = default;
};
int RaiiProbe::s_live = 0;

} // namespace

// Regression: HashMap::clear() destroyed values in place but left the
// slots for ~HashMap to destroy again -> every value present at clear()
// time was destructed twice (crashed the shutdown path via RHIRef).
TEST(Core, HashMapClearDestroysValuesExactlyOnce)
{
    RaiiProbe::s_live = 0;
    {
        Entelechy::HashMap<u32, RaiiProbe> map;
        for (u32 i = 0; i < 8; ++i)
        {
            RaiiProbe probe;
            probe.payload.pushBack(static_cast<u8>(i));
            map.insert(i, std::move(probe));
        }
        ASSERT_TRUE(map.size() == 8u);

        map.clear();
        ASSERT_EQ(map.size(), 0u);
        ASSERT_TRUE(map.find(3) == nullptr);

        // Reinsert after clear: slots must be reusable.
        RaiiProbe probe;
        map.insert(100, std::move(probe));
        ASSERT_TRUE(map.find(100) != nullptr);
    }
    // Every construction (slot defaults included) balanced by exactly
    // one destruction.
    ASSERT_EQ(RaiiProbe::s_live, 0);
}
