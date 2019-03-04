#ifndef BAKE_TEST_H
#define BAKE_TEST_H

/* This generated file contains includes for project dependencies */
#include "bake_config.h"
#include "cdiff.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bake_test_case {
    const char *id;
    void (*function)(void);
} bake_test_case;

typedef struct bake_test_suite {
    const char *id;
    uint32_t testcase_count;
    bake_test_case *testcases;
} bake_test_suite;

int bake_test_run(
    const char *test_id,
    int argc, 
    char *argv[], 
    bake_test_suite *suites,
    uint32_t suite_count);

#ifdef __cplusplus
}
#endif

#endif

