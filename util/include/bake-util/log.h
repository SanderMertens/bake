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
 * @section logging framework.
 * @brief Functions for throwing, catching, forwarding and printing logging.
 *
 * The logging framework is the basis for a multi-language approach that can be
 * used by languages/frameworks that do and do not support native exception handling.
 *
 * Usage of the framework is as follows:
 * - Nested- and library functions use ut_throw and ut_lasterr to log errors.
 *   This will store error messages in internal, thread-specific buffers until
 *   they are logged to the console by ut_error.
 *
 * - Top level functions use ut_error directly. This will
 *   log directly to the console (when verbosity level permits) and
 *   forward message to any registered handlers (always). Top level functions
 *   should use ut_lasterr when an error is caused by a nested call.
 *
 * When ut_throw is used, and ut_error is called without using ut_lasterr
 * the framework will report a warning to the console that an error has been
 * silenced. This protects users from accidentally missing error information.
 * When an application calls ut_stop and ut_lasterr has not been called
 * after calling ut_throw, the message is also logged to the console.
 *
 * The ut_info function should only be used by application-level functions
 * (not by functions in a library).
 *
 * The ut_ok, ut_trace and ut_debug functions may be used by both
 * library functions and application functions. These functions will only log to
 * the console when verbosity level permits.
 */

#ifndef UT_LOG_H_
#define UT_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <dbghelp.h>
#define TRACE_MAX_STACK_FRAMES 1024
#define TRACE_MAX_FUNCTION_NAME_LENGTH 1024
#endif

#ifdef _WIN32
    void ut_enable_console_color();
#endif

/* -- Setting & getting verbosity level -- */

/* Error verbosity levels */
typedef enum ut_log_verbosity {
    UT_THROW = -1,   /* used for internal debugging purposes */
    UT_DEBUG = 0,    /* implementation-specific tracing */
    UT_TRACE = 1,    /* progress/state of an application */
    UT_OK = 2,       /* successful completion of a task */
    UT_INFO = 3,     /* neutral message directed at user */
    UT_WARNING = 4,  /* issue that does not require immediate action */
    UT_ERROR = 5,    /* unsuccessful completion of a task */
    UT_CRITICAL = 6, /* task left application in undefined state (abort) */
    UT_ASSERT = 7,   /* assertion failed (abort) */

    /* Internal usage */
    UT_ERROR_FROM = 8,
    UT_ERROR_PUSH = 9,
    UT_ERROR_PROC = 10,

    /* Print no verbosity */
    UT_LOG = 11
} ut_log_verbosity;

/** Set verbosity level.
 * The verbosity level is the same for all threads.
 * @param verbosity Verbosity level.
 */
UT_EXPORT
ut_log_verbosity ut_log_verbositySet(
    ut_log_verbosity verbosity);

/** Get verbosity level.
 * The verbosity level is the same for all threads.
 * @return The current verbosity level.
 */
UT_EXPORT
ut_log_verbosity ut_log_verbosityGet(void);

/** Set verbosity depth.
 * The verbosity depth specifies whether to log messages at deep levels of
 * nesting, as indicated by log_push and log_pop. If the depth is set at 3,
 * messages logged after three push operations (without a pop) will be filtered
 * out.
 *
 * The verbosity depth is the same for all threads.
 *
 * @param verbosity Verbosity depth.
 */
UT_EXPORT
int ut_log_verbositySetDepth(
    int depth);


/** Enable or disable colors.
 */
UT_EXPORT
bool ut_log_useColors(
    bool enable);

/** Enable or disable profiling.
 *
 * When profiling is enabled, categories will always be printed to the console.
 */
UT_EXPORT
bool ut_log_profile(
    bool enable);

/* -- Logging format used in console -- */

/** Default logging format. */
#define UT_LOG_FORMAT_DEFAULT "%v  %C %F:%L %m"

/** Set logging format for console.
 * The following characters can be used for specifying the format:
 * -v Verbosity
 * -a Application name
 * -c Category
 * -C Category (tree it)
 * -m Formatted message
 * -t numerical representation of time (sec, nanosec)
 * -T user-friendly representation of time (1985-4-11 20:00:00.0000)
 * -d Time delta since last message
 * -f filename (use F for filename only in error messages)
 * -l line number (use L for linenumber only in error messages)
 * -r function (use R for function only in error messages)
 *
 * The logging format is the same for all threads. This function will also set
 * the UT_LOG_FORMAT environment variable.
 *
 * @param fmt Format used for logging.
 */
UT_EXPORT
void ut_log_fmt(
    char *fmt);

UT_EXPORT
const char* ut_log_fmtGet(void);

/* -- Pushing/popping logging categories -- */

/** Push a category to the category stack.
 *
 * @param category Identifier of category.
 * @return Zero if success, non-zero if failed (max number of categories pushed).
 */
UT_EXPORT
int _ut_log_push(
    char const *file,
    unsigned int line,
    char const *function,
    const char *category);

/** Pop a category from the category stack.
 */
UT_EXPORT
void _ut_log_pop(
    char const *file,
    unsigned int line,
    char const *function);

/** Embed categories in logmessage or print them on push/pop
 *
 */
UT_EXPORT
void ut_log_embedCategories(
    bool embed);

/* -- Registering handlers for logging -- */

typedef void (*ut_log_handler_cb)(
    ut_log_verbosity level,
    const char *msg,
    void *ctx);

/** Register callback that catches log messages.
 * @param callback Message handler callback.
 * @param context Generic value that will be passed to handler.
 * @return Handler object that can be used to unregister callback.
 */
UT_EXPORT
void ut_log_handlerRegister(
    ut_log_handler_cb callback,
    void *context);

/** Check if any handlers are registered.
 *
 * @return true if handlers are registered, otherwise false.
 */
UT_EXPORT
bool ut_log_handlerRegistered(void);



/* -- Logging messages to console -- */

/** Log message with level UT_DEBUG.
 * This function should not be called directly, but through a macro as follows:
 *   ut_debug(fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * @param fmt A printf-style format string.
 */
UT_EXPORT
void _ut_debug(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Log message with level UT_TRACE.
 * This function should not be called directly, but through a macro as follows:
 *   ut_trace(fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * @param fmt A printf-style format string.
 */
UT_EXPORT
void _ut_trace(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Log message with level UT_INFO.
 * This function should not be called directly, but through a macro as follows:
 *   ut_info(fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * @param fmt A printf-style format string.
 */
UT_EXPORT
void _ut_info(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Log message with level UT_OK.
 * This function should not be called directly, but through a macro as follows:
 *   ut_ok(fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * @param fmt A printf-style format string.
 */
UT_EXPORT
void _ut_ok(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Log message with level UT_WARNING.
 * This function should not be called directly, but through a macro as follows:
 *   ut_warning(fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * @param fmt A printf-style format string.
 */
UT_EXPORT
void _ut_warning(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Log message with level UT_ERROR.
 * This function should not be called directly, but through a macro as follows:
 *   ut_error(fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * @param fmt A printf-style format string.
 */
UT_EXPORT
void _ut_error(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Log message with level UT_ASSERT.
 * This function should not be called directly, but through a macro as follows:
 *   ut_assert(condition, fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * When the condition is false, this function will print a stack trace and exit
 * the process using abort(). This function ignores the verbosity level.
 *
 * Asserts are disabled when built with NDEBUG.
 *
 * @param fmt A printf-style format string.
 * @param condition A condition to evaluate. When false, the process aborts.
 */
UT_EXPORT
void _ut_assert(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Log message with level UT_CRITICAL.
 * This function should not be called directly, but through a macro as follows:
 *   ut_critical(fmt, ...)
 * The macro automatically adds information about the current file and line number.
 *
 * This function will abort the process after displaying the formatted message.
 * This function ignores the verbosity level.
 *
 * @param fmt A printf-style format string.
 */
UT_EXPORT
void _ut_critical(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    ...);

/** Overwrite log if below configured verbosity. */
UT_EXPORT
void _ut_log_overwrite(
    char const *file,
    unsigned int line,
    char const *function,
    ut_log_verbosity verbosity,
    const char *fmt,
    ...);

/** As ut_debug, but with va_list parameter. */
UT_EXPORT
void ut_debugv(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    va_list args);

/** As ut_trace, but with va_list parameter. */
UT_EXPORT
void ut_tracev(
    char const *file,
    unsigned int line,
    char const *function,
    const char* fmt,
    va_list args);

/** As ut_info, but with va_list parameter. */
UT_EXPORT
void ut_infov(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args);

/** As ut_ok, but with va_list parameter. */
UT_EXPORT
void ut_okv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args);

/** As ut_warning, but with va_list parameter. */
UT_EXPORT
void ut_warningv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args);

/** As ut_error, but with va_list parameter. */
UT_EXPORT
void ut_errorv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args);

/** As ut_assert, but with va_list parameter. */
UT_EXPORT
void _ut_assertv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args);

/** As ut_critical, but with va_list parameter. */
UT_EXPORT
void ut_criticalv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args);



/* -- Exception handling -- */

/** Propagate error to calling function upon failure.
 * This error must always be called when a function fails. When the function
 * fails because it in turn called a function that failed, the function may
 * choose to not call ut_throw, but rely on the error propagated by the
 * failed function. It is good practice however to enrich errors with additional
 * context wherever possible.
 *
 * @param fmt printf-style format string.
 */
UT_EXPORT
void _ut_throw(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...);

UT_EXPORT
void _ut_throw_fallback(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...);

/* As ut_throw, but with va_list parameter. */
UT_EXPORT
void ut_throwv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args);

/** Add details to an exception */
UT_EXPORT
void ut_throw_detail(
    const char *fmt,
    ...);

/** Catch an exception.
 * If an exception is not catched, it will eventually be logged to the console.
 *
 * @return true if an exception was catched.
 */
UT_EXPORT
bool ut_catch(void);

/** Check if exception was raised.
 * @return true if an exception was raised.
 */
UT_EXPORT
bool ut_raised(void);

/** Raise an exception.
 */
UT_EXPORT
bool ut_raise(void);

/** Raise an exception, add prefix to error messages.
 */
UT_EXPORT
bool ut_raise_ext(
    const char *log_fmt,
    const char *prefix_fmt, ...);

/** Try statement, if failed throw error and goto error.
 *
 */
#define ut_try(stmt, ...) if (stmt) { ut_throw(__VA_ARGS__); goto error; }

/** Check for unraised exceptions (internal usage).
 */
UT_EXPORT
bool __ut_raise_check(void);

typedef enum ut_log_exceptionAction {
    UT_LOG_ON_EXCEPTION_IGNORE = 0,
    UT_LOG_ON_EXCEPTION_EXIT = 1,
    UT_LOG_ON_EXCEPTION_ABORT = 2
} ut_log_exceptionAction;

/** Specify what to do when exception is raised.
 */
UT_EXPORT
ut_log_exceptionAction ut_log_setExceptionAction(
    ut_log_exceptionAction action);


/** Propagate information to calling function.
 * This function is useful when a function wants to propagate a message that
 * is does not necessarily indicate an error. The framework will not report a
 * warning when this information is not viewed.
 */
UT_EXPORT void ut_setinfo(char *fmt, ...);

/** Retrieve last propagated information. */
UT_EXPORT
char* ut_lastinfo(void);

/* -- Print stacktraces -- */

/** Print current stacktrace to a file.
 *
 * @param f The file to which to print the stacktrace.
 */
UT_EXPORT
void ut_backtrace(
    FILE* f);


/* Stub */
UT_EXPORT
char *ut_lasterr();

/* -- Utilities -- */
UT_EXPORT
void ut_log(char *str, ...);

UT_EXPORT
void ut_log_tail(char *str, ...);

int16_t ut_log_init(void);

void ut_log_deinit();

/* -- Helper macro's -- */

#define ut_critical(...) _ut_critical(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__)
#define ut_critical_fl(file, line, ...) _ut_critical(file, line, UT_FUNCTION, __VA_ARGS__)

#define _SHOULD_PRINT(lvl)\
    ut_log_handlerRegistered() ||\
    ut_log_verbosityGet() <= lvl

#define ut_throw(...) _ut_throw(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__)
#define ut_throw_fallback(...) _ut_throw_fallback(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__)
#define ut_error(...) _ut_error(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__)
#define ut_warning(...) _ut_warning(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__)
#define ut_log_push(category) _ut_log_push(__FILE__, __LINE__, UT_FUNCTION, category);
#define ut_log_pop() _ut_log_pop(__FILE__, __LINE__, UT_FUNCTION);
#define ut_throw_fl(f, l, ...) _ut_throw(f, l, UT_FUNCTION, __VA_ARGS__)
#define ut_throw_fallback_fl(f, l, ...) _ut_throw_fallback(f, l, UT_FUNCTION, __VA_ARGS__)
#define ut_error_fl(f, l, ...) _ut_error(f, l, UT_FUNCTION, __VA_ARGS__)
#define ut_warning_fl(f, l, ...) _ut_warning(f, l, UT_FUNCTION, __VA_ARGS__)
#ifndef NDEBUG
#define ut_assert(condition, ...) if (!(condition)) {_ut_assert(__FILE__, __LINE__, UT_FUNCTION, "(" #condition ") " __VA_ARGS__);}
#define ut_debug(...) if(_SHOULD_PRINT(UT_DEBUG)) { _ut_debug(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__);} else { __ut_raise_check(); }
#define ut_trace(...) if(_SHOULD_PRINT(UT_TRACE)) { _ut_trace(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__);} else { __ut_raise_check(); }
#define ut_info(...) if(_SHOULD_PRINT(UT_INFO)) { _ut_info(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__);} else { __ut_raise_check(); }
#define ut_ok(...) if(_SHOULD_PRINT(UT_OK)) { _ut_ok(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__);} else { __ut_raise_check(); }
#define ut_log_overwrite(...) _ut_log_overwrite(__FILE__, __LINE__, UT_FUNCTION, __VA_ARGS__);
#define ut_log_push_dbg(category) if(_SHOULD_PRINT(UT_TRACE)) {_ut_log_push(__FILE__, __LINE__, UT_FUNCTION, category);}
#define ut_log_pop_dbg() if(_SHOULD_PRINT(UT_TRACE)) {_ut_log_pop(__FILE__, __LINE__, UT_FUNCTION);}
#else
#define ut_assert(condition, ...) (void)(condition)
#define ut_debug(...)
#define ut_trace(...)
#define ut_info(...)
#define ut_ok(...)
#define ut_log_push_dbg(...)
#define ut_log_pop_dbg(...)

#endif

#ifdef __cplusplus
}
#endif

#endif /* UT_ERR_H_ */
