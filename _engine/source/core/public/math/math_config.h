#pragma once
#include "core/foundation_types.h"

// ============================================================
// Math Library Configuration
// ============================================================

#ifndef MATH_ENABLE_NAN_CHECKS
    #if defined(_DEBUG) || defined(DEBUG)
        #define MATH_ENABLE_NAN_CHECKS 1
    #else
        #define MATH_ENABLE_NAN_CHECKS 0
    #endif
#endif

#if MATH_ENABLE_NAN_CHECKS
    #include <cmath>
    #define MATH_CHECK_FINITE_2(v) \
        CHECK(std::isfinite((v).x) && std::isfinite((v).y))
    #define MATH_CHECK_FINITE_3(v) \
        CHECK(std::isfinite((v).x) && std::isfinite((v).y) && std::isfinite((v).z))
    #define MATH_CHECK_FINITE_4(v) \
        CHECK(std::isfinite((v).x) && std::isfinite((v).y) && std::isfinite((v).z) && std::isfinite((v).w))
    #define MATH_CHECK_FINITE_QUAT(q) \
        CHECK(std::isfinite((q).x) && std::isfinite((q).y) && std::isfinite((q).z) && std::isfinite((q).w))
#else
    #define MATH_CHECK_FINITE_2(v) ((void)0)
    #define MATH_CHECK_FINITE_3(v) ((void)0)
    #define MATH_CHECK_FINITE_4(v) ((void)0)
    #define MATH_CHECK_FINITE_QUAT(q) ((void)0)
#endif
