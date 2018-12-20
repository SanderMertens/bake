/* Copyright (c) 2010-2018 Sander Mertens
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

#include "../include/util.h"

static
bool ut_version_validate(
    ut_version *version)
{
    if (version->major < 0) {
        return false;
    } else if (version->minor < 0) {
        if (version->patch >= 0) {
            /* If no minor version is provided, patch version cannot be provided
             * either */
            return false;
        }
    }

    return true;
}

int16_t ut_version_parse(
    const char *version_string,
    ut_version *result)
{
    uint32_t pw = 3;
    const char *ptr;

    if (!result) {
        ut_throw("no value provided for version_out");
        goto error;
    }

    result->major = -1;
    result->minor = -1;
    result->patch = -1;

    for (ptr = version_string; ptr && *ptr; ptr = strchr(ptr, '.')) {
        if (ptr[0] == '.') {
            ptr ++;
        }

        if (!isdigit(*ptr)) {
            ut_throw("'%s' is not a valid version string");
            goto error;
        }

        if (pw == 3) result->major = atoi(ptr);
        else if (pw == 2) result->minor = atoi(ptr);
        else result->patch = atoi(ptr);
        pw --;
        if (!pw) break; /* max 3 numbers in a version */
    }

    return 0;
error:
    return -1;
}

char *ut_version_str(
    ut_version *version)
{
    if (ut_version_validate(version)) {
        if (version->minor >= 0) {
            if (version->patch >= 0) {
                return ut_asprintf(
                    "%u.%u.%u", version->major, version->minor, version->patch);
            } else {
                return ut_asprintf(
                    "%u.%u", version->major, version->minor);
            }
        } else {
            return ut_asprintf("%u", version->major);
        }
    }

    return NULL;
}

int ut_version_cmp(
    ut_version *v1,
    ut_version *v2)
{
    if (v2->major > v1->major) {
        return 1;
    } else if (v2->major < v1->major) {
        return -1;
    } else {
        if (v2->minor > v1->minor) {
            return 2;
        } else if (v2->minor < v1->minor) {
            return -2;
        } else {
            if (v2->patch > v1->patch) {
                return 3;
            } else if (v2->patch < v1->patch) {
                return -3;
            } else {
                return 0;
            }
        }
    }
}

int ut_version_strcmp(
    const char *v1,
    const char *v2)
{
    ut_version version_1, version_2;

    if (ut_version_parse(v1, &version_1)) {
        return -1;
    }
    if (ut_version_parse(v2, &version_2)) {
        return 1;
    }

    return ut_version_cmp(&version_1, &version_2);
}

char* ut_version_inc(
    const char *old_version,
    ut_version_kind kind)
{
    ut_version version;
    ut_try( ut_version_parse(old_version, &version), NULL);

    if (kind == UT_VERSION_PATCH) {
        version.patch ++;
    } else if (kind == UT_VERSION_MINOR) {
        version.minor ++;
        version.patch = 0;
    } else if (kind == UT_VERSION_MAJOR) {
        version.major ++;
        version.minor = 0;
        version.patch = 0;
    }

    return ut_version_str(&version);
error:
    return NULL;
}
