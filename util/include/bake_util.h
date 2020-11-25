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

/** @file
 * @section base Corto base platform definitions.
 * @brief Provide a stable platform/compiler independent set of definitions
 */

#ifndef UT_BASE_H
#define UT_BASE_H

#if UT_IMPL && defined _MSC_VER
#define UT_API __declspec(dllexport)
#elif UT_IMPL
#define UT_API __attribute__((__visibility__("default")))
#elif defined _MSC_VER
#define UT_API __declspec(dllimport)
#else
#define UT_API
#endif

#ifndef __ANDROID__
#define ENABLE_BACKTRACE
#endif

/* Standard C library */
#if defined(_WIN32)
#include <malloc.h>
#else
#include <alloca.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

/* OS-specific headers */
#if defined(_WIN32)
#include <windows.h>
#include <versionhelpers.h>
#include <shlwapi.h>
#include <stdint.h>
#else
#include <fnmatch.h>
#include <inttypes.h>

#ifdef ENABLE_BACKTRACE
#include <execinfo.h>
#endif

#include <dlfcn.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <ftw.h>
#include <fcntl.h>
#include <stdint.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#endif

/* Cpp headers */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
#define ut_fnmatch(pattern, string) fnmatch(pattern, string, 0)
#else
#define ut_fnmatch(pattern, string) !PathMatchSpecA(string, pattern)
#endif

/*
 * Configuration parameters
 *   Increasing these numbers will increase memory usage of Corto in various
 *   scenario's.
 */

/* The maximum nesting level of expressions in expr */
#define UT_MAX_SCOPE_DEPTH (64)

/* Limit length of file extensions to 16 characters */
#define UT_MAX_FILE_EXTENSION (16)

/* Maximum number of TLS keys that is supported by the core */
#define UT_MAX_THREAD_KEY (256)

/* The maximum number of TLS strings that can exist simultaneously */
#define UT_MAX_TLS_STRINGS (5)

/* Maximum retained buffer length for TLS string. This ensures that when a very
 * large string is stored in TLS, it will be cleaned up eventually. Keeping it
 * around would be wasteful since chances are low that a string of similar
 * length will take advantage of the memory. */
#define UT_MAX_TLS_STRINGS_MAX (1024)

/* Maximum number of arguments for command */
#define UT_MAX_CMD_ARGS (256)

/* Maximum number of operations in an id expression */
#define UT_EXPR_MAX_OP (32)

/* Maximum number of categories in logmsg, like: "comp1: comp2: comp3: msg" */
#define UT_MAX_LOG_CATEGORIES (24)

/* Maximum number of code frames in logmsg */
#define UT_MAX_LOG_CODEFRAMES (16)

#include <stdbool.h>

/* The API uses the native bool type in C++, or a custom one in C */
#if !defined(__cplusplus) && !defined(__bool_true_false_are_defined)
#undef bool
#undef true
#undef false
typedef char bool;
#define false 0
#define true !false
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

/* NULL pointer value */
#ifndef NULL
#define NULL (0x0)
#endif


#ifndef _WIN32
#define UT_CONSOLE_ENABLE_COLOR 1
#else
#if NTDDI_VERSION > NTDDI_WINBLUE
#define UT_CONSOLE_ENABLE_COLOR 1
#endif
#endif

/* Color constants */
#ifdef UT_CONSOLE_ENABLE_COLOR
#define UT_BLACK   "\033[1;30m"
#define UT_RED     "\033[0;31m"
#define UT_GREEN   "\033[0;32m"
#define UT_YELLOW  "\033[0;33m"
#define UT_BLUE    "\033[0;34m"
#define UT_MAGENTA "\033[0;35m"
#define UT_CYAN    "\033[0;36m"
#define UT_WHITE   "\033[1;37m"
#define UT_GREY    "\033[0;37m"
#define UT_NORMAL  "\033[0;49m"
#define UT_BOLD    "\033[1;49m"
#else
#define UT_BLACK   ""
#define UT_RED     ""
#define UT_GREEN   ""
#define UT_YELLOW  ""
#define UT_BLUE    ""
#define UT_MAGENTA ""
#define UT_CYAN    ""
#define UT_WHITE   ""
#define UT_GREY    ""
#define UT_NORMAL  ""
#define UT_BOLD    ""
#endif

#define UT_FUNCTION __func__

/* Builtin collection-implementation definitions */
typedef struct ut_rb_s* ut_rb;
typedef struct ut_ll_s* ut_ll;

/* Iterator type */
typedef struct ut_iter ut_iter;
struct ut_iter {
    void *ctx;
    void *data;
    bool (*hasNext)(ut_iter*);
    void* (*next)(ut_iter*);
    void* (*nextPtr)(ut_iter*);
    void (*release)(ut_iter*);
};

/* Callback used to compare values */
typedef int (*ut_compare_cb)(void* o1, void* o2);

/* Callback used to walk elements in collection */
typedef int (*ut_elementWalk_cb)(void* o, void* userData);

/* Callback used to determine if value is smaller/larger/equal */
typedef int (*ut_equals_cb)(void *context, const void* o1, const void* o2);

/* Type for traversing a tree */
#ifndef HEIGHT_LIMIT
#define HEIGHT_LIMIT (24) /* 16M nodes in a single tree */
#endif

typedef struct jsw_rbtrav jsw_rbtrav_t;
typedef struct jsw_rbtree jsw_rbtree_t;
typedef struct jsw_rbnode jsw_rbnode_t;

struct jsw_rbtrav {
  jsw_rbtree_t *tree;               /* Paired tree */
  jsw_rbnode_t *it;                 /* Current node */
  jsw_rbnode_t *path[HEIGHT_LIMIT]; /* Traversal path */
  size_t        top;                /* Top of stack */
  int32_t       changes;            /* Check if tree has changed since last */
};

/* Global variables */
UT_API 
extern int8_t UT_APP_STATUS;

UT_API 
extern int8_t UT_LOG_BACKTRACE;

UT_API
void ut_init(
    const char *appName);

UT_API
void ut_deinit(void);

UT_API
const char* ut_appname(void);

#ifdef __cplusplus
}
#endif

/* Base includes */
#include "bake-util/parson.h"
#include "bake-util/strbuf.h"
#include "bake-util/iter.h"
#include "bake-util/ll.h"
#include "bake-util/rb.h"
#include "bake-util/string.h"
#include "bake-util/time.h"
#include "bake-util/dl.h"
#include "bake-util/code.h"
#include "bake-util/fs.h"

#ifdef _WIN32
#include "bake-util/win/thread.h"
#include "bake-util/win/vs.h"
#else
#include "bake-util/posix/thread.h"
#endif

#include "bake-util/thread.h"
#include "bake-util/file.h"
#include "bake-util/env.h"
#include "bake-util/memory.h"
#include "bake-util/proc.h"
#include "bake-util/expr.h"
#include "bake-util/jsw_rbtree.h"
#include "bake-util/path.h"
#include "bake-util/load.h"
#include "bake-util/version.h"

#ifndef __BAKE_LEGACY__
#include "bake-util/log.h"
#include "bake-util/os.h"
#endif

#endif /* UT_BASE_H */
