#pragma once
#include "foundation_types.h"
#include <cstdio>
#include <type_traits>
#include "small_string.h"

namespace Entelechy {

// Verify alias -> primitive mappings so that template specializations below are complete.
static_assert(std::is_same_v<u32, unsigned int>,       "u32 must match unsigned int");
static_assert(std::is_same_v<i32, int>,                "i32 must match int");
static_assert(std::is_same_v<u64, unsigned long long>, "u64 must match unsigned long long");
static_assert(std::is_same_v<i64, long long>,          "i64 must match long long");
static_assert(std::is_same_v<f32, float>,              "f32 must match float");
static_assert(std::is_same_v<f64, double>,             "f64 must match double");

// ------------------------------------------------------------------
// toStringBuf — type-safe single-value formatting into a char buffer
// ------------------------------------------------------------------
// Default: compile-time error for unsupported types (forces explicit registration)
template<typename T>
void toStringBuf(char* buf, usize n, T v) {
    static_assert(!std::is_same_v<T, T>, "Type not supported by string format");
}

// Integers
template<>
inline void toStringBuf(char* buf, usize n, int v) {
    snprintf(buf, n, "%d", v);
}

template<>
inline void toStringBuf(char* buf, usize n, long long v) {
    snprintf(buf, n, "%lld", v);
}

template<>
inline void toStringBuf(char* buf, usize n, unsigned int v) {
    snprintf(buf, n, "%u", v);
}

template<>
inline void toStringBuf(char* buf, usize n, unsigned long long v) {
    snprintf(buf, n, "%llu", v);
}

// 8-bit integers (print as numbers, not characters)
template<>
inline void toStringBuf(char* buf, usize n, u8 v) {
    snprintf(buf, n, "%u", static_cast<unsigned int>(v));
}

template<>
inline void toStringBuf(char* buf, usize n, i8 v) {
    snprintf(buf, n, "%d", static_cast<int>(v));
}

// Floating-point
template<>
inline void toStringBuf(char* buf, usize n, f32 v) {
    snprintf(buf, n, "%.3f", static_cast<double>(v));
}

template<>
inline void toStringBuf(char* buf, usize n, f64 v) {
    snprintf(buf, n, "%.3f", v);
}

// C-string
template<>
inline void toStringBuf(char* buf, usize n, const char* v) {
    if (v) {
        snprintf(buf, n, "%s", v);
    } else {
        snprintf(buf, n, "(null)");
    }
}

// Bool
template<>
inline void toStringBuf(char* buf, usize n, bool v) {
    snprintf(buf, n, "%s", v ? "true" : "false");
}

// SmallString (non-template overload: matches lvalue, rvalue, and temporaries)
inline void toStringBuf(char* buf, usize n, const SmallString& v) {
    snprintf(buf, n, "%s", v.c_str());
}

// ------------------------------------------------------------------
// formatString — zero-heap-allocation positional formatter
// ------------------------------------------------------------------
// Syntax: "Player {0} pos=({1}, {2})"  (up to 10 arguments, index 0-9)
// All argument buffers live on the stack; no dynamic allocation occurs.

template<typename... Args>
int formatString(char* out, usize outSize, const char* fmt, Args&&... args) {
    if (outSize == 0) {
        return 0;
    }
    constexpr usize N = sizeof...(Args);
    char argBufs[N][64] = {};
    usize idx = 0;
    (..., (toStringBuf(argBufs[idx], 64, std::forward<Args>(args)), ++idx));

    usize pos = 0;
    for (const char* p = fmt; *p && pos < outSize - 1; ++p) {
        if (*p == '{' && *(p + 1) >= '0' && *(p + 1) <= '9' && *(p + 2) == '}') {
            int ai = *(p + 1) - '0';
            if (ai < static_cast<int>(N)) {
                for (const char* s = argBufs[ai]; *s && pos < outSize - 1; ++s) {
                    out[pos++] = *s;
                }
            }
            p += 2;
        } else {
            out[pos++] = *p;
        }
    }
    out[pos] = '\0';
    return static_cast<int>(pos);
}

// Convenience overload: returns a SmallString instead of writing to a buffer
template<typename... Args>
SmallString formatString(const char* fmt, Args&&... args) {
    char buf[512];
    formatString(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
    return SmallString(buf);
}

} // namespace Entelechy
