/* Copyright (c) 2010-2019 Sander Mertens
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <bake_util.h>

int8_t UT_APP_STATUS = 3;
int8_t UT_LOG_BACKTRACE = 0;

/* Lock to protect global administration related to logging framework */
ut_mutex_s ut_log_lock;
ut_mutex_s UT_LOAD_LOCK;

extern const char *ut_log_appName;

ut_tls UT_KEY_THREAD_STRING;

void ut_init(
    const char *appName)
{
    ut_log_appName = appName;

    if (ut_mutex_new(&ut_log_lock)) {
        ut_critical("failed to create mutex for logging framework");
    }

    if (ut_mutex_new(&UT_LOAD_LOCK)) {
        ut_critical("failed to create mutex for package loader");
    }

    void ut_threadStringDealloc(void *data);

    if (ut_tls_new(&UT_KEY_THREAD_STRING, ut_threadStringDealloc)) {
        ut_critical("failed to obtain tls key for thread admin");
    }

    if (ut_log_init()) {
        ut_critical("failed to initialize logging framework");
    }

    char *verbosity = ut_getenv("BAKE_VERBOSITY");
    if (verbosity) {
        if (!strcmp(verbosity, "DEBUG")) {
            ut_log_verbositySet(UT_DEBUG);
        }
        if (!strcmp(verbosity, "TRACE")) {
            ut_log_verbositySet(UT_TRACE);
        }
        if (!strcmp(verbosity, "OK")) {
            ut_log_verbositySet(UT_OK);
        }
        if (!strcmp(verbosity, "INFO")) {
            ut_log_verbositySet(UT_INFO);
        }
        if (!strcmp(verbosity, "WARNING")) {
            ut_log_verbositySet(UT_WARNING);
        }
        if (!strcmp(verbosity, "ERROR")) {
            ut_log_verbositySet(UT_ERROR);
        }
        if (!strcmp(verbosity, "CRITICAL")) {
            ut_log_verbositySet(UT_CRITICAL);
        }
        if (!strcmp(verbosity, "ASSERT")) {
            ut_log_verbositySet(UT_ASSERT);
        }
    }

    ut_log_fmt(ut_getenv("UT_LOG_FORMAT"));

    char *profile = ut_getenv("UT_LOG_PROFILE");
    if (profile && !strcmp(profile, "TRUE")) {
        ut_log_profile(true);
    }

    UT_APP_STATUS = 0;
}

void ut_deinit(void) {
    ut_tls_free();
    ut_load_deinit();
    ut_log_deinit();
}

const char* ut_appname() {
    return ut_log_appName;
}
