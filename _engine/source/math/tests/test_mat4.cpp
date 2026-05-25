#include "test_math_common.h"
using namespace Entelechy;
using namespace MathTest;

TEST(Mat4, IdentityAndZero) {
    Mat4 I = Mat4::identity();
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i == j) ASSERT_EQ(I(i, j), 1.0f);
            else ASSERT_EQ(I(i, j), 0.0f);
        }
    }

    Mat4 Z = Mat4::zero();
    for (int i = 0; i < 16; ++i) {
        ASSERT_EQ(Z.m[i], 0.0f);
    }
}

TEST(Mat4, MultiplyIdentity) {
    Mat4 I = Mat4::identity();
    Mat4 A = Mat4::fromTranslation(Vec3{1.0f, 2.0f, 3.0f});
    ASSERT_TRUE(Mat4Near(I * A, A));
    ASSERT_TRUE(Mat4Near(A * I, A));
}

TEST(Mat4, Translation) {
    Mat4 T = Mat4::fromTranslation(Vec3{1.0f, 2.0f, 3.0f});
    Vec3 p{0.0f, 0.0f, 0.0f};
    Vec3 r = T.transformPoint(p);
    ASSERT_TRUE(Vec3Near(r, Vec3{1.0f, 2.0f, 3.0f}));
}

TEST(Mat4, Scale) {
    Mat4 S = Mat4::fromScale(Vec3{2.0f, 3.0f, 4.0f});
    Vec3 p{1.0f, 1.0f, 1.0f};
    Vec3 r = S.transformPoint(p);
    ASSERT_TRUE(Vec3Near(r, Vec3{2.0f, 3.0f, 4.0f}));
}

TEST(Mat4, TransformPointVsVector) {
    Mat4 T = Mat4::fromTranslation(Vec3{5.0f, 0.0f, 0.0f});
    Vec3 v{1.0f, 0.0f, 0.0f};
    Vec3 pt = T.transformPoint(v);
    Vec3 vec = T.transformVector(v);
    ASSERT_TRUE(Vec3Near(pt, Vec3{6.0f, 0.0f, 0.0f}));
    ASSERT_TRUE(Vec3Near(vec, Vec3{1.0f, 0.0f, 0.0f}));
}

TEST(Mat4, InverseIdentity) {
    Mat4 I = Mat4::identity();
    ASSERT_TRUE(Mat4Near(I.inverse(), I));
}

TEST(Mat4, InverseMultiply) {
    Mat4 T = Mat4::fromTranslation(Vec3{1.0f, 2.0f, 3.0f});
    Mat4 invT = T.inverse();
    Mat4 R = T * invT;
    ASSERT_TRUE(Mat4Near(R, Mat4::identity(), 1e-3f));
}

TEST(Mat4, InverseSingular) {
    Mat4 S = Mat4::zero();
    ASSERT_TRUE(Mat4Near(S.inverse(), Mat4::zero()));
}

TEST(Mat4, Transpose) {
    Mat4 A = Mat4::fromTranslation(Vec3{1.0f, 2.0f, 3.0f});
    ASSERT_TRUE(Mat4Near(A.transpose().transpose(), A));
}

TEST(Mat4, LookAt) {
    Vec3 eye{0.0f, 0.0f, 5.0f};
    Vec3 center{0.0f, 0.0f, 0.0f};
    Vec3 up{0.0f, 1.0f, 0.0f};
    Mat4 V = Mat4::lookAt(eye, center, up);
    Vec3 worldOrigin = V.transformPoint(Vec3{0.0f, 0.0f, 0.0f});
    ASSERT_TRUE(std::isfinite(worldOrigin.x));
    ASSERT_TRUE(std::isfinite(worldOrigin.y));
    ASSERT_TRUE(std::isfinite(worldOrigin.z));
}

TEST(Mat4, Perspective) {
    Mat4 P = Mat4::perspective(3.14159f / 4.0f, 16.0f / 9.0f, 0.1f, 100.0f);
    ASSERT_TRUE(Mat4Near(P, Mat4::identity()) == false);
    ASSERT_TRUE(Mat4Near(P, Mat4::zero()) == false);
}

TEST(Mat4, Ortho) {
    Mat4 O = Mat4::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
    Vec3 p{0.0f, 0.0f, -0.1f};
    Vec3 r = O.transformPoint(p);
    ASSERT_TRUE(Vec3Near(r, Vec3{0.0f, 0.0f, 0.0f}, 1e-3f));
}

TEST(Mat4, FromRotationQuat) {
    Quat q = Quat::fromAxisAngle(Vec3{0.0f, 0.0f, 1.0f}, 3.14159f / 2.0f);
    Mat4 R = Mat4::fromRotation(q);
    Vec3 v{1.0f, 0.0f, 0.0f};
    Vec3 r = R.transformVector(v);
    ASSERT_TRUE(Vec3Near(r, Vec3{0.0f, 1.0f, 0.0f}, 1e-3f));
}

TEST(Mat4, TRS) {
    Vec3 t{1.0f, 2.0f, 3.0f};
    Quat r = Quat::identity();
    Vec3 s{2.0f, 2.0f, 2.0f};
    Mat4 M = Mat4::fromTRS(t, r, s);
    Vec3 p{1.0f, 0.0f, 0.0f};
    Vec3 result = M.transformPoint(p);
    ASSERT_TRUE(Vec3Near(result, Vec3{3.0f, 2.0f, 3.0f}, 1e-3f));
}

TEST(Mat4, Determinant) {
    Mat4 I = Mat4::identity();
    ASSERT_EQ(I.determinant(), 1.0f);

    Mat4 S = Mat4::fromScale(Vec3{2.0f, 3.0f, 4.0f});
    ASSERT_TRUE(NearEq(S.determinant(), 24.0f, 1e-3f));
}
