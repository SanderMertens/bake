
#include <bake_test.h>

static bake_test_suite *current_testsuite;
static bake_test_case *current_testcase;

static bool test_expect_abort_signal = false;
static bool test_flaky = false;

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

    exit(2);
}

static
void test_no_abort(void)
{
    ut_log("#[red]FAIL#[reset]: %s.%s (expected abort signal)\n", 
        current_testsuite->id, current_testcase->id);

    exit(-1);
}

static
int8_t bake_test_run_single_test(
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

    uint32_t i, t;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];

        if (suite_id && strcmp(suite_id, suite->id)) {
            continue;
        }

        for (t = 0; t < suite->testcase_count; t ++) {
            bake_test_case *test = &suite->testcases[t];

            if (!strcmp(test->id, case_id)) {
                if (suite->setup) {
                    suite->setup();
                }

                current_testsuite = suite;
                current_testcase = test;
                suite->assert_count = 0;
                test->function();

                if (test_expect_abort_signal) {
                    test_no_abort();
                }

                if (!suite->assert_count) {
                    test_empty();
                }

                found = true;

                if (suite->teardown) {
                    suite->teardown();
                }

                break;
            }
        }

        if (found) {
            break;
        }
    }

    if (!found) {
        ut_log("testcase '%s' not found\n", testcase_id);
        free(test_id);
        return -1;
    }

    free(test_id);
    return 0;
}

static
void print_dbg_command(
    const char *exec,
    const char *testcase) 
{
    ut_log("To run/debug your test, do:\n");
    ut_log("export $(bake env)#[reset]\n");
    ut_log("%s %s#[reset]\n", exec, testcase);
    ut_log("\n");
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
        ut_log("    : no testcases to run     (%s.%s)\n", test_id, suite_id);
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
bake_test_suite* bake_find_suite(
    bake_test_suite *suites,
    uint32_t suite_count,
    const char *suite_id)
{
    uint32_t i;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];
        if (!strcmp(suite->id, suite_id)) {
            return &suites[i];
        }
    }

    return NULL;
}

typedef struct {
    const char *exec;
    bake_test_suite *suite;
    uint32_t fail;
    uint32_t empty;
    uint32_t pass;
    uint32_t offset;
    uint32_t count;
    int8_t result;
    ut_thread job;
} bake_test_exec_ctx;

static
void* bake_test_run_suite_range(
    bake_test_exec_ctx *ctx)
{
    int8_t result = 0;
    uint32_t fail = 0, empty = 0, pass = 0;
    uint32_t offset = ctx->offset, count = ctx->count;
    const char *exec = ctx->exec;
    bake_test_suite *suite = ctx->suite;
    const char *prefix = ut_getenv("BAKE_TEST_PREFIX");

    uint32_t t;
    for (t = offset; t < (offset + count); t ++) {
        bake_test_case *test = &suite->testcases[t];

        char *test_name = ut_asprintf("%s.%s", suite->id, test->id);
        ut_proc proc;
        int8_t rc = 0;
        int sig = 0;
        bool proc_fail = false;
        int32_t retry_count = 0;

retry:
        memset(&proc, 0, sizeof(ut_proc));

        if (prefix) {
            char *has_space = strchr(prefix, ' ');
            if (has_space) {
                ut_strbuf cmd = UT_STRBUF_INIT;
                ut_strbuf_append(&cmd, "%s %s %s", prefix, exec, test_name);
                char *cmd_str = ut_strbuf_get(&cmd);
                sig = ut_proc_cmd(cmd_str, &rc);
                free(cmd_str);
            } else {
                proc = ut_proc_run(prefix, (const char*[]){
                    prefix,
                    exec,
                    test_name, 
                    NULL
                });

                if (proc) {
                    sig = ut_proc_wait(proc, &rc);
                } else {
                    proc_fail = true;
                }
            }               
        } else {
            proc = ut_proc_run(exec, (const char*[]){
                exec, 
                test_name, 
                NULL
            });

            if (proc) {
                sig = ut_proc_wait(proc, &rc);
            } else {
                proc_fail = true;
            }
        }

        if (sig || rc) {
            ut_catch();
            if (sig) {
                if (sig == 6) {
                    ut_log("#[red]FAIL#[reset]: %s aborted\n", test_name);
                } else if (sig == 11) {
                    ut_log("#[red]FAIL#[reset]: %s segfaulted\n", test_name);
                } else {
                    /* Signal 4 seems to get thrown every now and then when 
                     * trying to create lots of processes. Retry a few times
                     * before actually failing the test. */
                    if (sig == 4) {
                        retry_count ++;
                        if (retry_count < 5) {
                            /* Don't retry too fast in case OS resources are
                             * limited. */
                            ut_sleep(0, 100 * 1000 * 1000);
                            ut_log("#[grey]retrying after sig 4...\n");
                            goto retry;
                        } else {
                            ut_log("#[red]retried 5 times after sig 4\n");
                        }
                    }
                    ut_log("#[red]FAIL#[reset]: %s exited with signal %d\n", 
                        test_name, sig);
                }
                result = -1;
                fail ++;
            } else {
                if (rc == 2) {
                    /* Testcase is empty. No action required, but print the
                     * test command on command line */
                    empty ++;
                } else if (rc != -1) {
                    /* If return code is not -1, this was not a simple
                     * testcase failure (which already has been reported) */
                    ut_log(
                        "#[red]FAIL#[reset]: %s failed with return code %d\n", 
                        test_name, rc);

                    result = -1;
                    fail ++;
                } else {
                    /* Normal test failure */
                    result = -1;
                    fail ++;
                }
            }

            ut_catch();
            print_dbg_command(exec, test_name);
        } else if (proc_fail) {
            ut_log("Testcase '%s' failed to start\n", test_name);
        } else {
            if (ut_log_verbosityGet() <= UT_OK) {
                ut_log("#[green]PASS#[reset] %s.%s\n", 
                    suite->id, test->id);
            }
            pass ++;
        }

        free(test_name);
    }

    ctx->fail = fail;
    ctx->empty = empty;
    ctx->pass = pass;
    
    ctx->result = result;

    return 0;
}

static
int8_t bake_test_run_suite(
    const char *test_id,
    const char *exec,
    bake_test_suite *suite,
    uint32_t *fail_out,
    uint32_t *empty_out,
    uint32_t *pass_out,
    uint32_t job_count)
{
    assert(test_id != NULL);
    assert(exec != NULL);
    assert(strlen(exec) != 0);

    uint32_t i, cur = 0;
    float next = 0, tests_per_runner = 
        (float)suite->testcase_count / (float)job_count;

    bake_test_exec_ctx *ctx = ut_calloc(
        sizeof(bake_test_exec_ctx) * job_count);
    assert(ctx != NULL);

    // Divide the work
    for (i = 0; i < job_count; i ++) {
        next += tests_per_runner;
        ctx[i].exec = exec;
        ctx[i].suite = suite;
        ctx[i].offset = cur;
        ctx[i].count = next - cur;
        
        if (i == (job_count - 1)) {
            ctx[i].count += suite->testcase_count - ((uint32_t)next);
            assert(cur + ctx[i].count == suite->testcase_count);
        }

        cur += ctx[i].count;
    }

    // Run jobs
    for (i = 0; i < job_count; i ++) {
        ctx[i].job = ut_thread_new(
            (ut_thread_cb)bake_test_run_suite_range, &ctx[i]);
    }

    // Wait for jobs to complete
    for (i = 0; i < job_count; i ++) {
        ut_thread_join(ctx[i].job, NULL);
    }

    // Collect results
    for (i = 1; i < job_count; i ++) {
        ctx->pass += ctx[i].pass;
        ctx->fail += ctx[i].fail;
        ctx->empty += ctx[i].empty;
        ctx->result |= ctx[i].result;
    }

    // Report
    bake_test_report(test_id, suite->id, ctx->fail, ctx->empty, ctx->pass);

    int8_t result = ctx->result;
    if (fail_out) {
        *fail_out = ctx->fail;
    }
    if (empty_out) {
        *empty_out = ctx->empty;
    }
    if (pass_out) {
        *pass_out = ctx->pass;
    }

    free(ctx);

    return result;
}

static
int8_t bake_test_run_all_tests(
    const char *test_id,
    const char *exec,
    bake_test_suite *suites,
    uint32_t suite_count,
    uint32_t job_count)
{
    int8_t result = 0;

    uint32_t total_fail = 0, total_empty = 0, total_pass = 0;
    uint32_t fail = 0, empty = 0, pass = 0;

    ut_log("\n");

    uint32_t i;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];

        fail = 0;
        empty = 0;
        pass = 0;

        if (bake_test_run_suite(
            test_id, exec, suite, &fail, &empty, &pass, job_count)) 
        {
            result = -1;
        }

        if (empty || fail) {
            ut_log("\n");
        }

        total_fail += fail;
        total_empty += empty;
        total_pass += pass;
    }

    ut_log("-----------------------------\n");
    bake_test_report(test_id, "all", total_fail, total_empty, total_pass);
    ut_log("\n");

    return result;
}

static
void bake_list_tests(
    bake_test_suite *suites,
    uint32_t suite_count)
{
    uint32_t i, t;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];
        for (t = 0; t < suite->testcase_count; t ++) {
            printf("%s.%s\n", suite->id, suite->testcases[t].id);
        }
    }
}

static
void bake_list_suites(
    bake_test_suite *suites,
    uint32_t suite_count)
{
    uint32_t i;
    for (i = 0; i < suite_count; i ++) {
        printf("%s\n", suites[i].id);
    }
}

static
void bake_list_commands(
    const char *exec,
    bake_test_suite *suites,
    uint32_t suite_count)
{
    uint32_t i, t;
    for (i = 0; i < suite_count; i ++) {
        bake_test_suite *suite = &suites[i];
        for (t = 0; t < suite->testcase_count; t ++) {
            printf("%s %s.%s\n", exec, suite->id, suite->testcases[t].id);
        }
    }
}

int bake_test_run(
    const char *test_id,
    int argc, 
    char *argv[], 
    bake_test_suite *suites,
    uint32_t suite_count)
{
    const char *single_test = NULL;
    bake_test_suite *suite = NULL;
    int32_t job_count = 0;

    ut_init(test_id);

    for (int i = 1; i < argc; i ++) {
        char *arg = argv[i];

        if (arg[0] == '-') {
            if (arg[1] == '-') {
                if (!strcmp(arg, "--list-tests")) {
                    bake_list_tests(suites, suite_count);

                } else if (!strcmp(arg, "--list-suites")) {
                    bake_list_suites(suites, suite_count);

                } else if (!strcmp(arg, "--list-commands")) {
                    bake_list_commands(argv[0], suites, suite_count);

                } else {
                    ut_error("invalid argument for test executable", arg);
                    abort();
                }
            } else if (arg[1] == 'j') {
                if (argc < (i + 2)) {
                    ut_error("expected argument after -j");
                    abort();
                }

                job_count = atoi(argv[i + 1]);
                i ++;
            }

        /* Quick & dirty way to test if arg contains testcase name */
        } else if (strchr(arg, '.')) {
            single_test = argv[1];
        } else {
            suite = bake_find_suite(suites, suite_count, arg);
            if (!suite) {
                ut_error("test suite '%s' not found", arg);
                abort();
            }
        }
    }

    if (!job_count) {
        job_count = 8; /* run on 8 threads by default */
    }

    int result = 0;

    if (single_test) {
        result = bake_test_run_single_test(suites, suite_count, argv[1]);
    } else if (suite) {
        result = bake_test_run_suite(
            test_id, argv[0], suite, NULL, NULL, NULL, job_count);
    } else {
        result = bake_test_run_all_tests(
            test_id, argv[0], suites, suite_count, job_count);
    }

    ut_deinit();

    return result;
}

static
void test_fail(
    const char *file,
    int32_t line,
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

    if (!test_flaky) {
        ut_log("#[red]FAIL#[reset]: %s.%s:%d: %s\n", 
            current_testsuite->id, current_testcase->id, line, err);
    } else {
        ut_log("#[yellow]FLAKY#[reset]: %s.%s:%d: %s\n", 
            current_testsuite->id, current_testcase->id, line, err);
    }
}

static 
void test_exit(void) {
    exit(!test_flaky * -1);
}

bool _if_test_assert(
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
        return false;
    }

    return true;
}

bool _if_test_bool(
    bool v1,
    bool v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    current_testsuite->assert_count ++;

    if (v1 && v1 != true) {
        char *msg = ut_asprintf("%s has invalid value for bool (%lld)", str_v1, v1);
        test_fail(file, line, msg);
        free(msg);
        return false;
    }

    if (v2 && v2 != true) {
        char *msg = ut_asprintf("%s has invalid value for bool (%lld)", str_v2, v2);
        test_fail(file, line, msg);
        free(msg);
        return false;
    }

    if (v1 != v2) {
        char *sv1, *sv2;
        if (isdigit(*str_v1) || (*str_v1 == '-')) {
            sv1 = strdup(str_v1);
        } else {
            sv1 = ut_asprintf("%s (%s)", str_v1, v1 ? "true" : "false");
        }

        if (isdigit(*str_v2) || (*str_v2 == '-')) {
            sv2 = strdup(str_v2);
        } else {
            sv2 = ut_asprintf("%s (%s)", str_v2, v2 ? "true" : "false");
        }

        char *msg = ut_asprintf("%s != %s", sv1, sv2);
        test_fail(file, line, msg);
        free(msg);
        free(sv1);
        free(sv2);
        return false;
    }

    return true;
}

bool _if_test_int(
    int64_t v1,
    int64_t v2,
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
        return false;
    }

    return true;
}

bool _if_test_uint(
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
            sv1 = ut_asprintf("%s (%llu)", str_v1, v1);
        }

        if (isdigit(*str_v2) || (*str_v2 == '-')) {
            sv2 = strdup(str_v2);
        } else {
            sv2 = ut_asprintf("%s (%llu)", str_v2, v2);
        }

        char *msg = ut_asprintf("%s != %s", sv1, sv2);
        test_fail(file, line, msg);
        free(msg);
        free(sv1);
        free(sv2);
        return false;
    }

    return true;
}

bool _if_test_flt(
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
        return false;
    }

    return true;
}

bool _if_test_str(
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

            msg = ut_asprintf("\n"
                "arg 1: %s\n"
                "arg 2: %s\n", v1, v2);

            test_fail(file, line, msg);
            free(msg);
            return false;
        }
    }

    return true;
}

bool _if_test_null(
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
        return false;
    }

    return true;
}

bool _if_test_not_null(
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
        return false;
    }

    return true;
}

bool _if_test_ptr(
    const void *v1,
    const void *v2,
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
        return false;
    }
    return true;
}

void _test_assert(
    bool cond,
    const char *cond_str,
    const char *file,
    int line)
{
    if (!_if_test_assert(cond, cond_str, file, line))
        test_exit();
}

void _test_bool(
    bool v1,
    bool v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    if (!_if_test_bool(v1, v2, str_v1, str_v2, file, line))
        test_exit();
}

void _test_int(
    int64_t v1,
    int64_t v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    if (!_if_test_int(v1, v2, str_v1, str_v2, file, line))
        test_exit();
}

void _test_uint(
    uint64_t v1,
    uint64_t v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    if (!_if_test_uint(v1, v2, str_v1, str_v2, file, line))
        test_exit();
}

void _test_flt(
    double v1,
    double v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    if (!_if_test_flt(v1, v2, str_v1, str_v2, file, line))
        test_exit();
}

void _test_str(
    const char *v1,
    const char *v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    if (!_if_test_str(v1, v2, str_v1, str_v2, file, line))
        test_exit();
}

void _test_null(
    void *v,
    const char *str_v,
    const char *file,
    int line)
{
    if (!_if_test_null(v, str_v, file, line))
        test_exit();
}

void _test_not_null(
    void *v,
    const char *str_v,
    const char *file,
    int line)
{
    if (!_if_test_not_null(v, str_v, file, line))
        test_exit();
}

void _test_ptr(
    const void *v1,
    const void *v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line)
{
    if (!_if_test_ptr(v1, v2, str_v1, str_v2, file, line))
        test_exit();
}

static
void abort_handler(int sig)
{
    if (test_expect_abort_signal) {
        exit(0);
    } else {
        ut_error("bake.test: unexpected abort");
        exit(-1);
    }
}

void test_abort(void) {
    abort_handler(SIGABRT);
}

void test_expect_abort(void) {
    test_expect_abort_signal = true;
    signal(SIGABRT, &abort_handler);
}

void test_is_flaky(void) {
    test_flaky = true;
}

void test_quarantine(const char *date) {
    ut_log("#[yellow]SKIP#[reset]: %s.%s: test was quarantined on %s\n", 
        current_testsuite->id, current_testcase->id, date);
    exit(0);
}
