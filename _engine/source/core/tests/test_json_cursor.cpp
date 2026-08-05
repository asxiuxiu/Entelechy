#include "test/test_framework.h"
#include "core/json/json_cursor.h"
#include <cmath>
#include <cstring>

using namespace Entelechy;

namespace
{

JsonCursor cursorOver(const char *text)
{
    return JsonCursor{text, 0, std::strlen(text)};
}

bool floatNear(f32 a, f32 b)
{
    return std::fabs(a - b) < 1e-5f;
}

} // namespace

TEST(JsonCursor, ConsumeSkipsWhitespace)
{
    JsonCursor cur = cursorOver(" \t\r\n{ }");
    ASSERT_TRUE(cur.consume('{'));
    ASSERT_TRUE(cur.consume('}'));
    ASSERT_FALSE(cur.consume(','));
}

TEST(JsonCursor, MatchLiteral)
{
    JsonCursor cur = cursorOver("  true false");
    ASSERT_TRUE(cur.match("true"));
    ASSERT_FALSE(cur.match("true"));
    ASSERT_TRUE(cur.match("false"));
}

TEST(JsonCursor, ParseString)
{
    JsonCursor cur = cursorOver("  \"meshes/arch_01.emesh\" ,");
    String out;
    ASSERT_TRUE(cur.parseString(out));
    ASSERT_TRUE(out == "meshes/arch_01.emesh");
    ASSERT_TRUE(cur.consume(','));
}

TEST(JsonCursor, ParseStringSkipsEscapes)
{
    // The escaped quote must not terminate the string; escapes are
    // skipped over, not decoded.
    JsonCursor cur = cursorOver("\"a\\\"b\"");
    String out;
    ASSERT_TRUE(cur.parseString(out));
    ASSERT_EQ(out.length(), 4u);
    ASSERT_EQ(cur.pos, cur.len);
}

TEST(JsonCursor, ParseNumbers)
{
    JsonCursor cur = cursorOver("-13.5 2e2 42 -7");
    f32 f = 0.0f;
    u32 u = 0;
    i32 i = 0;
    ASSERT_TRUE(cur.parseFloat(f));
    ASSERT_TRUE(floatNear(f, -13.5f));
    ASSERT_TRUE(cur.parseFloat(f));
    ASSERT_TRUE(floatNear(f, 200.0f));
    ASSERT_TRUE(cur.parseUint32(u));
    ASSERT_EQ(u, 42u);
    ASSERT_TRUE(cur.parseInt32(i));
    ASSERT_EQ(i, -7);
}

TEST(JsonCursor, ParseBool)
{
    JsonCursor cur = cursorOver("true false");
    bool b = false;
    ASSERT_TRUE(cur.parseBool(b));
    ASSERT_TRUE(b);
    ASSERT_TRUE(cur.parseBool(b));
    ASSERT_FALSE(b);
}

TEST(JsonCursor, ParseFloatArray)
{
    JsonCursor cur = cursorOver("[1, 2.5 ,-3]");
    f32 v[3] = {};
    ASSERT_TRUE(cur.parseFloatArray(v, 3));
    ASSERT_TRUE(floatNear(v[0], 1.0f));
    ASSERT_TRUE(floatNear(v[1], 2.5f));
    ASSERT_TRUE(floatNear(v[2], -3.0f));
    ASSERT_EQ(cur.pos, cur.len);
}

TEST(JsonCursor, ParseFailuresReturnFalse)
{
    JsonCursor cur = cursorOver("]");
    String s;
    f32 f = 0.0f;
    ASSERT_FALSE(cur.parseString(s));
    ASSERT_FALSE(cur.parseFloat(f));
    ASSERT_FALSE(cur.consume('{'));
}

TEST(JsonCursor, ReadMiniManifest)
{
    // Same access pattern as the cooked scene manifest reader.
    JsonCursor cur = cursorOver("{\"entities\":[{\"mesh\":\"a.emesh\",\"transform\":[1,0,0,0,0,1,0,0,0,0,1,0,5,6,7,1]}]}");
    String key;
    ASSERT_TRUE(cur.consume('{'));
    ASSERT_TRUE(cur.parseString(key));
    ASSERT_TRUE(key == "entities");
    ASSERT_TRUE(cur.consume(':'));
    ASSERT_TRUE(cur.consume('['));

    ASSERT_TRUE(cur.consume('{'));
    ASSERT_TRUE(cur.parseString(key));
    ASSERT_TRUE(key == "mesh");
    ASSERT_TRUE(cur.consume(':'));
    String mesh;
    ASSERT_TRUE(cur.parseString(mesh));
    ASSERT_TRUE(mesh == "a.emesh");
    ASSERT_TRUE(cur.consume(','));

    ASSERT_TRUE(cur.parseString(key));
    ASSERT_TRUE(key == "transform");
    ASSERT_TRUE(cur.consume(':'));
    f32 m[16] = {};
    ASSERT_TRUE(cur.parseFloatArray(m, 16));
    ASSERT_TRUE(floatNear(m[12], 5.0f));
    ASSERT_TRUE(floatNear(m[13], 6.0f));
    ASSERT_TRUE(floatNear(m[14], 7.0f));
    ASSERT_TRUE(cur.consume('}'));

    ASSERT_TRUE(cur.consume(']'));
    ASSERT_TRUE(cur.consume('}'));
    ASSERT_EQ(cur.pos, cur.len);
}
