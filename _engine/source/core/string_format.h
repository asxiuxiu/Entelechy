#pragma once
#include <cstdio>
#include <type_traits>
#include "small_string.h"

namespace Entelechy {

template<typename T>
void toStringBuf(char* buf, size_t n, T v) {
    static_assert(!std::is_same_v<T, T>, "Type not supported by string format");
}

template<>
inline void toStringBuf(char* buf, size_t n, int v) {
    snprintf(buf, n, "%d", v);
}

template<>
inline void toStringBuf(char* buf, size_t n, long long v) {
    snprintf(buf, n, "%lld", v);
}

template<>
inline void toStringBuf(char* buf, size_t n, unsigned int v) {
    snprintf(buf, n, "%u", v);
}

template<>
inline void toStringBuf(char* buf, size_t n, unsigned long long v) {
    snprintf(buf, n, "%llu", v);
}

template<>
inline void toStringBuf(char* buf, size_t n, float v) {
    snprintf(buf, n, "%.3f", static_cast<double>(v));
}

template<>
inline void toStringBuf(char* buf, size_t n, double v) {
    snprintf(buf, n, "%.3f", v);
}

template<>
inline void toStringBuf(char* buf, size_t n, const char* v) {
    if (v) {
        snprintf(buf, n, "%s", v);
    } else {
        snprintf(buf, n, "(null)");
    }
}

template<>
inline void toStringBuf(char* buf, size_t n, bool v) {
    snprintf(buf, n, "%s", v ? "true" : "false");
}

template<>
inline void toStringBuf(char* buf, size_t n, SmallString v) {
    snprintf(buf, n, "%s", v.c_str());
}

template<>
inline void toStringBuf(char* buf, size_t n, const SmallString& v) {
    snprintf(buf, n, "%s", v.c_str());
}

template<typename... Args>
int formatString(char* out, size_t outSize, const char* fmt, Args&&... args) {
    if (outSize == 0) {
        return 0;
    }
    constexpr size_t N = sizeof...(Args);
    char argBufs[N][64] = {};
    size_t idx = 0;
    (..., (toStringBuf(argBufs[idx], 64, std::forward<Args>(args)), ++idx));

    size_t pos = 0;
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

template<typename... Args>
SmallString formatString(const char* fmt, Args&&... args) {
    char buf[512];
    formatString(buf, sizeof(buf), fmt, std::forward<Args>(args)...);
    return SmallString(buf);
}

} // namespace Entelechy
