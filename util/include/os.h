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

#ifndef UT_OS_H
#define UT_OS_H

/* UNSTABLE API */

#ifdef __cplusplus
extern "C" {
#endif

#if INTPTR_MAX == INT32_MAX
#define UT_CPU_32BIT
#elif INTPTR_MAX == INT64_MAX
#define UT_CPU_64BIT
#else
#warning "bake is not supported on platforms which are neither 32- nor 64-bit."
#endif

#if defined(WIN32) || defined(WIN64)
#define UT_OS_WINDOWS
#elif defined(__linux__)
#define UT_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define UT_OS_OSX
#else
#warning "Unsupported operating system"
#endif

#ifdef __aarch64__
#define UT_CPU_STRING  "arm64"
#elif defined(__arm__)
#define UT_CPU_STRING  "arm"
#elif defined(__x86_64__)
#define UT_CPU_STRING  "x64"
#elif defined(__i386__)
#define UT_CPU_STRING  "x86"
#else
#error "Unsupported CPU architecture"
#endif

#ifdef UT_OS_WINDOWS
#define UT_OS_STRING "windows"
#define UT_OS_LIB_EXT ".dll"
#define UT_WINDOWS
#elif defined(UT_OS_LINUX)
#define UT_OS_STRING "linux"
#define UT_OS_LIB_EXT ".so"
#define UT_LINUX
#elif defined(UT_OS_OSX)
#define UT_OS_STRING "darwin"
#define UT_OS_LIB_EXT ".dylib"
#define UT_MACOS
#define UT_LINUX
#endif

#define UT_PLATFORM_STRING UT_CPU_STRING "-" UT_OS_STRING

#if defined(_WIN32) 
#define PATH_SEPARATOR "\\"
#define PATH_SEPARATOR_C '\\'
#define LIB_PREFIX ""
#else 
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_C '/'
#define LIB_PREFIX "lib"
#endif 

/* Get hostname of current machine */
UT_EXPORT
char* ut_hostname(void);

/** Test whether string matches with current operating system.
 * This function tests for the most common occurances to denotate operating
 * systems and cpu architectures.
 *
 * @param operating system identifier.
 * @return true if matches, false if no match.
 */
UT_EXPORT
bool ut_os_match(const char *os);

#ifdef __cplusplus
}
#endif

#endif
