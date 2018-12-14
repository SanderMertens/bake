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
    int sig = ut_proc_cmd(cmd, &ret);
    if (sig || ret) {
        ut_error("'%s' (%s %d)", cmd, sig ? "sig" : "result", sig ? sig : ret);
        return -1;
    }

    return 0;
}

static
int16_t bake_parse_repo_url(
    const char *url,
    char **full_url_out,
    char **name_out,
    char **src_out)
{
    bool has_protocol = false;
    bool has_url = false;

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

    if (strchr(url, '.')) {
        has_url = true;
    }

    char *name = strrchr(url, '/');
    if (!name) {
        name = strrchr(url, ':');
        if (!name) {
            ut_throw("invalid git URL");
            goto error;
        }
    }

    name ++; /* Skip separator character */

    char *full_url = NULL;
    if (has_protocol && has_url) {
        full_url = ut_strdup(url);
    } else {
        full_url = ut_asprintf("%s%s%s",
          has_protocol ? "" : "https://",
          has_url ? "" : "github.com/",
          url);
    }

    char *src_path = ut_envparse("$BAKE_HOME/src/%s", name);

    *full_url_out = full_url;
    *name_out = name;
    *src_out = src_path;

    return 0;
error:
    return -1;
}

bake_project* bake_update(
    bake_config *config,
    const char *url)
{
    char *full_url = NULL, *name = NULL, *src_path = NULL;
    bake_project *result = NULL;
    ut_try( bake_parse_repo_url(url, &full_url, &name, &src_path), NULL);

    ut_log("update '%s' in '%s'\n", full_url, src_path);

    char *
    gitcmd = ut_asprintf("git -C %s fetch -q origin", src_path);
    ut_try(cmd(gitcmd), NULL);
    gitcmd = ut_asprintf("git -C %s reset -q --hard origin/master", src_path);
    ut_try(cmd(gitcmd), NULL);
    gitcmd = ut_asprintf("git -C %s clean -q -xdf", src_path);
    ut_try(cmd(gitcmd), NULL);

    result = bake_project_new(src_path, config);
    free(full_url);
    free(src_path);

    return result;
error:
    return NULL;
}

bake_project* bake_clone(
    bake_config *config,
    const char *url)
{
    char *full_url = NULL, *name = NULL, *src_path = NULL;
    bake_project *result = NULL;
    ut_try( bake_parse_repo_url(url, &full_url, &name, &src_path), NULL);

    if (ut_file_test(src_path) == 1) {
        ut_trace("project '%s' already cloned, doing update instead", name);
        free(full_url);
        free(src_path);
        return bake_update(config, url);
    } else {
        ut_log("clone '%s' into '%s'\n", full_url, src_path);
        char *gitcmd = ut_envparse("git clone -q %s %s", full_url, src_path);
        ut_try (cmd(gitcmd), NULL);
    }

    result = bake_project_new(src_path, config);

    free(full_url);
    free(src_path);

    return result;
error:
    return NULL;
}
