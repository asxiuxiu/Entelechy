#include "test_framework.h"
#include "small_string.h"

TEST(Base, SmallStringConstructAndCompare) {
    Entelechy::SmallString s1("hello");
    Entelechy::SmallString s2("world");
    Entelechy::SmallString s3("hello");

    ASSERT_EQ(s1, s3);
    ASSERT_NE(s1, s2);
    ASSERT_EQ(s1.length(), 5u);
    ASSERT_TRUE(s1.startsWith("he"));
    ASSERT_TRUE(s1.endsWith("lo"));
}
