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

#include "bake.h"

static
int16_t cmd(
   char *cmd)
{
    int8_t ret;
    int sig = ut_proc_cmd_stderr_only(cmd, &ret);
    if (sig || ret) {
        ut_error("'%s' (%s %d)", cmd, sig ? "sig" : "result", sig ? sig : ret);
        return -1;
    }

    return 0;
}

int16_t bake_install_remote(
    bake_config *config,
    const char *id)
{
    /* TODO: implement installation from git cache using logical package name */
    return 0;
}

int16_t bake_clone(
    bake_config *config,
    const char *url)
{
    bool has_protocol = false;
    char *proto = strchr(url, '/');
    if (proto) {
        if (proto == url) {
            ut_throw("invalid repository name '%s': cannot start with '/'");
            goto error;
        }

        if (proto[-1] == ':') {
            if (proto[1] == '/') {
                has_protocol = true;
            }
        }
    }

    char *repo = strrchr(url, '/');
    if (!repo) {
        repo = strrchr(url, ':');
        if (!repo) {
            ut_throw("invalid git URL");
            goto error;
        }
    }

    repo ++; /* Skip separator character */

    char *src_path = ut_envparse("$BAKE_HOME/src/%s", repo);
    if (ut_file_test(src_path) == 1) {
        ut_throw("project '%s' already cloned, use bake update %s instead", repo);
        free(src_path);
        goto error;
    }

    ut_info("cloning repository '%s' to '%s'", repo, src_path);
    char *gitcmd = ut_envparse("git clone %s $BAKE_HOME/src/%s", url, repo);
    ut_try (cmd(gitcmd), NULL);

    return 0;
error:
    return -1;
}
