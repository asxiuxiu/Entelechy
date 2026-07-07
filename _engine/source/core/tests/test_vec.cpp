#include "test_math_common.h"
using namespace Entelechy;
using namespace MathTest;

TEST(Vec, Vec2Arithmetic)
{
    Vec2 a{1.0f, 2.0f};
    Vec2 b{3.0f, 4.0f};
    Vec2 c = a + b;
    ASSERT_EQ(c.x, 4.0f);
    ASSERT_EQ(c.y, 6.0f);

    c = a - b;
    ASSERT_EQ(c.x, -2.0f);
    ASSERT_EQ(c.y, -2.0f);

    c = a * 2.0f;
    ASSERT_EQ(c.x, 2.0f);
    ASSERT_EQ(c.y, 4.0f);

    c = 3.0f * a;
    ASSERT_EQ(c.x, 3.0f);
    ASSERT_EQ(c.y, 6.0f);

    c = a * b;
    ASSERT_EQ(c.x, 3.0f);
    ASSERT_EQ(c.y, 8.0f);

    c = a / 2.0f;
    ASSERT_EQ(c.x, 0.5f);
    ASSERT_EQ(c.y, 1.0f);

    c = -a;
    ASSERT_EQ(c.x, -1.0f);
    ASSERT_EQ(c.y, -2.0f);
}

TEST(Vec, Vec2DotAndLength)
{
    Vec2 a{3.0f, 4.0f};
    ASSERT_EQ(a.dot(a), 25.0f);
    ASSERT_EQ(a.lengthSq(), 25.0f);
    ASSERT_TRUE(NearEq(a.length(), 5.0f));
}

TEST(Vec, Vec2Normalized)
{
    Vec2 a{3.0f, 4.0f};
    Vec2 n = a.normalized();
    ASSERT_TRUE(NearEq(n.length(), 1.0f));
    ASSERT_TRUE(NearEq(n.x, 0.6f));
    ASSERT_TRUE(NearEq(n.y, 0.8f));

    Vec2 z{0.0f, 0.0f};
    Vec2 zn = z.normalized();
    ASSERT_EQ(zn.x, 0.0f);
    ASSERT_EQ(zn.y, 0.0f);
}

TEST(Vec, Vec3Arithmetic)
{
    Vec3 a{1.0f, 2.0f, 3.0f};
    Vec3 b{4.0f, 5.0f, 6.0f};
    Vec3 c = a + b;
    ASSERT_EQ(c.x, 5.0f);
    ASSERT_EQ(c.y, 7.0f);
    ASSERT_EQ(c.z, 9.0f);

    c = a * 2.0f;
    ASSERT_EQ(c.x, 2.0f);
    ASSERT_EQ(c.y, 4.0f);
    ASSERT_EQ(c.z, 6.0f);
}

TEST(Vec, Vec3DotCross)
{
    Vec3 i{1.0f, 0.0f, 0.0f};
    Vec3 j{0.0f, 1.0f, 0.0f};
    Vec3 k{0.0f, 0.0f, 1.0f};

    ASSERT_EQ(i.dot(j), 0.0f);
    ASSERT_EQ(i.dot(i), 1.0f);

    Vec3 r = i.cross(j);
    ASSERT_TRUE(Vec3Near(r, k));

    r = j.cross(k);
    ASSERT_TRUE(Vec3Near(r, i));

    r = k.cross(i);
    ASSERT_TRUE(Vec3Near(r, j));

    Vec3 a{1.0f, 2.0f, 3.0f};
    Vec3 b{4.0f, 5.0f, 6.0f};
    ASSERT_TRUE(Vec3Near(a.cross(b), -(b.cross(a))));
}

TEST(Vec, Vec3Normalized)
{
    Vec3 a{1.0f, 2.0f, 2.0f};
    Vec3 n = a.normalized();
    ASSERT_TRUE(NearEq(n.length(), 1.0f));

    Vec3 z{0.0f, 0.0f, 0.0f};
    Vec3 zn = z.normalized();
    ASSERT_EQ(zn.x, 0.0f);
    ASSERT_EQ(zn.y, 0.0f);
    ASSERT_EQ(zn.z, 0.0f);
}

TEST(Vec, Vec3Index)
{
    Vec3 a{1.0f, 2.0f, 3.0f};
    ASSERT_EQ(a[0], 1.0f);
    ASSERT_EQ(a[1], 2.0f);
    ASSERT_EQ(a[2], 3.0f);

    a[1] = 5.0f;
    ASSERT_EQ(a[1], 5.0f);
}

TEST(Vec, Vec4Arithmetic)
{
    Vec4 a{1.0f, 2.0f, 3.0f, 4.0f};
    Vec4 b{2.0f, 3.0f, 4.0f, 5.0f};
    Vec4 c = a + b;
    ASSERT_EQ(c.x, 3.0f);
    ASSERT_EQ(c.y, 5.0f);
    ASSERT_EQ(c.z, 7.0f);
    ASSERT_EQ(c.w, 9.0f);

    c = a * 2.0f;
    ASSERT_EQ(c.x, 2.0f);
    ASSERT_EQ(c.y, 4.0f);
    ASSERT_EQ(c.z, 6.0f);
    ASSERT_EQ(c.w, 8.0f);
}

TEST(Vec, Vec4Normalized)
{
    Vec4 a{1.0f, 1.0f, 1.0f, 1.0f};
    Vec4 n = a.normalized();
    ASSERT_TRUE(NearEq(n.length(), 1.0f));

    Vec4 z{0.0f, 0.0f, 0.0f, 0.0f};
    Vec4 zn = z.normalized();
    ASSERT_EQ(zn.x, 0.0f);
    ASSERT_EQ(zn.y, 0.0f);
    ASSERT_EQ(zn.z, 0.0f);
    ASSERT_EQ(zn.w, 0.0f);
}
