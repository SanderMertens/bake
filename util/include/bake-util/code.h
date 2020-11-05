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

/** @code
 * @section Source code utility API
 * @brief Utility functions for generating source code
 */

#ifndef UT_CODE_H
#define UT_CODE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ut_code {
    FILE* file;
    char *name;
    uint32_t indent;
    bool endLine; /* If last written character was a '\n', the next write must insert indentation spaces. */
} ut_code;

/* Open a file for writing. */
UT_API
ut_code* ut_code_open(
    const char *name,
    ...);

/* Close a file. */
UT_API
void ut_code_close(
    ut_code *file);

/* Increase indentation. */
UT_API
void ut_code_indent(
    ut_code *file);

/* Decrease indentation. */
UT_API
void ut_code_dedent(
    ut_code *file);

/* Write to a file. */
UT_API
int ut_code_write(
    ut_code *file,
    char* fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
