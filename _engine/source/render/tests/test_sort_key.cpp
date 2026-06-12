#include "test/test_framework.h"
#include "render/queue/PhaseItem.h"

using namespace Entelechy;

TEST(SortKeyDepthEncoding, NegativeDepthClamped) {
    ASSERT_EQ(encodeLinearDepth(-10.0f, 0.1f, 100.0f), 0u);
    ASSERT_EQ(encodeLinearDepth(-0.0f, 0.1f, 100.0f), 0u);
}

TEST(SortKeyDepthEncoding, NearPlaneMapsToZero) {
    ASSERT_EQ(encodeLinearDepth(0.1f, 0.1f, 100.0f), 0u);
}

TEST(SortKeyDepthEncoding, FarPlaneMapsToMax) {
    ASSERT_EQ(encodeLinearDepth(100.0f, 0.1f, 100.0f), 0xFFFFFFFFu);
}

TEST(SortKeyDepthEncoding, BeyondRangeClamped) {
    ASSERT_EQ(encodeLinearDepth(0.0f, 0.1f, 100.0f), 0u);
    ASSERT_EQ(encodeLinearDepth(1000.0f, 0.1f, 100.0f), 0xFFFFFFFFu);
}

TEST(SortKeyDepthEncoding, MonotonicOrdering) {
    const f32 depths[] = {0.1f, 1.0f, 10.0f, 50.0f, 100.0f};
    u32 previous = 0u;
    for (usize i = 0; i < 5; ++i) {
        u32 encoded = encodeLinearDepth(depths[i], 0.1f, 100.0f);
        ASSERT_TRUE(encoded >= previous);
        previous = encoded;
    }
    ASSERT_TRUE(previous == 0xFFFFFFFFu);
}

TEST(SortKeyDepthEncoding, TransparentDescendingOrder) {
    const f32 depths[] = {0.1f, 1.0f, 10.0f, 50.0f, 100.0f};
    u32 previous = 0xFFFFFFFFu;
    for (usize i = 0; i < 5; ++i) {
        u32 encoded = encodeLinearDepth(depths[i], 0.1f, 100.0f);
        u32 inverted = ~encoded;
        ASSERT_TRUE(inverted <= previous);
        previous = inverted;
    }
    ASSERT_TRUE(previous == 0u);
}
