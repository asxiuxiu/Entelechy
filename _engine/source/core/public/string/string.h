#pragma once
#include "core/foundation_types.h"
#include "core/allocator/allocator.h"
#include "core/allocator/std_allocator_adapter.h"
#include "core/string/string_view.h"
#include <string>
#include <cstring>
#include <functional>

namespace Entelechy
{

template <typename AllocatorT = DefaultAllocator>
class BasicString
{
    using AllocType = StdAllocatorAdapter<char, AllocatorT>;
    std::basic_string<char, std::char_traits<char>, AllocType> m_str;

public:
    static constexpr usize npos = static_cast<usize>(-1);
    static constexpr usize SSO_CAPACITY = 15;

    BasicString() = default;

    BasicString(const char *str) : m_str(str ? str : "") {}

    BasicString(const char *str, usize len)
    {
        if (str && len > 0)
        {
            m_str.assign(str, len);
        }
    }

    BasicString(StringView sv) : m_str(sv.data(), sv.length()) {}

    BasicString(const BasicString &other) = default;
    BasicString(BasicString &&other) noexcept = default;
    ~BasicString() = default;

    BasicString &operator=(const BasicString &other) = default;
    BasicString &operator=(BasicString &&other) noexcept = default;

    BasicString &operator=(const char *str)
    {
        m_str = (str ? str : "");
        return *this;
    }

    BasicString &assign(const char *str, usize len)
    {
        if (str && len > 0)
        {
            m_str.assign(str, len);
        }
        else
        {
            m_str.clear();
        }
        return *this;
    }

    const char *c_str() const
    {
        return m_str.c_str();
    }
    char operator[](usize i) const
    {
        return m_str[i];
    }
    usize length() const
    {
        return m_str.size();
    }
    usize size() const
    {
        return m_str.size();
    }
    bool empty() const
    {
        return m_str.empty();
    }
    bool isInline() const
    {
        return m_str.size() <= SSO_CAPACITY;
    }
    static constexpr usize inlineCapacity()
    {
        return SSO_CAPACITY;
    }

    [[nodiscard]] usize capacity() const
    {
        return m_str.capacity();
    }

    BasicString &reserve(usize min_cap)
    {
        m_str.reserve(min_cap);
        return *this;
    }

    void clear()
    {
        m_str.clear();
    }

    BasicString &append(const char *str)
    {
        if (str)
        {
            m_str.append(str);
        }
        return *this;
    }

    BasicString &append(const BasicString &other)
    {
        m_str.append(other.m_str);
        return *this;
    }

    BasicString &append(StringView sv)
    {
        m_str.append(sv.data(), sv.length());
        return *this;
    }

    BasicString &append(char ch)
    {
        m_str.push_back(ch);
        return *this;
    }

    BasicString &operator+=(const char *str)
    {
        return append(str);
    }
    BasicString &operator+=(const BasicString &other)
    {
        return append(other);
    }
    BasicString &operator+=(StringView sv)
    {
        return append(sv);
    }
    BasicString &operator+=(char ch)
    {
        return append(ch);
    }

    bool operator==(const BasicString &other) const
    {
        return m_str == other.m_str;
    }

    bool operator!=(const BasicString &other) const
    {
        return !(*this == other);
    }

    bool operator==(const char *str) const
    {
        return m_str == (str ? str : "");
    }

    bool operator!=(const char *str) const
    {
        return !(*this == str);
    }

    bool operator==(StringView sv) const
    {
        return m_str.size() == sv.length() && std::memcmp(m_str.data(), sv.data(), sv.length()) == 0;
    }

    bool operator!=(StringView sv) const
    {
        return !(*this == sv);
    }

    usize find(const char *str, usize pos = 0) const
    {
        if (!str || str[0] == '\0')
            return 0;
        usize subLen = std::strlen(str);
        if (subLen > m_str.size() || pos >= m_str.size())
            return npos;
        for (usize i = pos; i + subLen <= m_str.size(); ++i)
        {
            if (std::memcmp(m_str.data() + i, str, subLen) == 0)
            {
                return i;
            }
        }
        return npos;
    }

    usize find(const BasicString &str, usize pos = 0) const
    {
        return find(str.c_str(), pos);
    }

    usize find(StringView sv, usize pos = 0) const
    {
        if (sv.empty())
            return 0;
        if (sv.length() > m_str.size() || pos >= m_str.size())
            return npos;
        for (usize i = pos; i + sv.length() <= m_str.size(); ++i)
        {
            if (std::memcmp(m_str.data() + i, sv.data(), sv.length()) == 0)
            {
                return i;
            }
        }
        return npos;
    }

    usize find(char ch, usize pos = 0) const
    {
        const char *s = m_str.data();
        for (usize i = pos; i < m_str.size(); ++i)
        {
            if (s[i] == ch)
                return i;
        }
        return npos;
    }

    BasicString substr(usize pos = 0, usize len = npos) const
    {
        if (pos >= m_str.size())
            return BasicString();
        usize actualLen = (len == npos || pos + len > m_str.size()) ? (m_str.size() - pos) : len;
        return BasicString(m_str.data() + pos, actualLen);
    }

    StringView view() const
    {
        return StringView(m_str.data(), m_str.size());
    }

    StringView view(usize pos, usize len = npos) const
    {
        if (pos >= m_str.size())
            return StringView();
        usize actualLen = (len == npos || pos + len > m_str.size()) ? (m_str.size() - pos) : len;
        return StringView(m_str.data() + pos, actualLen);
    }

    bool startsWith(const char *prefix) const
    {
        if (!prefix || prefix[0] == '\0')
            return true;
        usize preLen = std::strlen(prefix);
        if (preLen > m_str.size())
            return false;
        return std::memcmp(m_str.data(), prefix, preLen) == 0;
    }

    bool startsWith(StringView prefix) const
    {
        if (prefix.empty())
            return true;
        if (prefix.length() > m_str.size())
            return false;
        return std::memcmp(m_str.data(), prefix.data(), prefix.length()) == 0;
    }

    bool endsWith(const char *suffix) const
    {
        if (!suffix || suffix[0] == '\0')
            return true;
        usize sufLen = std::strlen(suffix);
        if (sufLen > m_str.size())
            return false;
        return std::memcmp(m_str.data() + (m_str.size() - sufLen), suffix, sufLen) == 0;
    }

    bool endsWith(StringView suffix) const
    {
        if (suffix.empty())
            return true;
        if (suffix.length() > m_str.size())
            return false;
        return std::memcmp(m_str.data() + (m_str.size() - suffix.length()), suffix.data(), suffix.length()) == 0;
    }
};

template <typename AllocatorT>
inline bool operator==(const char *lhs, const BasicString<AllocatorT> &rhs)
{
    return rhs == lhs;
}

template <typename AllocatorT>
inline bool operator!=(const char *lhs, const BasicString<AllocatorT> &rhs)
{
    return rhs != lhs;
}

template <typename AllocatorT>
inline bool operator==(StringView lhs, const BasicString<AllocatorT> &rhs)
{
    return rhs == lhs;
}

template <typename AllocatorT>
inline bool operator!=(StringView lhs, const BasicString<AllocatorT> &rhs)
{
    return rhs != lhs;
}

template <typename AllocatorT>
inline BasicString<AllocatorT> operator+(const BasicString<AllocatorT> &lhs, const BasicString<AllocatorT> &rhs)
{
    BasicString<AllocatorT> result = lhs;
    result += rhs;
    return result;
}

template <typename AllocatorT>
inline BasicString<AllocatorT> operator+(const BasicString<AllocatorT> &lhs, const char *rhs)
{
    BasicString<AllocatorT> result = lhs;
    result += rhs;
    return result;
}

template <typename AllocatorT>
inline BasicString<AllocatorT> operator+(const char *lhs, const BasicString<AllocatorT> &rhs)
{
    BasicString<AllocatorT> result(lhs);
    result += rhs;
    return result;
}

template <typename AllocatorT>
inline BasicString<AllocatorT> operator+(const BasicString<AllocatorT> &lhs, StringView rhs)
{
    BasicString<AllocatorT> result = lhs;
    result += rhs;
    return result;
}

template <typename AllocatorT>
inline BasicString<AllocatorT> operator+(StringView lhs, const BasicString<AllocatorT> &rhs)
{
    BasicString<AllocatorT> result(lhs.data(), lhs.length());
    result += rhs;
    return result;
}

// StringView construction from BasicString (breaks circular include between string_view.h and string.h)
template <typename AllocatorT>
inline StringView::StringView(const BasicString<AllocatorT> &s) : m_data(s.c_str()), m_len(s.length())
{
}

using String = BasicString<DefaultAllocator>;

} // namespace Entelechy

namespace std
{
template <typename AllocatorT>
struct hash<Entelechy::BasicString<AllocatorT>>
{
    usize operator()(const Entelechy::BasicString<AllocatorT> &str) const noexcept
    {
        const char *s = str.c_str();
        u64 h = 0xcbf29ce484222325ULL;
        while (*s)
        {
            h ^= static_cast<u64>(*s);
            h *= 0x100000001b3ULL;
            ++s;
        }
        return static_cast<usize>(h);
    }
};
} // namespace std
