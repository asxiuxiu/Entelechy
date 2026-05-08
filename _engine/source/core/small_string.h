#pragma once
#include "foundation_types.h"
#include <cstring>
#include <functional>

namespace Entelechy {

class SmallString {
    static constexpr usize SSO_CAPACITY = 15;
    union {
        char m_inline[SSO_CAPACITY + 1];
        struct {
            char* m_data;
            usize m_capacity;
        } m_heap;
    };
    usize m_len;

public:
    SmallString() {
        m_inline[0] = '\0';
        m_len = 0;
    }

    SmallString(const char* str) {
        if (!str) {
            m_inline[0] = '\0';
            m_len = 0;
            return;
        }
        m_len = std::strlen(str);
        if (m_len <= SSO_CAPACITY) {
            std::memcpy(m_inline, str, m_len + 1);
        } else {
            m_heap.m_capacity = m_len + 1;
            m_heap.m_data = new char[m_heap.m_capacity];
            std::memcpy(m_heap.m_data, str, m_len + 1);
        }
    }

    SmallString(const SmallString& other) {
        m_len = other.m_len;
        if (m_len <= SSO_CAPACITY) {
            std::memcpy(m_inline, other.m_inline, m_len + 1);
        } else {
            m_heap.m_capacity = other.m_heap.m_capacity;
            m_heap.m_data = new char[m_heap.m_capacity];
            std::memcpy(m_heap.m_data, other.m_heap.m_data, m_len + 1);
        }
    }

    SmallString(SmallString&& other) noexcept {
        m_len = other.m_len;
        if (other.m_len <= SSO_CAPACITY) {
            std::memcpy(m_inline, other.m_inline, m_len + 1);
        } else {
            m_heap.m_data = other.m_heap.m_data;
            m_heap.m_capacity = other.m_heap.m_capacity;
            other.m_inline[0] = '\0';
            other.m_len = 0;
        }
    }

    ~SmallString() {
        if (m_len > SSO_CAPACITY) {
            delete[] m_heap.m_data;
        }
    }

    SmallString& operator=(const SmallString& other) {
        if (this == &other) {
            return *this;
        }
        if (m_len > SSO_CAPACITY) {
            delete[] m_heap.m_data;
        }
        m_len = other.m_len;
        if (m_len <= SSO_CAPACITY) {
            std::memcpy(m_inline, other.m_inline, m_len + 1);
        } else {
            m_heap.m_capacity = other.m_heap.m_capacity;
            m_heap.m_data = new char[m_heap.m_capacity];
            std::memcpy(m_heap.m_data, other.m_heap.m_data, m_len + 1);
        }
        return *this;
    }

    SmallString& operator=(SmallString&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (m_len > SSO_CAPACITY) {
            delete[] m_heap.m_data;
        }
        m_len = other.m_len;
        if (other.m_len <= SSO_CAPACITY) {
            std::memcpy(m_inline, other.m_inline, m_len + 1);
        } else {
            m_heap.m_data = other.m_heap.m_data;
            m_heap.m_capacity = other.m_heap.m_capacity;
            other.m_inline[0] = '\0';
            other.m_len = 0;
        }
        return *this;
    }

    SmallString& operator=(const char* str) {
        if (m_len > SSO_CAPACITY) {
            delete[] m_heap.m_data;
        }
        if (!str) {
            m_inline[0] = '\0';
            m_len = 0;
            return *this;
        }
        m_len = std::strlen(str);
        if (m_len <= SSO_CAPACITY) {
            std::memcpy(m_inline, str, m_len + 1);
        } else {
            m_heap.m_capacity = m_len + 1;
            m_heap.m_data = new char[m_heap.m_capacity];
            std::memcpy(m_heap.m_data, str, m_len + 1);
        }
        return *this;
    }

    const char* c_str() const {
        return m_len <= SSO_CAPACITY ? m_inline : m_heap.m_data;
    }
    usize length() const { return m_len; }
    bool empty() const { return m_len == 0; }
    bool isInline() const { return m_len <= SSO_CAPACITY; }
    static constexpr usize inlineCapacity() { return SSO_CAPACITY; }

    void clear() {
        if (m_len > SSO_CAPACITY) {
            delete[] m_heap.m_data;
        }
        m_inline[0] = '\0';
        m_len = 0;
    }

    SmallString& append(const char* str) {
        if (!str || str[0] == '\0') {
            return *this;
        }
        usize add_len = std::strlen(str);
        usize new_len = m_len + add_len;
        if (new_len <= SSO_CAPACITY) {
            std::memcpy(m_inline + m_len, str, add_len + 1);
        } else {
            usize new_cap = new_len + 1;
            char* new_data = new char[new_cap];
            std::memcpy(new_data, c_str(), m_len);
            std::memcpy(new_data + m_len, str, add_len + 1);
            if (m_len > SSO_CAPACITY) {
                delete[] m_heap.m_data;
            }
            m_heap.m_data = new_data;
            m_heap.m_capacity = new_cap;
        }
        m_len = new_len;
        return *this;
    }

    SmallString& append(const SmallString& other) {
        return append(other.c_str());
    }

    SmallString& append(char ch) {
        char buf[2] = { ch, '\0' };
        return append(buf);
    }

    SmallString& operator+=(const char* str) { return append(str); }
    SmallString& operator+=(const SmallString& other) { return append(other); }
    SmallString& operator+=(char ch) { return append(ch); }

    bool operator==(const SmallString& other) const {
        return m_len == other.m_len && std::memcmp(c_str(), other.c_str(), m_len) == 0;
    }

    bool operator!=(const SmallString& other) const {
        return !(*this == other);
    }

    bool operator==(const char* str) const {
        if (!str) return m_len == 0;
        return std::strcmp(c_str(), str) == 0;
    }

    bool operator!=(const char* str) const {
        return !(*this == str);
    }
};

inline bool operator==(const char* lhs, const SmallString& rhs) {
    return rhs == lhs;
}

inline bool operator!=(const char* lhs, const SmallString& rhs) {
    return rhs != lhs;
}

} // namespace Entelechy

namespace std {
    template<>
    struct hash<Entelechy::SmallString> {
        size_t operator()(const Entelechy::SmallString& str) const noexcept {
            // FNV-1a for SmallString
            const char* s = str.c_str();
            uint64_t h = 0xcbf29ce484222325ULL;
            while (*s) {
                h ^= static_cast<u64>(*s);
                h *= 0x100000001b3ULL;
                ++s;
            }
            return static_cast<size_t>(h);
        }
    };
} // namespace std
