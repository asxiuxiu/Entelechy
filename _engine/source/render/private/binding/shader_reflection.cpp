#include "render/binding/shader_reflection.h"
#include "core/json/json_cursor.h"
#include "core/math/align.h"
#include "log/core/log_macros.h"
#include <cstring>

namespace Entelechy
{

namespace
{

constexpr LogCategory kLogReflection("Render");

ShaderMemberType memberTypeFromName(const String &typeName)
{
    if (typeName == "float")
        return ShaderMemberType::Float;
    if (typeName == "vec2")
        return ShaderMemberType::Vec2;
    if (typeName == "vec3")
        return ShaderMemberType::Vec3;
    if (typeName == "vec4")
        return ShaderMemberType::Vec4;
    if (typeName == "mat3")
        return ShaderMemberType::Mat3;
    if (typeName == "mat4")
        return ShaderMemberType::Mat4;
    if (typeName == "int")
        return ShaderMemberType::Int;
    if (typeName == "uint")
        return ShaderMemberType::Uint;
    if (typeName == "bool")
        return ShaderMemberType::Bool;
    return ShaderMemberType::Unknown;
}

// Skip one JSON value (string, number, literal, array or object). Used to
// ignore keys the parser does not care about.
void skipJsonValue(JsonCursor &c)
{
    c.skipWs();
    if (c.pos >= c.len)
        return;

    const char ch = c.s[c.pos];
    if (ch == '"')
    {
        String tmp;
        c.parseString(tmp);
        return;
    }
    if (ch == '{' || ch == '[')
    {
        int depth = 0;
        while (c.pos < c.len)
        {
            const char d = c.s[c.pos];
            if (d == '{' || d == '[')
                ++depth;
            else if (d == '}' || d == ']')
                --depth;
            ++c.pos;
            if (depth == 0)
                return;
        }
        return;
    }
    // Number or literal: consume up to the next separator.
    while (c.pos < c.len && c.s[c.pos] != ',' && c.s[c.pos] != ']' && c.s[c.pos] != '}')
        ++c.pos;
}

bool parseMember(JsonCursor &c, ShaderReflectionMember &out)
{
    // { "name": "...", "type": "...", "offset": N, "size": N }
    if (!c.consume('{'))
        return false;
    while (!c.consume('}'))
    {
        String key;
        if (!c.parseString(key))
        {
            return false;
        }
        c.consume(':');
        if (key == "name")
        {
            c.parseString(out.name);
        }
        else if (key == "type")
        {
            String typeName;
            if (c.parseString(typeName))
                out.type = memberTypeFromName(typeName);
        }
        else if (key == "offset")
        {
            c.parseUint32(out.offset);
        }
        else if (key == "size")
        {
            c.parseUint32(out.size);
        }
        else
        {
            skipJsonValue(c);
        }
        c.skipWs();
        c.consume(','); // optional separator before next key or '}'
    }
    return true;
}

bool parseCBuffer(JsonCursor &c, ShaderReflectionCBuffer &out)
{
    // { "name": "...", "binding": N, "size": N, "members": [ ... ] }
    if (!c.consume('{'))
        return false;
    while (!c.consume('}'))
    {
        String key;
        if (!c.parseString(key))
        {
            return false;
        }
        c.consume(':');
        if (key == "name")
        {
            c.parseString(out.name);
        }
        else if (key == "binding")
        {
            c.parseUint32(out.binding);
        }
        else if (key == "size")
        {
            c.parseUint32(out.size);
        }
        else if (key == "members")
        {
            if (!c.consume('['))
                return false;
            while (!c.consume(']'))
            {
                ShaderReflectionMember member;
                if (!parseMember(c, member))
                    return false;
                out.members.pushBack(std::move(member));
                c.skipWs();
                c.consume(','); // optional separator before next member or ']'
            }
        }
        else
        {
            skipJsonValue(c);
        }
        c.skipWs();
        c.consume(','); // optional separator before next key or '}'
    }
    return true;
}

bool parseTexture(JsonCursor &c, ShaderReflectionTexture &out)
{
    // { "name": "...", "binding": N }
    if (!c.consume('{'))
        return false;
    while (!c.consume('}'))
    {
        String key;
        if (!c.parseString(key))
        {
            return false;
        }
        c.consume(':');
        if (key == "name")
        {
            c.parseString(out.name);
        }
        else if (key == "binding")
        {
            c.parseUint32(out.binding);
        }
        else
        {
            skipJsonValue(c);
        }
        c.skipWs();
        c.consume(','); // optional separator before next key or '}'
    }
    return true;
}

} // namespace

const ShaderReflectionMember *ShaderReflectionCBuffer::findMember(const String &name) const
{
    for (usize i = 0; i < members.size(); ++i)
    {
        if (members[i].name == name)
            return &members[i];
    }
    return nullptr;
}

const ShaderReflectionCBuffer *ShaderReflection::findCBufferByBinding(u32 binding) const
{
    for (usize i = 0; i < cbuffers.size(); ++i)
    {
        if (cbuffers[i].binding == binding)
            return &cbuffers[i];
    }
    return nullptr;
}

bool parseShaderReflection(const char *text, usize length, ShaderReflection &out)
{
    if (!text || length == 0)
        return false;

    JsonCursor c{text, 0, length};
    if (!c.consume('{'))
    {
        LOG_ERROR(kLogReflection, "parseShaderReflection: expected top-level object");
        return false;
    }

    while (!c.consume('}'))
    {
        String key;
        if (!c.parseString(key))
        {
            return false;
        }
        c.consume(':');
        if (key == "name")
        {
            c.parseString(out.name);
        }
        else if (key == "stage")
        {
            String stage;
            if (c.parseString(stage))
                out.stage = (stage == "vertex") ? ShaderStage::Vertex : ShaderStage::Fragment;
        }
        else if (key == "cbuffers")
        {
            if (!c.consume('['))
                return false;
            while (!c.consume(']'))
            {
                ShaderReflectionCBuffer cbuffer;
                if (!parseCBuffer(c, cbuffer))
                    return false;
                c.skipWs();
                c.consume(','); // optional separator before next cbuffer or ']'
                // Pad to 16 so ring allocations cover the full std140 block
                // (SPIR-V declared size can end mid-vector, e.g. 40 for a
                // 48-byte block; GL and D3D12 both round to 16).
                cbuffer.size = static_cast<u32>(AlignUp(static_cast<usize>(cbuffer.size), 16));
                out.cbuffers.pushBack(std::move(cbuffer));
            }
        }
        else if (key == "textures")
        {
            if (!c.consume('['))
                return false;
            while (!c.consume(']'))
            {
                ShaderReflectionTexture texture;
                if (!parseTexture(c, texture))
                    return false;
                out.textures.pushBack(std::move(texture));
                c.skipWs();
                c.consume(','); // optional separator before next texture or ']'
            }
        }
        else
        {
            skipJsonValue(c);
        }
        c.skipWs();
        c.consume(','); // optional separator before next key or '}'
    }

    return true;
}

} // namespace Entelechy
