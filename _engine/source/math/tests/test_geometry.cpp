#include "test_math_common.h"
using namespace Entelechy;
using namespace MathTest;

TEST(AABB, FromCenterExtent) {
    AABB box = AABB::fromCenterExtent(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 2.0f, 3.0f});
    ASSERT_TRUE(Vec3Near(box.min, Vec3{-1.0f, -2.0f, -3.0f}));
    ASSERT_TRUE(Vec3Near(box.max, Vec3{1.0f, 2.0f, 3.0f}));
}

TEST(AABB, Contains) {
    AABB box = AABB::fromCenterExtent(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    ASSERT_TRUE(box.contains(Vec3{0.0f, 0.0f, 0.0f}));
    ASSERT_TRUE(box.contains(Vec3{1.0f, 1.0f, 1.0f}));
    ASSERT_TRUE(box.contains(Vec3{-1.0f, -1.0f, -1.0f}));
    ASSERT_FALSE(box.contains(Vec3{2.0f, 0.0f, 0.0f}));
}

TEST(AABB, Intersects) {
    AABB a = AABB::fromCenterExtent(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    AABB b = AABB::fromCenterExtent(Vec3{1.5f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    ASSERT_TRUE(a.intersects(b));

    AABB c = AABB::fromCenterExtent(Vec3{3.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    ASSERT_FALSE(a.intersects(c));
}

TEST(AABB, Expand) {
    AABB box = AABB::fromMinMax(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    box.expand(Vec3{2.0f, -1.0f, 0.5f});
    ASSERT_TRUE(Vec3Near(box.min, Vec3{0.0f, -1.0f, 0.0f}));
    ASSERT_TRUE(Vec3Near(box.max, Vec3{2.0f, 1.0f, 1.0f}));
}

TEST(Ray, IntersectAABBHit) {
    Ray ray{Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}.normalized()};
    AABB box = AABB::fromCenterExtent(Vec3{5.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    f32 t = 0.0f;
    ASSERT_TRUE(ray.intersectAABB(box, t));
    ASSERT_TRUE(NearEq(t, 4.0f, 1e-3f));
}

TEST(Ray, IntersectAABBMiss) {
    Ray ray{Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}.normalized()};
    AABB box = AABB::fromCenterExtent(Vec3{5.0f, 5.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    f32 t = 0.0f;
    ASSERT_FALSE(ray.intersectAABB(box, t));
}

TEST(Ray, IntersectAABBInside) {
    Ray ray{Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 0.0f, 0.0f}.normalized()};
    AABB box = AABB::fromCenterExtent(Vec3{0.0f, 0.0f, 0.0f}, Vec3{5.0f, 5.0f, 5.0f});
    f32 t = 0.0f;
    ASSERT_TRUE(ray.intersectAABB(box, t));
    ASSERT_EQ(t, 0.0f);
}

TEST(Ray, IntersectAABBAxisParallel) {
    Ray ray{Vec3{10.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}.normalized()};
    AABB box = AABB::fromCenterExtent(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    f32 t = 0.0f;
    ASSERT_FALSE(ray.intersectAABB(box, t));
}

TEST(Frustum, FromPerspectiveLookAt) {
    Mat4 P = Mat4::perspective(3.14159f / 4.0f, 1.0f, 0.1f, 100.0f);
    Mat4 V = Mat4::lookAt(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f});
    Mat4 VP = P * V;
    Frustum f = Frustum::fromMatrix(VP);

    AABB inside = AABB::fromCenterExtent(Vec3{0.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    ASSERT_TRUE(f.intersectsAABB(inside));

    AABB outside = AABB::fromCenterExtent(Vec3{100.0f, 0.0f, 0.0f}, Vec3{1.0f, 1.0f, 1.0f});
    ASSERT_FALSE(f.intersectsAABB(outside));
}
