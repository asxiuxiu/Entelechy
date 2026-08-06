#pragma once
#include "core/math/mat4.h"
#include "core/math/math_config.h"
#include <cmath>

namespace Entelechy
{

struct Mat3
{
    f32 m[9]; // column-major: m[col * 3 + row]

    [[nodiscard]] static Mat3 identity()
    {
        Mat3 out{};
        out.m[0] = out.m[4] = out.m[8] = 1.0f;
        return out;
    }

    [[nodiscard]] static Mat3 zero()
    {
        return Mat3{};
    }

    f32 operator()(int row, int col) const
    {
        return m[col * 3 + row];
    }
    f32 &operator()(int row, int col)
    {
        return m[col * 3 + row];
    }

    // Upper-left 3x3 of a 4x4 (drops translation).
    [[nodiscard]] static Mat3 fromMat4(const Mat4 &mat)
    {
        Mat3 out{};
        for (int col = 0; col < 3; ++col)
        {
            for (int row = 0; row < 3; ++row)
            {
                out(row, col) = mat(row, col);
            }
        }
        return out;
    }

    [[nodiscard]] Vec3 transformVector(const Vec3 &v) const
    {
        return {m[0] * v.x + m[3] * v.y + m[6] * v.z, m[1] * v.x + m[4] * v.y + m[7] * v.z,
                m[2] * v.x + m[5] * v.y + m[8] * v.z};
    }

    [[nodiscard]] Mat3 transpose() const
    {
        Mat3 r{};
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                r(i, j) = (*this)(j, i);
            }
        }
        return r;
    }

    [[nodiscard]] f32 determinant() const
    {
        const f32 a00 = (*this)(0, 0), a01 = (*this)(0, 1), a02 = (*this)(0, 2);
        const f32 a10 = (*this)(1, 0), a11 = (*this)(1, 1), a12 = (*this)(1, 2);
        const f32 a20 = (*this)(2, 0), a21 = (*this)(2, 1), a22 = (*this)(2, 2);
        return a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) +
               a02 * (a10 * a21 - a11 * a20);
    }

    [[nodiscard]] Mat3 inverse() const
    {
        const f32 a00 = (*this)(0, 0), a01 = (*this)(0, 1), a02 = (*this)(0, 2);
        const f32 a10 = (*this)(1, 0), a11 = (*this)(1, 1), a12 = (*this)(1, 2);
        const f32 a20 = (*this)(2, 0), a21 = (*this)(2, 1), a22 = (*this)(2, 2);

        // Cofactor matrix.
        Mat3 cof{};
        cof(0, 0) = a11 * a22 - a12 * a21;
        cof(0, 1) = -(a10 * a22 - a12 * a20);
        cof(0, 2) = a10 * a21 - a11 * a20;
        cof(1, 0) = -(a01 * a22 - a02 * a21);
        cof(1, 1) = a00 * a22 - a02 * a20;
        cof(1, 2) = -(a00 * a21 - a01 * a20);
        cof(2, 0) = a01 * a12 - a02 * a11;
        cof(2, 1) = -(a00 * a12 - a02 * a10);
        cof(2, 2) = a00 * a11 - a01 * a10;

        const f32 det = a00 * cof(0, 0) + a01 * cof(0, 1) + a02 * cof(0, 2);
        if (std::abs(det) < 1e-6f)
            return Mat3::zero();

        // inverse = adjugate (cofactor transpose) / determinant
        Mat3 out = cof.transpose();
        const f32 invDet = 1.0f / det;
        for (int i = 0; i < 9; ++i)
            out.m[i] *= invDet;
        return out;
    }

    // Normal matrix for a world transform: inverse-transpose of the upper
    // 3x3, so normals stay perpendicular to the surface under non-uniform
    // scale (translation never affects direction vectors).
    [[nodiscard]] static Mat3 normalMatrix(const Mat4 &world)
    {
        return fromMat4(world).inverse().transpose();
    }
};

} // namespace Entelechy
