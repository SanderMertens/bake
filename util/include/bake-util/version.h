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

#ifndef UT_VERSION_H_
#define UT_VERSION_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ut_version {
    int32_t major;
    int32_t minor;
    int32_t patch;
} ut_version;

typedef enum ut_version_kind {
    UT_VERSION_MAJOR = 1,
    UT_VERSION_MINOR = 2,
    UT_VERSION_PATCH = 3
} ut_version_kind;

UT_API
int16_t ut_version_parse(
    const char *version_string,
    ut_version *version_out);

UT_API
char *ut_version_str(
    ut_version *version);

UT_API
int ut_version_cmp(
    ut_version *v1,
    ut_version *v2);

UT_API
int ut_version_strcmp(
    const char *v1,
    const char *v2);

UT_API
char* ut_version_inc(
    const char *old_version,
    ut_version_kind kind);

#ifdef __cplusplus
}
#endif

#endif
