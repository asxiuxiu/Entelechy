#pragma once
#include "core/foundation_types.h"
#include <cstring>
#include <functional>

namespace Entelechy
{

// Forward declaration for StringView's templated constructor.
template <typename AllocatorT>
class BasicString;

class StringView
{
    const char *m_data = "";
    usize m_len = 0;

    static constexpr usize constexpr_strlen(const char *s)
    {
        usize len = 0;
        while (s && s[len])
            ++len;
        return len;
    }

public:
    static constexpr usize npos = static_cast<usize>(-1);

    constexpr StringView() = default;
    constexpr StringView(const char *s) : m_data(s ? s : ""), m_len(s ? constexpr_strlen(s) : 0) {}
    constexpr StringView(const char *s, usize len) : m_data(s ? s : ""), m_len(len) {}

    template <typename AllocatorT>
    StringView(const BasicString<AllocatorT> &s);

    [[nodiscard]] constexpr const char *data() const
    {
        return m_data;
    }
    [[nodiscard]] constexpr usize size() const
    {
        return m_len;
    }
    [[nodiscard]] constexpr usize length() const
    {
        return m_len;
    }
    [[nodiscard]] constexpr bool empty() const
    {
        return m_len == 0;
    }
    [[nodiscard]] constexpr char operator[](usize i) const
    {
        return m_data[i];
    }

    [[nodiscard]] constexpr StringView substr(usize pos = 0, usize len = npos) const
    {
        if (pos >= m_len)
            return StringView();
        usize actual = (len == npos || pos + len > m_len) ? (m_len - pos) : len;
        return StringView(m_data + pos, actual);
    }

    [[nodiscard]] constexpr bool startsWith(const char *prefix) const
    {
        if (!prefix)
            return true;
        usize preLen = constexpr_strlen(prefix);
        if (preLen > m_len)
            return false;
        return std::memcmp(m_data, prefix, preLen) == 0;
    }

    [[nodiscard]] constexpr bool startsWith(StringView prefix) const
    {
        if (prefix.m_len > m_len)
            return false;
        return std::memcmp(m_data, prefix.m_data, prefix.m_len) == 0;
    }

    [[nodiscard]] constexpr bool endsWith(const char *suffix) const
    {
        if (!suffix)
            return true;
        usize sufLen = constexpr_strlen(suffix);
        if (sufLen > m_len)
            return false;
        return std::memcmp(m_data + (m_len - sufLen), suffix, sufLen) == 0;
    }

    [[nodiscard]] constexpr bool endsWith(StringView suffix) const
    {
        if (suffix.m_len > m_len)
            return false;
        return std::memcmp(m_data + (m_len - suffix.m_len), suffix.m_data, suffix.m_len) == 0;
    }

    [[nodiscard]] constexpr bool operator==(StringView other) const
    {
        return m_len == other.m_len && std::memcmp(m_data, other.m_data, m_len) == 0;
    }

    [[nodiscard]] constexpr bool operator!=(StringView other) const
    {
        return !(*this == other);
    }

    [[nodiscard]] constexpr usize find(char ch, usize pos = 0) const
    {
        for (usize i = pos; i < m_len; ++i)
        {
            if (m_data[i] == ch)
                return i;
        }
        return npos;
    }

    [[nodiscard]] constexpr usize find(const char *str, usize pos = 0) const
    {
        if (!str || str[0] == '\0')
            return 0;
        usize subLen = constexpr_strlen(str);
        if (subLen > m_len || pos >= m_len)
            return npos;
        for (usize i = pos; i + subLen <= m_len; ++i)
        {
            if (std::memcmp(m_data + i, str, subLen) == 0)
                return i;
        }
        return npos;
    }

    [[nodiscard]] constexpr usize find(StringView str, usize pos = 0) const
    {
        if (str.empty())
            return 0;
        if (str.m_len > m_len || pos >= m_len)
            return npos;
        for (usize i = pos; i + str.m_len <= m_len; ++i)
        {
            if (std::memcmp(m_data + i, str.m_data, str.m_len) == 0)
                return i;
        }
        return npos;
    }

    [[nodiscard]] constexpr usize findLast(char ch) const
    {
        for (usize i = m_len; i > 0; --i)
        {
            if (m_data[i - 1] == ch)
                return i - 1;
        }
        return npos;
    }
};

} // namespace Entelechy

namespace std
{
template <>
struct hash<Entelechy::StringView>
{
    usize operator()(Entelechy::StringView sv) const noexcept
    {
        const char *s = sv.data();
        u64 h = 0xcbf29ce484222325ULL;
        for (usize i = 0; i < sv.length(); ++i)
        {
            h ^= static_cast<u64>(s[i]);
            h *= 0x100000001b3ULL;
        }
        return static_cast<usize>(h);
    }
};
} // namespace std
