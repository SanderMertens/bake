
#include <include/test.h>

static bake_test_suite *current_testsuite;
static bake_test_case *current_testcase;

static
void test_empty(void)
{
    if (!current_testsuite) {
        ut_error("test_empty called but no testsuite is running!");
        abort();
    }

    if (!current_testcase) {
        ut_error("test_empty called but no testcase is running!");
        abort();
    }

    ut_raise();

    ut_log("#[yellow]EMPTY#[reset] %s.%s (add test statements)\n", 
        current_testsuite->id, current_testcase->id);

    exit(1);
}

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
                current_testsuite = suite;
                current_testcase = test;
                suite->assert_count = 0;
                test->function();
                if (!suite->assert_count) {
                    test_empty();
                }
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
void print_dbg_command(
    const char *exec,
    const char *testcase) 
{
    printf("  To run/debug your test, do:\n");
    ut_log("    export $(bake env)#[reset]\n");
    ut_log("    %s %s#[reset]\n", exec, testcase);
    printf("\n");
}

static
void bake_test_report(
    const char *test_id,
    const char *suite_id,
    uint32_t fail,
    uint32_t empty,
    uint32_t pass)
{
    if (!pass && !fail && !empty) {
        printf("    : no testcases to run     (%s.%s)\n", test_id, suite_id);
    } else {
        if (fail) {
            if (empty) {
                ut_log("#[]PASS:%3d, #[red]FAIL#[normal]:%3d, #[yellow]EMPTY#[normal]:%3d (%s.%s)\n",
                    pass, fail, empty, test_id, suite_id);
                } else {
                    ut_log("#[]PASS:%3d, #[red]FAIL#[normal]:%3d, EMPTY:%3d (%s.%s)\n",
                        pass, fail, empty, test_id, suite_id);
                }
        } else {
            if (empty) {
                ut_log("#[]#[green]PASS#[normal]:%3d, FAIL:%3d, #[yellow]EMPTY#[normal]:%3d (%s.%s)\n",
                    pass, fail, empty, test_id, suite_id);
            } else {
                ut_log("#[]#[green]PASS#[normal]:%3d, FAIL:%3d, EMPTY:%3d (%s.%s)\n",
                    pass, fail, empty, test_id, suite_id);
            }
        }
    }
}

static
int bake_test_run_all_tests(
    const char *test_id,
    const char *exec,
    bake_test_suite *suites,
    uint32_t suite_count)
{
    int result = 0;

    uint32_t total_fail = 0, total_empty = 0, total_pass = 0;
    uint32_t fail = 0, empty = 0, pass = 0;

    printf("\n");

    int i, t;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];

        if (i && (fail || empty)) {
            printf("\n");
        }

        fail = 0;
        empty = 0;
        pass = 0;

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
                    if (sig == 6) {
                        ut_log(
                            "#[red]FAIL#[reset] %s aborted\n", test_id);
                    } else if (sig == 11) {
                        ut_log(
                            "#[red]FAIL#[reset] %s segfaulted\n", test_id);
                    } else {
                        ut_log(
                            "#[red]FAIL#[reset] %s crashed with signal %d\n", 
                            test_id, sig);
                    }
                    result = -1;
                    fail ++;
                } else {
                    if (rc == 1) {
                        /* Testcase is empty. No action required, but print the
                         * test command on command line */
                        empty ++;
                    } else if (rc != -1) {
                        /* If return code is not -1, this was not a simple
                         * testcase failure (which already has been reported) */
                        fprintf(stderr, 
                            "Testcase '%s' failed with return code %d\n", 
                            test_id, rc);

                        result = -1;
                        fail ++;
                    } else {
                        /* Normal test failure */
                        fail ++;
                    }
                }

                ut_catch();
                print_dbg_command(exec, test_id);
            } else {
                if (ut_log_verbosityGet() <= UT_OK) {
                    ut_log("#[green]PASS#[reset] %s.%s\n", 
                        suite->id, test->id);
                }
                pass ++;
            }
        }

        bake_test_report(test_id, suite->id, fail, empty, pass);

        total_fail += fail;
        total_empty += empty;
        total_pass += pass;
    }

    printf("-----------------------------\n");
    bake_test_report(test_id, "all", total_fail, total_empty, total_pass);
    printf("\n");

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
        return bake_test_run_all_tests(test_id, argv[0], suites, suite_count);
    }

    return 0;
}

static
void test_fail(
    const char *file,
    uint32_t line,
    const char *err)
{
    if (!current_testsuite) {
        ut_error("test_fail called but no testsuite is running!");
        abort();
    }

    if (!current_testcase) {
        ut_error("test_fail called but no testcase is running!");
        abort();
    }

    ut_raise();

    ut_log("#[red]FAIL#[reset] %s.%s:%d: %s\n", 
        current_testsuite->id, current_testcase->id, line, err);

    exit(-1);
}

void _test_assert(
    bool cond,
    const char *cond_str,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;

    if (!cond) {
        char *err = ut_asprintf("assert(%s)", cond_str);
        test_fail(file, line, err);
        free(err);
    }
}

void _test_int(
    uint64_t v1,
    uint64_t v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;

    if (v1 != v2) {
        char *sv1, *sv2;
        if (isdigit(*str_v1) || (*str_v1 == '-')) {
            sv1 = strdup(str_v1);
        } else {
            sv1 = ut_asprintf("%s (%lld)", str_v1, v1);
        }

        if (isdigit(*str_v2) || (*str_v2 == '-')) {
            sv2 = strdup(str_v2);
        } else {
            sv2 = ut_asprintf("%s (%lld)", str_v2, v2);
        }

        char *msg = ut_asprintf("%s != %s", sv1, sv2);
        test_fail(file, line, msg);
        free(msg);
        free(sv1);
        free(sv2);
    }
}

void _test_flt(
    double v1,
    double v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;

    if (v1 != v2) {
        char *sv1, *sv2;
        if (isdigit(*str_v1) || (*str_v1 == '-')) {
            sv1 = strdup(str_v1);
        } else {
            sv1 = ut_asprintf("%s (%f)", str_v1, v1);
        }

        if (isdigit(*str_v2) || (*str_v2 == '-')) {
            sv2 = strdup(str_v2);
        } else {
            sv2 = ut_asprintf("%s (%f)", str_v2, v2);
        }

        char *msg = ut_asprintf("%s != %s", sv1, sv2);
        test_fail(file, line, msg);
        free(msg);
        free(sv1);
        free(sv2);
    }
}

void _test_str(
    const char *v1,
    const char *v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;
    
    if (v1 || v2) {
        if ((!v1 && v2) || (v1 && !v2) || strcmp(v1, v2)) {
            char *msg = NULL;

            if ((v1 && strchr(v1, '\n')) || (v2 && strchr(v2, '\n'))) {
                msg = ut_asprintf("\n%s:\n%s\n%s:\n%s\n", 
                    str_v1, v1, str_v2, v2);
            } else {
                msg = ut_asprintf("%s (\"%s\") != %s (\"%s\")", 
                    str_v1, v1, str_v2, v2);
            }

            test_fail(file, line, msg);
            free(msg);
        }
    }
}

void _test_null(
    void *v,
    const char *str_v,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;

    if (v) {
        char *msg = ut_asprintf("%s (%p) is not null", str_v);
        test_fail(file, line, msg);
        free(msg);
    }
}

void _test_not_null(
    void *v,
    const char *str_v,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;

    if (!v) {
        char *msg = ut_asprintf("%s is null", v);
        test_fail(file, line, msg);
        free(msg);
    }
}

void _test_ptr(
    void *v1,
    void *v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;

    if (v1 != v2) {
        char *msg = ut_asprintf("%s (%p) != %s (%p)", str_v1, v1, str_v2, v2);
        test_fail(file, line, msg);
        free(msg);
    }
}
