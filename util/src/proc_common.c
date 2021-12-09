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

#define BUFFER_SIZE (256)

/* Simple blocking function to create and wait for a process */
static
int ut_proc_cmd_intern(
    char* cmd,
    int8_t *rc,
    bool stderr_only)
{
    ut_proc pid;
    const char *args[UT_MAX_CMD_ARGS];
    char stack_buffer[BUFFER_SIZE];
    char *buffer = stack_buffer;

    int len = strlen(cmd);
    if (len >= BUFFER_SIZE) {
        buffer = malloc(len + 1);
    }

    strcpy(buffer, cmd);

    /* Split up commands */
    char ch, *ptr;
    uint8_t argCount = 0;
    bool newArg = false;
    bool isString = false;
    args[argCount] = buffer;
    for (ptr = buffer; (ch = *ptr); ptr++) {
        if (ch == '"') {
            if (isspace(ptr[-1])) {
                *ptr = '\0';
            }
            isString = !isString;
            if (!isString) {
                newArg = true;
            }
        } else
        if (!isString && isspace(ch)) {
            *ptr = '\0';
            newArg = true;
        } else if (newArg) {
            args[++argCount] = ptr;
            newArg = false;
        }
    }
    args[argCount + 1] = NULL;

    if (stderr_only) {
        if (!(pid = ut_proc_runRedirect(
            args[0],
            args,
            stdin,
            NULL,
            stderr)))
        {
            goto error;
        }
    } else {
        if (!(pid = ut_proc_run(args[0], args))) {
            goto error;
        }
    }

    if (buffer != stack_buffer) free(buffer);
    return ut_proc_wait(pid, rc);
error:
    if (buffer != stack_buffer) free(buffer);
    return -1;
}

int ut_proc_cmd(char* cmd, int8_t *rc) {
    return ut_proc_cmd_intern(cmd, rc, false);
}

int ut_proc_cmd_stderr_only(char* cmd, int8_t *rc) {
    return ut_proc_cmd_intern(cmd, rc, true);
}
