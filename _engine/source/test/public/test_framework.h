#pragma once
#include "core/foundation_types.h"
#include "core/container/dynamic_array.h"
#include <cstdio>

namespace Entelechy::Test
{

struct TestCase
{
    const char *suite;
    const char *name;
    const char *file;
    int line;
    void (*fn)();
};

class TestRegistry
{
public:
    static TestRegistry &instance();

    void registerTest(const TestCase &tc);
    int runAll();

    void reportFailure(const char *file, int line, const char *fmt, ...);

private:
    DynamicArray<TestCase> m_tests;
    usize m_failures = 0;
};

} // namespace Entelechy::Test

#define TEST(suite, name)                                                                                              \
    static void _entelechy_test_##suite##_##name();                                                                    \
    static struct _entelechy_test_reg_##suite##_##name                                                                 \
    {                                                                                                                  \
        _entelechy_test_reg_##suite##_##name()                                                                         \
        {                                                                                                              \
            Entelechy::Test::TestRegistry::instance().registerTest(                                                    \
                {#suite, #name, __FILE__, __LINE__, _entelechy_test_##suite##_##name});                                \
        }                                                                                                              \
    } _entelechy_test_reg_instance_##suite##_##name;                                                                   \
    static void _entelechy_test_##suite##_##name()

#define ASSERT_TRUE(expr)                                                                                              \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr))                                                                                                   \
        {                                                                                                              \
            Entelechy::Test::TestRegistry::instance().reportFailure(__FILE__, __LINE__, "ASSERT_TRUE failed: %s",      \
                                                                    #expr);                                            \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_FALSE(expr)                                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if (expr)                                                                                                      \
        {                                                                                                              \
            Entelechy::Test::TestRegistry::instance().reportFailure(__FILE__, __LINE__, "ASSERT_FALSE failed: %s",     \
                                                                    #expr);                                            \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_EQ(a, b)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!((a) == (b)))                                                                                             \
        {                                                                                                              \
            Entelechy::Test::TestRegistry::instance().reportFailure(__FILE__, __LINE__, "ASSERT_EQ failed: %s != %s",  \
                                                                    #a, #b);                                           \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)

#define ASSERT_NE(a, b)                                                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((a) == (b))                                                                                                \
        {                                                                                                              \
            Entelechy::Test::TestRegistry::instance().reportFailure(__FILE__, __LINE__, "ASSERT_NE failed: %s == %s",  \
                                                                    #a, #b);                                           \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)
