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
 * @section Process management functions.
 * @brief Functions for doing basic operations on a filesystem.
 */

#ifndef UT_PROC_H
#define UT_PROC_H

#ifdef __cplusplus
extern "C" {
#endif

/** Process handle */
#ifdef _WIN32
typedef HANDLE ut_proc;
#else
typedef int ut_proc;
#endif

/** Process signals */
typedef enum ut_procsignal {
    UT_SIGINT = 2,
    UT_SIGQUIT = 3,
    UT_SIGILL = 4,
    UT_SIGABRT = 6,
    UT_SIGFPE = 8,
    UT_SIGKILL = 9,
    UT_SIGSEGV = 11,
    UT_SIGTERM = 15
} ut_procsignal;

/** Run a process.
 * Use ut_proc_wait/ut_prochcheck with the returned handle to check if the
 * child process has exited.
 *
 * @param exec Name of the executable file.
 * @param argv Null-terminated array of strings.
 * @return Handle to process.
 */
UT_API
ut_proc ut_proc_run(
    const char* exec,
    const char *argv[]);

/** Run a process, redirect stdin, stdout and stderr.
 * Use ut_proc_wait/ut_prochcheck with the returned handle to check if the
 * child process has exited.
 *
 * @param exec Name of the executable file.
 * @param argv Null-terminated array of strings.
 * @param in File to redirect stdin to.
 * @param out File to redirect stdout to.
 * @param err FIle to redirect stderr to.
 * @return Handle to process.
 */
UT_API
ut_proc ut_proc_runRedirect(
    const char* exec,
    const char *argv[],
    FILE *in,
    FILE *out,
    FILE *err);

/** Send signal to a process.
 *
 * @param pid Process handle.
 * @param sig Signal to send to process.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_proc_kill(
    ut_proc pid,
    ut_procsignal sig);

/** Wait for process to exit (blocking).
 *
 * @param pid Process handle.
 * @param rc Value returned by process.
 * @return 0 if success, -1 if function failed, otherwise the signal raised by the process during exit.
 */
UT_API
int ut_proc_wait(
    ut_proc pid,
    int8_t *rc);

/** Check if process is still alive.
 *
 * @param pid Process handle.
 * @param rc Value returned by process.
 * @return 0 if still running, -1 if exitted normally, otherwise the signal raised by the process during exit.
 */
UT_API
int ut_proc_check(
    ut_proc pid,
    int8_t *rc);

/** Run a process (blocking).
 * This function will block until the process exits.
 *
 * @param sig Signal to send to process.
 * @param rc Value returned by process.
 * @return 0 if success, -1 if function failed, otherwise the signal raised by the process during exit.
 */
UT_API
int ut_proc_cmd(
    char *cmd,
    int8_t *rc);

UT_API
int ut_proc_cmd_stderr_only(char* cmd, int8_t *rc);

/** Function that checks if process is being traced (experimental)
 *
 * @return non-zero if being traced, otherwise 0.
 */
UT_API
int ut_beingTraced(void);

/** Get pid of current process.
 * Call using ut_proc().
 *
 * @return Process handle of current process.
 */
UT_API
ut_proc _ut_proc(void);

/* Macro that allows using ut_proc as a function in addition to it being used
 * as a type. */
#define ut_proc() _ut_proc()

#ifdef __cplusplus
}
#endif

#endif
