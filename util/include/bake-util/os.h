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

#if defined(_WIN32)
#define UT_OS_WINDOWS
#elif defined(__linux__)
#define UT_OS_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
#define UT_OS_DARWIN
#elif defined(__EMSCRIPTEN__)
#define UT_OS_EMSCRIPTEN
#else
#warning "Unsupported operating system"
#endif

#if defined(__aarch64__) || defined(_M_ARM64)
#define UT_CPU_STRING  "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#define UT_CPU_STRING  "arm"
#elif defined(__x86_64__) || defined(_M_X64)
#define UT_CPU_STRING  "x64"
#elif defined(__i386__) || defined(_M_IX86)
#define UT_CPU_STRING  "x86"
#elif defined(__wasm32__)
#define UT_CPU_STRING "wasm32"
#elif defined(__wasm64__)
#define UT_CPU_STRING "wasm64"
#else
#error "Unsupported CPU architecture"
#endif

#define UT_EM_STRING "Emscripten"
#define UT_EM_STATIC_LIB_EXT ".a"
#define UT_EM_LIB_EXT UT_EM_STATIC_LIB_EXT
#define UT_EM_BIN_EXT ".js"
#define UT_EM_SCRIPT_EXT ".js"
#define UT_EM_LIB_PREFIX "lib"
#define UT_EM_PS "/"

#ifdef __MINGW32__
#define UT_OS_STRING "Mingw"
#define UT_OS_LIB_EXT ".dll"
#define UT_OS_STATIC_LIB_EXT ".lib"
#define UT_OS_BIN_EXT ".exe"
#define UT_OS_SCRIPT_EXT ".bat"
#define UT_OS_LIB_PREFIX ""
#define UT_OS_PS "\\"
#define UT_ENV_HOME "HOME"
#define UT_ENV_LIBPATH "PATH"
#define UT_ENV_BINPATH "PATH"
#define UT_ENV_PATH_SEPARATOR ";"
#define UT_GLOBAL_BIN_PATH "/usr/local/bin"
#define UT_GLOBAL_LIB_PATH "/usr/local/lib"
#define UT_LINUX
#elif defined(UT_OS_WINDOWS)
#define UT_OS_STRING "Windows"
#define UT_OS_LIB_EXT ".dll"
#define UT_OS_STATIC_LIB_EXT ".lib"
#define UT_OS_BIN_EXT ".exe"
#define UT_OS_SCRIPT_EXT ".bat"
#define UT_OS_LIB_PREFIX ""
#define UT_OS_PS "\\"
#define UT_ENV_HOME "USERPROFILE"
#define UT_ENV_LIBPATH "PATH"
#define UT_ENV_BINPATH "PATH"
#define UT_ENV_PATH_SEPARATOR ";"
#define UT_GLOBAL_BIN_PATH "C:\\Program Files\\bake"
#define UT_GLOBAL_LIB_PATH "C:\\Windows\\system32"
#define UT_WINDOWS
#elif defined(UT_OS_LINUX)
#define UT_OS_STRING "Linux"
#define UT_OS_LIB_EXT ".so"
#define UT_OS_STATIC_LIB_EXT ".a"
#define UT_OS_BIN_EXT ""
#define UT_OS_SCRIPT_EXT ".sh"
#define UT_OS_LIB_PREFIX "lib"
#define UT_OS_PS "/"
#define UT_ENV_HOME "HOME"
#define UT_ENV_LIBPATH "LD_LIBRARY_PATH"
#define UT_ENV_BINPATH "PATH"
#define UT_ENV_PATH_SEPARATOR ":"
#define UT_GLOBAL_BIN_PATH "/usr/local/bin"
#define UT_GLOBAL_LIB_PATH "/usr/local/lib"
#define UT_LINUX
#elif defined(UT_OS_DARWIN)
#define UT_OS_STRING "Darwin"
#define UT_OS_LIB_EXT ".dylib"
#define UT_OS_STATIC_LIB_EXT ".a"
#define UT_OS_BIN_EXT ""
#define UT_OS_SCRIPT_EXT ".sh"
#define UT_OS_LIB_PREFIX "lib"
#define UT_OS_PS "/"
#define UT_ENV_HOME "HOME"
#define UT_ENV_LIBPATH "LD_LIBRARY_PATH"
#define UT_ENV_DYLIBPATH "DYLD_LIBRARY_PATH"
#define UT_ENV_BINPATH "PATH"
#define UT_ENV_PATH_SEPARATOR ":"
#define UT_GLOBAL_BIN_PATH "/usr/local/bin"
#define UT_GLOBAL_LIB_PATH "/usr/local/lib"
#define UT_MACOS
#define UT_LINUX
#elif defined(UT_OS_EMSCRIPTEN)
#define UT_OS_STRING UT_EM_STRING
#define UT_OS_LIB_EXT UT_EM_LIB_EXT
#define UT_OS_STATIC_LIB_EXT UT_EM_STATIC_LIB_EXT
#define UT_OS_BIN_EXT UT_EM_BIN_EXT
#define UT_OS_SCRIPT_EXT UT_EM_SCRIPT_EXT
#define UT_OS_LIB_PREFIX UT_EM_LIB_PREFIX
#define UT_OS_PS UT_EM_PS
#define UT_ENV_HOME "HOME"
#define UT_ENV_LIBPATH "LD_LIBRARY_PATH"
#define UT_ENV_BINPATH "PATH"
#define UT_ENV_PATH_SEPARATOR ":"
#define UT_GLOBAL_BIN_PATH "/usr/local/bin"
#define UT_GLOBAL_LIB_PATH "/usr/local/lib"
#endif

#define UT_CMD_OK 0
#define UT_CMD_ERR ((int8_t)-1)

#define UT_PLATFORM_STRING UT_CPU_STRING "-" UT_OS_STRING

typedef struct ut_target {
    const char *lib_ext;       /* Target library file extension */
    const char *static_lib_ext;/* Target static library file extension */
    const char *bin_ext;       /* Target binary file extension */
    const char *script_ext;    /* Target script file extension */
    const char *lib_prefix;    /* Target library file prefix */
} ut_target;

/* Get target for current machine */
UT_API
ut_target ut_target_host(void);

/* Get target for Emscripten */
UT_API
ut_target ut_target_emscripten(void);

/* Get hostname of current machine */
UT_API
char* ut_hostname(void);

/** Test whether string matches with current operating system.
 * This function tests for the most common occurances to denotate operating
 * systems and cpu architectures.
 *
 * @param os operating system identifier.
 * @return true if matches, false if no match.
 */
UT_API
bool ut_os_match(const char *os);

#ifdef __cplusplus
}
#endif

#endif
