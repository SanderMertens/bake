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

#include "../../include/util.h"
#include <io.h>

ut_proc ut_proc_run(
    const char* exec,
    const char *argv[])
{
    char *cmdline = NULL;
    int count = 0;

    ut_strbuf buf = UT_STRBUF_INIT;
    ut_strbuf_appendstr(&buf, exec);
    while (argv[count]) {
        ut_strbuf_append(&buf, " %s", argv[count ++]);
    }
    cmdline = ut_strbuf_get(&buf);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    BOOL bSuccess = FALSE;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    ut_trace("#[cyan]%s", cmdline);

    bSuccess = CreateProcess(exec,
        cmdline,     // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        0,             // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &si,  // STARTUPINFO pointer 
        &pi);
    return pi.hProcess;
}

ut_proc ut_proc_runRedirect(
    const char* exec,
    const char *argv[],
    FILE *in,
    FILE *out,
    FILE *err)
{
    char *cmdline = NULL;
    int count = 0;

    ut_strbuf buf = UT_STRBUF_INIT;
    ut_strbuf_appendstr(&buf, exec);
    while (argv[count]) {
        ut_strbuf_append(&buf, " %s", argv[count ++]);
    }
    cmdline = ut_strbuf_get(&buf);

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    BOOL bSuccess = FALSE;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (in)
        si.hStdInput  = (HANDLE)_get_osfhandle(_fileno(in));
    if(out)
        si.hStdOutput = (HANDLE)_get_osfhandle(_fileno(out));
    if (err)
        si.hStdError  = (HANDLE)_get_osfhandle(_fileno(err));
    si.dwFlags |= STARTF_USESTDHANDLES;

    ut_trace("#[cyan]%s", cmdline);

    bSuccess = CreateProcess(exec,
        cmdline,     // command line 
        NULL,          // process security attributes 
        NULL,          // primary thread security attributes 
        TRUE,          // handles are inherited 
        0,             // creation flags 
        NULL,          // use parent's environment 
        NULL,          // use parent's current directory 
        &si,  // STARTUPINFO pointer 
        &pi);
    return pi.hProcess;
}

int ut_proc_kill(ut_proc hProcess, ut_procsignal sig) {
    // Close process and thread handles. 
    CloseHandle(hProcess);
    return 0;
}

int ut_proc_wait(ut_proc hProcess, int8_t *rc) {
    WaitForSingleObject(hProcess, INFINITE);
    return 0;
}

#define BUFFER_SIZE (256)

/* Simple blocking function to create and wait for a process */
static
int ut_proc_cmd_intern(
    char* cmd,
    int8_t *rc,
    bool stderr_only)
{
    int status = -1;
    if (stderr_only)
    {
        char *buff = ut_asprintf("%s 1> nul", cmd);
        status = system(buff);
    }
    else
    {
        status = system(cmd);
    }
    *rc = status;
    return  status;
}

int ut_proc_cmd(char* cmd, int8_t *rc) {
    return ut_proc_cmd_intern(cmd, rc, false);
}

int ut_proc_cmd_stderr_only(char* cmd, int8_t *rc) {
    return ut_proc_cmd_intern(cmd, rc, true);
}

int ut_proc_check(ut_proc pid, int8_t *rc) {
    return 0;
}

int ut_beingTraced(void) {
    return 0;
}

ut_proc _ut_proc(void) {
    HANDLE currentProcessHandle = GetCurrentProcess(); 
    return currentProcessHandle;
}
