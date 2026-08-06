#include "test_math_common.h"
using namespace Entelechy;
using namespace MathTest;

namespace
{

bool Mat3Near(const Mat3 &a, const Mat3 &b, f32 eps = 1e-4f)
{
    for (int i = 0; i < 9; ++i)
    {
        if (!NearEq(a.m[i], b.m[i], eps))
            return false;
    }
    return true;
}

} // namespace

TEST(Mat3, IdentityAndZero)
{
    Mat3 I = Mat3::identity();
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            ASSERT_EQ(I(i, j), i == j ? 1.0f : 0.0f);
        }
    }

    Mat3 Z = Mat3::zero();
    for (int i = 0; i < 9; ++i)
    {
        ASSERT_EQ(Z.m[i], 0.0f);
    }
}

TEST(Mat3, FromMat4DropsTranslation)
{
    Mat4 m = Mat4::fromTranslation(Vec3{10.0f, 20.0f, 30.0f}) * Mat4::fromScale(Vec3{2.0f, 3.0f, 4.0f});
    Mat3 upper = Mat3::fromMat4(m);
    ASSERT_EQ(upper(0, 0), 2.0f);
    ASSERT_EQ(upper(1, 1), 3.0f);
    ASSERT_EQ(upper(2, 2), 4.0f);
    ASSERT_EQ(upper(0, 1), 0.0f);
    ASSERT_EQ(upper(2, 0), 0.0f);
}

TEST(Mat3, Transpose)
{
    Mat3 a{};
    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 3; ++row)
        {
            a(row, col) = static_cast<f32>(col * 3 + row);
        }
    }
    Mat3 t = a.transpose();
    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 3; ++row)
        {
            ASSERT_EQ(t(row, col), a(col, row));
        }
    }
}

TEST(Mat3, InverseTimesOriginalIsIdentity)
{
    // Rotation (45 deg around Y) + non-uniform scale, built through Mat4.
    Quat q = Quat::fromAxisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.7853981633974483f);
    Mat4 world = Mat4::fromTRS(Vec3{5.0f, 6.0f, 7.0f}, q, Vec3{2.0f, 3.0f, 4.0f});
    Mat3 a = Mat3::fromMat4(world);

    Mat3 inv = a.inverse();
    ASSERT_TRUE(NearEq(a.determinant(), 2.0f * 3.0f * 4.0f, 1e-3f));

    // A * A^-1 == I (column-major manual multiply).
    Mat3 prod{};
    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 3; ++row)
        {
            f32 sum = 0.0f;
            for (int k = 0; k < 3; ++k)
            {
                sum += a(row, k) * inv(k, col);
            }
            prod(row, col) = sum;
        }
    }
    ASSERT_TRUE(Mat3Near(prod, Mat3::identity(), 1e-4f));
}

TEST(Mat3, SingularInverseReturnsZero)
{
    Mat3 singular = Mat3::zero();
    ASSERT_TRUE(Mat3Near(singular.inverse(), Mat3::zero()));
}

TEST(Mat3, NormalMatrixPureRotation)
{
    // Pure rotation: inverse-transpose equals the rotation itself.
    Quat q = Quat::fromAxisAngle(Vec3{0.0f, 0.0f, 1.0f}, 1.0472f);
    Mat4 world = Mat4::fromTRS(Vec3{1.0f, 2.0f, 3.0f}, q, Vec3{1.0f, 1.0f, 1.0f});
    Mat3 nm = Mat3::normalMatrix(world);
    ASSERT_TRUE(Mat3Near(nm, Mat3::fromMat4(world)));
}

TEST(Mat3, NormalMatrixUniformScale)
{
    // Uniform scale s: normal matrix = rotation / s.
    Quat q = Quat::fromAxisAngle(Vec3{1.0f, 0.0f, 0.0f}, 0.5f);
    Mat4 world = Mat4::fromTRS(Vec3{0.0f, 0.0f, 0.0f}, q, Vec3{2.0f, 2.0f, 2.0f});
    Mat3 nm = Mat3::normalMatrix(world);
    Mat3 rot = Mat3::fromMat4(Mat4::fromRotation(q));
    for (int i = 0; i < 9; ++i)
    {
        ASSERT_TRUE(NearEq(nm.m[i], rot.m[i] * 0.5f));
    }
}

TEST(Mat3, NormalMatrixKeepsPerpendicularity)
{
    // Non-uniform scale: a tangent and its normal must stay perpendicular
    // after (world * tangent) and (normalMatrix * normal).
    Mat4 world = Mat4::fromTranslation(Vec3{1.0f, 1.0f, 1.0f}) * Mat4::fromScale(Vec3{2.0f, 1.0f, 0.5f});
    Mat3 nm = Mat3::normalMatrix(world);

    Vec3 normal{0.0f, 0.0f, 1.0f};
    Vec3 tangent{1.0f, 0.0f, 0.0f};

    Vec3 worldNormal = nm.transformVector(normal).normalized();
    Vec3 worldTangent = world.transformVector(tangent).normalized();
    ASSERT_TRUE(NearEq(worldNormal.dot(worldTangent), 0.0f, 1e-4f));
}
