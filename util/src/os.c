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

char* ut_hostname(void) {
    char buff[256];
    gethostname(buff, sizeof(buff));
    return ut_setThreadString(buff);
}

bool ut_os_match(
    const char *os)
{
#if defined(__MINGW32__)
    if (!stricmp(os, "windows")) {
        return true;
    } else if (!stricmp(os, "msys")) {
        return true;
    }
#endif

    if (!stricmp(os, UT_OS_STRING) ||

#if defined(__i386__) || defined(_M_IX86)
        !stricmp(os, UT_OS_STRING "-x86") ||
        !stricmp(os, UT_OS_STRING "-i386") ||
        !stricmp(os, UT_OS_STRING "-i686") ||
        !stricmp(os, "x86-" UT_OS_STRING) ||
        !stricmp(os, "i386-" UT_OS_STRING) ||
        !stricmp(os, "i686-" UT_OS_STRING) ||
        !stricmp(os, "x86") ||
        !stricmp(os, "i386") ||
        !stricmp(os, "i686"))

#elif defined(__x86_64__) || defined(_M_X64)
        !stricmp(os, UT_OS_STRING "-amd64") ||
        !stricmp(os, UT_OS_STRING "-x64") ||
        !stricmp(os, UT_OS_STRING "-x86_64") ||
        !stricmp(os, UT_OS_STRING "-x86-64") ||
        !stricmp(os, "amd64-" UT_OS_STRING) ||
        !stricmp(os, "x64-" UT_OS_STRING) ||
        !stricmp(os, "x86-64-" UT_OS_STRING) ||
        !stricmp(os, "x86_64-" UT_OS_STRING) ||
        !stricmp(os, "amd64") ||
        !stricmp(os, "x64") ||
        !stricmp(os, "x86-64") ||
        !stricmp(os, "x86_64"))

#elif defined(__aarch64__)
        !stricmp(os, UT_OS_STRING "-arm8") ||
        !stricmp(os, UT_OS_STRING "-arm64") ||
        !stricmp(os, "arm64-" UT_OS_STRING) ||
        !stricmp(os, "arm8-" UT_OS_STRING) ||
        !stricmp(os, "arm64") ||
        !stricmp(os, "arm8"))

#elif defined(__arm__)
        !stricmp(os, UT_OS_STRING "-arm") ||
        !stricmp(os, UT_OS_STRING "-arm7l") ||
        !stricmp(os, "arm-" UT_OS_STRING) ||
        !stricmp(os, "arm7l-" UT_OS_STRING) ||
        !stricmp(os, "arm") ||
        !stricmp(os, "arm7l"))

#elif defined(__wasm32__)
        !stricmp(os, UT_OS_STRING "-wasm") ||
        !stricmp(os, UT_OS_STRING "-wasm32") ||
        !stricmp(os, "wasm-" UT_OS_STRING) ||
        !stricmp(os, "wasm32-" UT_OS_STRING) ||
        !stricmp(os, "wasm") ||
        !stricmp(os, "wasm32"))

#elif defined(__wasm64__)
        !stricmp(os, UT_OS_STRING "-wasm") ||
        !stricmp(os, UT_OS_STRING "-wasm64") ||
        !stricmp(os, "wasm-" UT_OS_STRING) ||
        !stricmp(os, "wasm64-" UT_OS_STRING) ||
        !stricmp(os, "wasm") ||
        !stricmp(os, "wasm64"))

#endif
    {
        return true;
    } else {
        return false;
    }
}
