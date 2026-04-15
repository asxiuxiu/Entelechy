#include "math_lib.h"

namespace Entelechy {

Mat4 Mat4::fromRotation(const Quat& q) {
    float x2 = q.x * 2.0f;
    float y2 = q.y * 2.0f;
    float z2 = q.z * 2.0f;
    float wx = q.w * x2;
    float wy = q.w * y2;
    float wz = q.w * z2;
    float xx = q.x * x2;
    float xy = q.x * y2;
    float xz = q.x * z2;
    float yy = q.y * y2;
    float yz = q.y * z2;
    float zz = q.z * z2;

    Mat4 r{};
    r.m[0]  = 1.0f - (yy + zz);
    r.m[1]  = xy + wz;
    r.m[2]  = xz - wy;
    r.m[3]  = 0.0f;
    r.m[4]  = xy - wz;
    r.m[5]  = 1.0f - (xx + zz);
    r.m[6]  = yz + wx;
    r.m[7]  = 0.0f;
    r.m[8]  = xz + wy;
    r.m[9]  = yz - wx;
    r.m[10] = 1.0f - (xx + yy);
    r.m[11] = 0.0f;
    r.m[12] = 0.0f;
    r.m[13] = 0.0f;
    r.m[14] = 0.0f;
    r.m[15] = 1.0f;
    return r;
}

} // namespace Entelechy
