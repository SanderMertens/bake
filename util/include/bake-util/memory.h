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

#ifndef UT_MEMORY_H_
#define UT_MEMORY_H_

/* UNSTABLE API */

#ifdef __cplusplus
extern "C" {
#endif

/* Mark variable(parameter) as unused */
#define UT_UNUSED(p) (void)(p)

/* Calculate offset */
#define UT_OFFSET(o, offset) (void*)(((uintptr_t)(o)) + ((uintptr_t)(offset)))

/* Determine alignment of struct */
#define UT_ALIGNMENT(t) ((uintptr_t)(&((struct {ut_char dummy;t alignMember;}*)(void *)0)->alignMember))

/* Determine offset based on size of type and alignment */
#define UT_ALIGN(size, alignment) (((((size) - 1) / (alignment)) + 1) * (alignment))

/* Hash for primitive typekinds. Enables quick lookup of transformations and operators for primitive types. */
#define UT_PWIDTH(width) (((width) == UT_WIDTH_8) ? 0 : ((width) == UT_WIDTH_16) ? 1 : ((width) == UT_WIDTH_32) ? 2 : ((width) == UT_WIDTH_64) ? 3 : -1)
#define UT_TYPEHASH_VARWIDTH(kind, width) ((kind) * (UT_FLOAT+1)) + ((int[4]){0, UT_FLOAT, UT_FLOAT * 2, UT_FLOAT * 3}[UT_PWIDTH(width)])
#define UT_TYPEHASH(kind, width) ((kind) <= UT_FLOAT ? UT_TYPEHASH_VARWIDTH((kind), (width)) : UT_TYPEHASH_VARWIDTH(UT_FLOAT, UT_WIDTH_64) + ((kind) - UT_FLOAT))
#define UT_TYPEHASH_MAX (50)

#define UT_ISNAN(x) ((x) != (x))

/* Macros for memory allocation functions */
#define ut_calloc(n) calloc(n, 1)

/* Set intern TLS string */
UT_API char* ut_setThreadString(char* string);

UT_API char* ut_itoa(int num, char* buff);
UT_API char* ut_ulltoa(uint64_t value, char *ptr, int base);

#ifdef __cplusplus
}
#endif

#endif /* UT_UTIL_H_ */
