#pragma once
#include "core/string/small_string.h"
#include "core/foundation_types.h"
#include "core/allocator/allocator.h"
#include <cstring>

namespace Entelechy {

// ------------------------------------------------------------------
// Path — cross-platform path abstraction
// ------------------------------------------------------------------
// Internal storage always uses '/' as the separator.
// Platform conversion only happens when calling OS APIs.

class Path {
    SmallString m_path;

    static void normalize(SmallString& str) {
        const char* s = str.c_str();
        usize len = str.length();
        if (len == 0) return;

        char* buf = static_cast<char*>(DefaultAllocator::alloc(len + 1, alignof(char)));
        std::memcpy(buf, s, len + 1);

        for (usize i = 0; i < len; ++i) {
            if (buf[i] == '\\') buf[i] = '/';
        }

        // Remove trailing '/' (but preserve root "/")
        while (len > 1 && buf[len - 1] == '/') {
            --len;
        }
        buf[len] = '\0';

        str.assign(buf, len);
        DefaultAllocator::free(buf);
    }

public:
    Path() = default;

    explicit Path(const char* p) : m_path(p) {
        normalize(m_path);
    }

    explicit Path(const SmallString& str) : m_path(str) {
        normalize(m_path);
    }

    Path operator/(const char* sub) const {
        if (!sub || sub[0] == '\0') return *this;

        Path result = *this;
        const char* s = result.m_path.c_str();
        usize len = result.m_path.length();

        // If sub is absolute, just replace
        if (sub[0] == '/' || sub[0] == '\\') {
            result.m_path = sub;
            normalize(result.m_path);
            return result;
        }

        // Append separator if needed
        if (len > 0 && s[len - 1] != '/') {
            result.m_path.append('/');
        }
        result.m_path.append(sub);
        normalize(result.m_path);
        return result;
    }

    Path operator/(const SmallString& sub) const {
        return operator/(sub.c_str());
    }

    Path operator/(const Path& sub) const {
        return operator/(sub.m_path.c_str());
    }

    Path& operator/=(const char* sub) {
        *this = operator/(sub);
        return *this;
    }

    const char* c_str() const { return m_path.c_str(); }
    usize length() const { return m_path.length(); }
    bool empty() const { return m_path.empty(); }

    // Returns the file extension (including the dot), or empty string if none.
    SmallString extension() const {
        const char* s = m_path.c_str();
        usize len = m_path.length();
        // Find last '/'
        usize lastSlash = SmallString::npos;
        for (usize i = 0; i < len; ++i) {
            if (s[i] == '/') lastSlash = i;
        }
        // Find last '.' after lastSlash
        usize lastDot = SmallString::npos;
        for (usize i = (lastSlash == SmallString::npos ? 0 : lastSlash + 1); i < len; ++i) {
            if (s[i] == '.') lastDot = i;
        }
        if (lastDot == SmallString::npos || lastDot == len - 1) return SmallString();
        return m_path.substr(lastDot);
    }

    // Returns the file name including extension.
    SmallString fileName() const {
        const char* s = m_path.c_str();
        usize len = m_path.length();
        usize lastSlash = SmallString::npos;
        for (usize i = 0; i < len; ++i) {
            if (s[i] == '/') lastSlash = i;
        }
        if (lastSlash == SmallString::npos) return m_path;
        if (lastSlash + 1 >= len) return SmallString();
        return m_path.substr(lastSlash + 1);
    }

    // Returns the file name without extension.
    SmallString stem() const {
        SmallString name = fileName();
        const char* s = name.c_str();
        usize len = name.length();
        usize lastDot = SmallString::npos;
        for (usize i = 0; i < len; ++i) {
            if (s[i] == '.') lastDot = i;
        }
        if (lastDot == SmallString::npos || lastDot == 0) return name;
        return name.substr(0, lastDot);
    }

    // Returns true if the path is absolute.
    bool isAbsolute() const {
        const char* s = m_path.c_str();
        if (s[0] == '/') return true;
#if PLATFORM_WINDOWS
        // Windows: "C:/" or "C:\" are absolute
        if (m_path.length() >= 2 && s[1] == ':') return true;
#endif
        return false;
    }

    bool operator==(const Path& other) const { return m_path == other.m_path; }
    bool operator!=(const Path& other) const { return m_path != other.m_path; }
};

} // namespace Entelechy

namespace std {
    template<>
    struct hash<Entelechy::Path> {
        usize operator()(const Entelechy::Path& path) const noexcept {
            return std::hash<Entelechy::SmallString>{}(Entelechy::SmallString(path.c_str()));
        }
    };
} // namespace std
