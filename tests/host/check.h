// Minimal host test harness: CHECK records failures, main returns non-zero.
#pragma once
#include <cstdio>

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

#define TEST(name) static void name()
#define RUN(name) (std::printf("  %s\n", #name), name())

static int report(const char *suite)
{
    std::printf("%s: %d checks, %d failures\n", suite, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
