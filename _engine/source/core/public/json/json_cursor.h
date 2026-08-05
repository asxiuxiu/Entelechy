#pragma once
#include "core/foundation_types.h"
#include "core/string/string.h"
#include <cstdlib>
#include <cstring>

namespace Entelechy
{

// JsonCursor — minimal forward-only cursor over a JSON text buffer.
//
// Not a validating parser: it understands whitespace, literals, strings
// and numbers just well enough to read the engine's own fixed-schema JSON
// outputs (ECS scene serializer, mesh_cooker manifests). It exists so
// consumers stop carrying private copies (this type was extracted from
// the ECS SceneSerializer and the game-side scene_loader, which had
// diverged into two near-identical implementations).
//
// The buffer must stay alive for the cursor's lifetime and should be
// null-terminated so the strto* scanners always stop inside it.
struct JsonCursor
{
    const char *s;
    usize pos;
    usize len;

    void skipWs()
    {
        while (pos < len && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
            ++pos;
    }

    bool consume(char c)
    {
        skipWs();
        if (pos < len && s[pos] == c)
        {
            ++pos;
            return true;
        }
        return false;
    }

    bool match(const char *key)
    {
        skipWs();
        const usize klen = std::strlen(key);
        if (pos + klen > len)
            return false;
        if (std::strncmp(s + pos, key, klen) == 0)
        {
            pos += klen;
            return true;
        }
        return false;
    }

    // Reads a quoted string. Escape sequences are skipped over but not
    // decoded (the engine's own writers never emit them in practice).
    bool parseString(String &out)
    {
        skipWs();
        if (pos >= len || s[pos] != '"')
            return false;
        ++pos;
        const usize start = pos;
        while (pos < len && s[pos] != '"')
        {
            if (s[pos] == '\\' && pos + 1 < len)
                pos += 2;
            else
                ++pos;
        }
        if (pos >= len)
            return false;
        out.assign(s + start, pos - start);
        ++pos; // skip closing quote
        return true;
    }

    bool parseFloat(f32 &out)
    {
        skipWs();
        if (pos >= len)
            return false;
        char *end = nullptr;
        out = std::strtof(s + pos, &end);
        if (end == s + pos)
            return false;
        pos = static_cast<usize>(end - s);
        return true;
    }

    bool parseFloatArray(f32 *out, usize count)
    {
        if (!consume('['))
            return false;
        for (usize i = 0; i < count; ++i)
        {
            if (i > 0 && !consume(','))
                return false;
            if (!parseFloat(out[i]))
                return false;
        }
        return consume(']');
    }

    bool parseUint32(u32 &out)
    {
        skipWs();
        if (pos >= len)
            return false;
        char *end = nullptr;
        const unsigned long v = std::strtoul(s + pos, &end, 10);
        if (end == s + pos)
            return false;
        out = static_cast<u32>(v);
        pos = static_cast<usize>(end - s);
        return true;
    }

    bool parseInt32(i32 &out)
    {
        skipWs();
        if (pos >= len)
            return false;
        char *end = nullptr;
        const long v = std::strtol(s + pos, &end, 10);
        if (end == s + pos)
            return false;
        out = static_cast<i32>(v);
        pos = static_cast<usize>(end - s);
        return true;
    }

    bool parseBool(bool &out)
    {
        if (match("true"))
        {
            out = true;
            return true;
        }
        if (match("false"))
        {
            out = false;
            return true;
        }
        return false;
    }
};

} // namespace Entelechy
