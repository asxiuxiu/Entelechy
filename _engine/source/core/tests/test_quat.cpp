#include "test_math_common.h"
using namespace Entelechy;
using namespace MathTest;

TEST(Quat, Identity) {
    Quat q = Quat::identity();
    ASSERT_EQ(q.x, 0.0f);
    ASSERT_EQ(q.y, 0.0f);
    ASSERT_EQ(q.z, 0.0f);
    ASSERT_EQ(q.w, 1.0f);
}

TEST(Quat, DefaultIsIdentity) {
    Quat q;
    ASSERT_TRUE(QuatNear(q, Quat::identity()));
}

TEST(Quat, FromAxisAngle) {
    Quat q = Quat::fromAxisAngle(Vec3{0.0f, 0.0f, 1.0f}, 3.14159f / 2.0f);
    Vec3 v{1.0f, 0.0f, 0.0f};
    Vec3 r = rotate(q, v);
    ASSERT_TRUE(Vec3Near(r, Vec3{0.0f, 1.0f, 0.0f}, 1e-3f));
}

TEST(Quat, RotateIdentity) {
    Vec3 v{1.0f, 2.0f, 3.0f};
    Vec3 r = rotate(Quat::identity(), v);
    ASSERT_TRUE(Vec3Near(r, v));
}

TEST(Quat, ConjugateAndInverse) {
    Quat q = Quat::fromAxisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.5f);
    Quat inv = q.inverse();
    Quat prod = q * inv;
    ASSERT_TRUE(QuatNear(prod, Quat::identity(), 1e-3f));
}

TEST(Quat, Normalized) {
    Quat q{1.0f, 2.0f, 3.0f, 4.0f};
    Quat n = q.normalized();
    ASSERT_TRUE(NearEq(n.length(), 1.0f));

    Quat z{0.0f, 0.0f, 0.0f, 0.0f};
    Quat zn = z.normalized();
    ASSERT_TRUE(QuatNear(zn, Quat::identity()));
}

TEST(Quat, NLerpEndpoints) {
    Quat a = Quat::identity();
    Quat b = Quat::fromAxisAngle(Vec3{0.0f, 0.0f, 1.0f}, 3.14159f / 2.0f);

    Quat r0 = nlerp(a, b, 0.0f);
    ASSERT_TRUE(QuatNear(r0, a, 1e-3f));

    Quat r1 = nlerp(a, b, 1.0f);
    ASSERT_TRUE(QuatNear(r1, b, 1e-3f));
}

TEST(Quat, SLerpSameQuat) {
    Quat a = Quat::fromAxisAngle(Vec3{1.0f, 0.0f, 0.0f}, 0.5f);
    Quat r = slerp(a, a, 0.5f);
    ASSERT_TRUE(QuatNear(r, a, 1e-3f));
}

TEST(Quat, SLerpNearThreshold) {
    Quat a = Quat::identity();
    Quat b = Quat::fromAxisAngle(Vec3{0.0f, 0.0f, 1.0f}, 0.001f);
    Quat r = slerp(a, b, 0.5f);
    ASSERT_TRUE(QuatNear(r, nlerp(a, b, 0.5f), 1e-3f));
}

TEST(Quat, HamiltonProduct) {
    Quat i{1.0f, 0.0f, 0.0f, 0.0f};
    Quat j{0.0f, 1.0f, 0.0f, 0.0f};
    Quat k = i * j;
    ASSERT_TRUE(QuatNear(k, Quat{0.0f, 0.0f, 1.0f, 0.0f}));
}

TEST(Quat, FromEuler) {
    // In this Z-Y-X convention, yaw controls rotation around Z axis
    Quat q = Quat::fromEuler(0.0f, 3.14159f / 2.0f, 0.0f);
    Vec3 v{1.0f, 0.0f, 0.0f};
    Vec3 r = rotate(q, v);
    ASSERT_TRUE(Vec3Near(r, Vec3{0.0f, 1.0f, 0.0f}, 1e-3f));
}
