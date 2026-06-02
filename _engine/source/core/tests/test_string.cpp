#include "test/test_framework.h"
#include "core/string/string.h"
#include "core/string/string_view.h"

TEST(Base, String_SSOStr) {
    Entelechy::String s15("123456789012345");  // 15 chars
    ASSERT_TRUE(s15.isInline());
    ASSERT_EQ(s15.length(), 15u);

    Entelechy::String s16("1234567890123456"); // 16 chars
    ASSERT_FALSE(s16.isInline());
    ASSERT_EQ(s16.length(), 16u);
}

TEST(Base, String_ConstructNull) {
    Entelechy::String s(nullptr);
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.length(), 0u);
}

TEST(Base, String_ConstructFromView) {
    Entelechy::StringView sv("hello world");
    Entelechy::String s(sv);
    ASSERT_EQ(s, "hello world");
    ASSERT_EQ(s.view(), sv);
}

TEST(Base, String_CopyCtor) {
    Entelechy::String src("1234567890123456"); // heap
    Entelechy::String dst(src);
    ASSERT_EQ(dst, src);
    dst.append("x");
    ASSERT_NE(dst, src); // verify deep copy
}

TEST(Base, String_MoveCtorInline) {
    Entelechy::String src("hi");
    Entelechy::String dst(std::move(src));
    ASSERT_EQ(dst, "hi");
    ASSERT_TRUE(src.empty());
    ASSERT_EQ(src.length(), 0u);
}

TEST(Base, String_MoveCtorHeap) {
    Entelechy::String src("1234567890123456");
    Entelechy::String dst(std::move(src));
    ASSERT_EQ(dst, "1234567890123456");
    ASSERT_TRUE(src.empty());
    ASSERT_EQ(src.length(), 0u);
}

TEST(Base, String_AssignInline) {
    Entelechy::String s("hello");
    s = "world";
    ASSERT_EQ(s, "world");
    ASSERT_TRUE(s.isInline());
}

TEST(Base, String_AssignHeapToInline) {
    Entelechy::String s("1234567890123456"); // heap
    s = "hi";
    ASSERT_TRUE(s.isInline());
    ASSERT_EQ(s, "hi");
}

TEST(Base, String_AssignHeapToInlineExplicit) {
    Entelechy::String s("1234567890123456"); // heap
    s.assign("hello", 5);
    ASSERT_TRUE(s.isInline());
    ASSERT_EQ(s, "hello");
}

TEST(Base, String_AssignSelf) {
    Entelechy::String s("hello");
    const char* self = s.c_str();
    s = self;
    ASSERT_EQ(s, "hello");
}

TEST(Base, String_AssignSelfHeap) {
    Entelechy::String s("12345678901234567890");
    const char* self = s.c_str();
    s = self;
    ASSERT_EQ(s, "12345678901234567890");
}

TEST(Base, String_AppendInline) {
    Entelechy::String s("hello");
    s.append(" world");
    ASSERT_EQ(s, "hello world");
    ASSERT_TRUE(s.isInline());
}

TEST(Base, String_AppendHeap) {
    Entelechy::String s("12345678901234"); // 14 bytes
    s.append("xx");                        // 16 bytes -> heap
    ASSERT_EQ(s.length(), 16u);
    ASSERT_FALSE(s.isInline());
}

TEST(Base, String_AppendGrowth) {
    Entelechy::String s;
    for (int i = 0; i < 20; ++i) {
        s.append("x");
    }
    ASSERT_EQ(s.length(), 20u);
    ASSERT_FALSE(s.isInline());
}

TEST(Base, String_AppendChar) {
    Entelechy::String s("hi");
    s.append('!');
    ASSERT_EQ(s, "hi!");
}

TEST(Base, String_ClearReuse) {
    Entelechy::String s("1234567890123456");
    s.clear();
    ASSERT_TRUE(s.empty());
    s.append("short");
    ASSERT_TRUE(s.isInline());
}

TEST(Base, String_Reserve) {
    Entelechy::String s;
    s.reserve(200);
    ASSERT_TRUE(s.capacity() >= 200u);
    ASSERT_TRUE(s.empty());
    for (int i = 0; i < 100; ++i) {
        s.append("x");
    }
    ASSERT_EQ(s.length(), 100u);
}

TEST(Base, String_Find) {
    Entelechy::String s("hello world");
    ASSERT_EQ(s.find("world"), 6u);
    ASSERT_EQ(s.find("xyz"), Entelechy::String::npos);
    ASSERT_EQ(s.find('o'), 4u);
    ASSERT_EQ(s.find("o", 5), 7u);
}

TEST(Base, String_FindView) {
    Entelechy::String s("hello world");
    ASSERT_EQ(s.find(Entelechy::StringView("world")), 6u);
}

TEST(Base, String_Substr) {
    Entelechy::String s("hello world");
    auto sub = s.substr(6, 5);
    ASSERT_EQ(sub, "world");
}

TEST(Base, String_StartsEndsWith) {
    Entelechy::String s("hello world");
    ASSERT_TRUE(s.startsWith("hello"));
    ASSERT_TRUE(s.endsWith("world"));
    ASSERT_TRUE(s.startsWith(Entelechy::StringView("hello")));
    ASSERT_TRUE(s.endsWith(Entelechy::StringView("world")));
    ASSERT_FALSE(s.startsWith("world"));
    ASSERT_FALSE(s.endsWith("hello"));
}

TEST(Base, String_Hash) {
    Entelechy::String a("test");
    Entelechy::String b("test");
    std::hash<Entelechy::String> hasher;
    ASSERT_EQ(hasher(a), hasher(b));
}

TEST(Base, String_PlusOperator) {
    Entelechy::String a("hello");
    Entelechy::String b(" world");
    auto c = a + b;
    ASSERT_EQ(c, "hello world");
}

// ------------------------------------------------------------------
// StringView tests
// ------------------------------------------------------------------

TEST(Base, StringView_Construct) {
    Entelechy::StringView sv("hello");
    ASSERT_EQ(sv.length(), 5u);
    ASSERT_EQ(sv, "hello");
}

TEST(Base, StringView_FromString) {
    Entelechy::String s("hello");
    Entelechy::StringView sv(s);
    ASSERT_EQ(sv, "hello");
    ASSERT_EQ(sv.length(), 5u);
}

TEST(Base, StringView_Substr) {
    Entelechy::StringView sv("hello world");
    auto sub = sv.substr(6, 5);
    ASSERT_EQ(sub, "world");
}

TEST(Base, StringView_Find) {
    Entelechy::StringView sv("hello world");
    ASSERT_EQ(sv.find('o'), 4u);
    ASSERT_EQ(sv.find("world"), 6u);
    ASSERT_EQ(sv.find("xyz"), Entelechy::StringView::npos);
    ASSERT_EQ(sv.findLast('l'), 9u);
}

TEST(Base, StringView_StartsEndsWith) {
    Entelechy::StringView sv("hello world");
    ASSERT_TRUE(sv.startsWith("hello"));
    ASSERT_TRUE(sv.endsWith("world"));
    ASSERT_TRUE(sv.startsWith(Entelechy::StringView("hello")));
    ASSERT_TRUE(sv.endsWith(Entelechy::StringView("world")));
}

TEST(Base, StringView_Equality) {
    Entelechy::StringView a("test");
    Entelechy::StringView b("test");
    Entelechy::StringView c("other");
    ASSERT_EQ(a, b);
    ASSERT_NE(a, c);
}

TEST(Base, StringView_Hash) {
    Entelechy::StringView a("test");
    Entelechy::StringView b("test");
    std::hash<Entelechy::StringView> hasher;
    ASSERT_EQ(hasher(a), hasher(b));
}

TEST(Base, StringView_Empty) {
    Entelechy::StringView sv;
    ASSERT_TRUE(sv.empty());
    ASSERT_EQ(sv.length(), 0u);
}
