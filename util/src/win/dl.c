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

#include "../../include/util.h"

typedef void*(*dlproc)(void);

/* Link dynamic library */
ut_dl ut_dl_open(const char* file) {
    ut_dl dl;
    dl = LoadLibraryA(file);
    return dl;
}

/* Close dynamic library */
void ut_dl_close(ut_dl dl) {
    FreeLibrary(dl);
}

/* Lookup symbol in dynamic library */
void* ut_dl_sym(ut_dl dl, const char* sym) {
    FARPROC a = GetProcAddress(dl, sym);
    return (void *)a;
}

/* Lookup procedure in dynamic library */
void*(*ut_dl_proc(ut_dl dl, const char* proc))(void) {
    FARPROC a = GetProcAddress(dl, proc);
    return (void*)a;
}

/* Lookup last error */
const char* ut_dl_error(void) {
    return ut_last_win_error();
}
