#ifndef BAKE_TEST_H
#define BAKE_TEST_H

/* This generated file contains includes for project dependencies */
#include "bake-test/bake_config.h"
#include "bake-test/cdiff.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bake_test_case {
    const char *id;
    void (*function)(void);
} bake_test_case;

typedef struct bake_test_suite {
    const char *id;
    void (*setup)(void);
    void (*teardown)(void);
    uint32_t testcase_count;
    bake_test_case *testcases;
    uint32_t assert_count;
} bake_test_suite;

BAKE_TEST_EXPORT
int bake_test_run(
    const char *test_id,
    int argc, 
    char *argv[], 
    bake_test_suite *suites,
    uint32_t suite_count);

BAKE_TEST_EXPORT
void _test_assert(
    bool cond,
    const char *cond_str,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void _test_int(
    int64_t v1,
    int64_t v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void _test_uint(
    uint64_t v1,
    uint64_t v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line);    

BAKE_TEST_EXPORT
void _test_bool(
    bool v1,
    bool v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void _test_flt(
    double v1,
    double v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void _test_str(
    const char *v1,
    const char *v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void _test_null(
    void *v,
    const char *str_v,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void _test_not_null(
    void *v,
    const char *str_v,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void _test_ptr(
    const void *v1,
    const void *v2,
    const char *str_v1,
    const char *str_v2,
    const char *file,
    int line);

BAKE_TEST_EXPORT
void test_quarantine(const char *date);

BAKE_TEST_EXPORT
void test_expect_abort(void);

/* On platforms that do not support proper signal handling
 * (read: Windows) a test may replace abort with this function
 * if the library under test allows for it. */
BAKE_TEST_EXPORT
void test_abort();

#define test_assert(cond) _test_assert(cond, #cond, __FILE__, __LINE__)
#define test_bool(v1, v2) _test_bool(v1, v2, #v1, #v2, __FILE__, __LINE__)
#define test_int(v1, v2) _test_int(v1, v2, #v1, #v2, __FILE__, __LINE__)
#define test_uint(v1, v2) _test_uint(v1, v2, #v1, #v2, __FILE__, __LINE__)
#define test_flt(v1, v2) _test_flt(v1, v2, #v1, #v2, __FILE__, __LINE__)
#define test_str(v1, v2) _test_str(v1, v2, #v1, #v2, __FILE__, __LINE__)
#define test_null(v) _test_null(v, #v, __FILE__, __LINE__)
#define test_not_null(v) _test_not_null(v, #v, __FILE__, __LINE__)
#define test_ptr(v1, v2) _test_ptr(v1, v2, #v1, #v2, __FILE__, __LINE__)

#ifdef __cplusplus
}
#endif

#endif
