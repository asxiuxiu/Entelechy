#include "test/test_framework.h"
#include <cstdarg>
#include <cstring>

namespace Entelechy::Test
{

TestRegistry &TestRegistry::instance()
{
    static TestRegistry s_instance;
    return s_instance;
}

void TestRegistry::registerTest(const TestCase &tc)
{
    m_tests.pushBack(tc);
}

void TestRegistry::reportFailure(const char *file, int line, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    printf("    [FAIL] %s:%d %s\n", file, line, buf);
    m_failures++;
}

int TestRegistry::runAll()
{
    printf("=== Entelechy Tests ===\n");
    printf("Running %zu test(s)...\n\n", m_tests.size());

    usize passed = 0;
    usize failed = 0;
    const char *currentSuite = "";

    for (usize i = 0; i < m_tests.size(); ++i)
    {
        const TestCase &tc = m_tests[i];

        if (tc.suite[0] != '\0' && std::strcmp(tc.suite, currentSuite) != 0)
        {
            currentSuite = tc.suite;
            printf("[%s]\n", currentSuite);
        }

        printf("  %s ... ", tc.name);
        fflush(stdout);

        m_failures = 0;
        tc.fn();

        if (m_failures == 0)
        {
            printf("PASS\n");
            passed++;
        }
        else
        {
            failed++;
        }
    }

    printf("\n=== Results: %zu passed, %zu failed ===\n", passed, failed);
    return static_cast<int>(failed);
}

} // namespace Entelechy::Test
