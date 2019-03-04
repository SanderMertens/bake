
#include <include/test.h>

int bake_test_run(
    const char *test_id,
    int argc, 
    char *argv[], 
    bake_test_suite *suites,
    uint32_t suite_count)
{
    int i, t;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];

        for (t = 0; t < suite->testcase_count; t ++) {
            bake_test_case *test = &suite->testcases[t];
            test->function();
        }
    }

    return 0;
}