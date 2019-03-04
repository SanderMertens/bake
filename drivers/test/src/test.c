
#include <include/test.h>

static
int bake_test_run_single_test(
    bake_test_suite *suites,
    uint32_t suite_count,
    const char *testcase_id)
{
    char *test_id = ut_strdup(testcase_id);
    char *suite_id = NULL;
    char *case_id = strrchr(test_id, '.');

    bool found = false;

    if (case_id) {
        suite_id = test_id;
        case_id[0] = '\0';
        case_id ++;
    } else {
        case_id = test_id;
    }

    int i, t;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];

        if (suite_id && strcmp(suite_id, suite->id)) {
            continue;
        }

        for (t = 0; t < suite->testcase_count; t ++) {
            bake_test_case *test = &suite->testcases[t];

            if (!strcmp(test->id, case_id)) {
                test->function();
                found = true;
            }
        }
    }

    if (!found) {
        fprintf(stderr, "testcase '%s' not found", testcase_id);
        return -1;
    }

    return 0;
}

static
int bake_test_run_all_tests(
    const char *exec,
    bake_test_suite *suites,
    uint32_t suite_count)
{
    int result = 0;

    int i, t;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];

        for (t = 0; t < suite->testcase_count; t ++) {
            bake_test_case *test = &suite->testcases[t];

            char *test_id = ut_asprintf("%s.%s", suite->id, test->id);

            ut_proc proc = ut_proc_run(exec, (const char*[]){
                exec, 
                test_id, 
                NULL
            });

            int8_t rc;
            int sig = ut_proc_wait(proc, &rc);
            if (sig || rc) {
                if (sig) {
                    fprintf(stderr, 
                        "Testcase '%s' crashed with signal %d\n", test_id, sig);
                    result = -1;
                } else {
                    fprintf(stderr, 
                        "Testcase '%s' failed with return code %d\n", test_id, rc);
                    result = -1;
                }
            }
        }
    }

    return result;
}

int bake_test_run(
    const char *test_id,
    int argc, 
    char *argv[], 
    bake_test_suite *suites,
    uint32_t suite_count)
{
    if (argc > 1) {
        return bake_test_run_single_test(suites, suite_count, argv[1]);
    } else {
        return bake_test_run_all_tests(argv[0], suites, suite_count);
    }

    return 0;
}
