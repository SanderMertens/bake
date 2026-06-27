#include <check.h>
#include <stdlib.h>
#include <string.h>
#include "util/src/strbuf.h"

START_TEST(test_strbuf_append_intern_bounds)
{
    // Invariant: Buffer reads never exceed the declared length
    const char *payloads[] = {
        "%s",  // Format string that could cause overflow
        "%.1000s",  // Large width specifier
        "normal string",  // Valid input
        "%500s",  // Boundary case near typical buffer limits
        "%*s"  // Variable width format
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        ut_strbuf buf = UT_STRBUF_INIT;
        const char *test_str = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                               "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
        
        // Use vsprintf-like behavior through ut_strbuf_append
        ut_strbuf_append(&buf, payloads[i], test_str);
        
        // If we get here without crashing, the test passes
        // Additional check: verify buffer is properly terminated
        char *result = ut_strbuf_get(&buf);
        if (result) {
            ck_assert_msg(strlen(result) < 1024, 
                         "Buffer length should be bounded");
            free(result);
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_strbuf_append_intern_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}