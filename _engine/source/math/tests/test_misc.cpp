#include "test_math_common.h"
using namespace Entelechy;
using namespace MathTest;

TEST(Random, Deterministic) {
    RandomXORShift32 rng1(12345);
    RandomXORShift32 rng2(12345);
    for (int i = 0; i < 10; ++i) {
        ASSERT_EQ(rng1.nextUint32(), rng2.nextUint32());
    }
}

TEST(Random, SeedZeroFallback) {
    RandomXORShift32 rng(0);
    u32 v1 = rng.nextUint32();
    u32 v2 = rng.nextUint32();
    ASSERT_TRUE(v1 != 0 || v2 != 0);
}

TEST(Random, Float01Range) {
    RandomXORShift32 rng(42);
    for (int i = 0; i < 100; ++i) {
        f32 v = rng.nextFloat01();
        ASSERT_TRUE(v >= 0.0f);
        ASSERT_TRUE(v < 1.0f);
    }
}

TEST(Random, FloatRange) {
    RandomXORShift32 rng(42);
    for (int i = 0; i < 100; ++i) {
        f32 v = rng.nextFloatRange(-5.0f, 5.0f);
        ASSERT_TRUE(v >= -5.0f);
        ASSERT_TRUE(v < 5.0f);
    }
}

TEST(Align, AlignUp) {
    ASSERT_EQ(AlignUp(0, 16), 0u);
    ASSERT_EQ(AlignUp(1, 16), 16u);
    ASSERT_EQ(AlignUp(15, 16), 16u);
    ASSERT_EQ(AlignUp(16, 16), 16u);
    ASSERT_EQ(AlignUp(17, 16), 32u);
    ASSERT_EQ(AlignUp(256, 256), 256u);
    ASSERT_EQ(AlignUp(257, 256), 512u);
}

TEST(Half, Zero) {
    Float16 h = Float16::FromFloat32(0.0f);
    f32 r = h.ToFloat32();
    ASSERT_EQ(r, 0.0f);
}

TEST(Half, One) {
    Float16 h = Float16::FromFloat32(1.0f);
    f32 r = h.ToFloat32();
    ASSERT_TRUE(NearEq(r, 1.0f, 1e-3f));
}

TEST(Half, MinusOne) {
    Float16 h = Float16::FromFloat32(-1.0f);
    f32 r = h.ToFloat32();
    ASSERT_TRUE(NearEq(r, -1.0f, 1e-3f));
}

TEST(Half, RoundTrip) {
    f32 values[] = {0.0f, 1.0f, -1.0f, 2.0f, 0.5f, -0.5f, 100.0f, -100.0f};
    for (f32 v : values) {
        Float16 h = Float16::FromFloat32(v);
        f32 r = h.ToFloat32();
        ASSERT_TRUE(NearEq(r, v, 1e-2f));
    }
}

TEST(Half, Inf) {
    Float16 h = Float16::FromFloat32(1e6f);
    f32 r = h.ToFloat32();
    ASSERT_TRUE(std::isinf(r));
}

TEST(SIMD, BatchVec4Add) {
    alignas(16) f32 a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    alignas(16) f32 b[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    alignas(16) f32 out[8] = {0};

    BatchVec4Add(a, b, out, 8);
    for (int i = 0; i < 8; ++i) {
        ASSERT_EQ(out[i], a[i] + b[i]);
    }
}

TEST(SIMD, BatchVec4AddPartial) {
    alignas(16) f32 a[5] = {1, 2, 3, 4, 5};
    alignas(16) f32 b[5] = {10, 20, 30, 40, 50};
    alignas(16) f32 out[5] = {0};

    BatchVec4Add(a, b, out, 5);
    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(out[i], a[i] + b[i]);
    }
}

TEST(SIMD, BatchVec4AddZero) {
    alignas(16) f32 a[4] = {1, 2, 3, 4};
    alignas(16) f32 b[4] = {5, 6, 7, 8};
    alignas(16) f32 out[4] = {0};

    BatchVec4Add(a, b, out, 0);
    for (int i = 0; i < 4; ++i) {
        ASSERT_EQ(out[i], 0.0f);
    }
}
