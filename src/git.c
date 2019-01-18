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
int16_t bake_update_dependencies(
    bake_config *config,
    const char *base_url,
    bake_project *project);

static
int16_t cmd(
   char *cmd)
{
    int8_t ret;
    int sig = ut_proc_cmd(cmd, &ret);
    if (sig || ret) {
        ut_throw("'%s' (%s %d)", cmd, sig ? "sig" : "result", sig ? sig : ret);
        return -1;
    }

    return 0;
}

static
int16_t git_pull(
    const char *src_path)
{
    char *
    gitcmd = ut_asprintf("git -C %s fetch -q origin", src_path);
    ut_try(cmd(gitcmd), NULL);
    gitcmd = ut_asprintf("git -C %s reset -q --hard origin/master", src_path);
    ut_try(cmd(gitcmd), NULL);
    gitcmd = ut_asprintf("git -C %s clean -q -xdf", src_path);
    ut_try(cmd(gitcmd), NULL);

    return 0;
error:
    return -1;
}

static
int16_t bake_parse_repo_url(
    const char *url,
    char **full_url_out,
    char **base_url_out,
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

    char *last_elem = strrchr(full_url, '/');
    char *base_url = NULL;
    if (last_elem) {
        *last_elem = '\0';
        base_url = ut_strdup(full_url);
        *last_elem = '/';
    }

    *full_url_out = full_url;
    *base_url_out = base_url;
    *name_out = name;
    *src_out = src_path;

    return 0;
error:
    return -1;
}

static
int16_t bake_update_dependency_list(
    bake_config *config,
    const char *base_url,
    ut_ll dependencies)
{
    ut_iter it = ut_ll_iter(dependencies);
    while (ut_iter_hasNext(&it)) {
        char *dep = ut_iter_next(&it);

        if (!strcmp(dep, "bake.util")) {
            /* Skip bootstrapped bake.util dependency */
            continue;
        }

        if (bake_crawler_get(dep)) {
            ut_debug("dependency '%s' already added to crawler, skipping", dep);
            continue;
        }

        char ch, *ptr, *dep_tmp = ut_strdup(dep);
        for (ptr = dep_tmp; (ch = *ptr); ptr ++) {
            if (ch == '.' || ch == '/') {
                *ptr = '-';
            }
        }

        const char *src = ut_locate(dep, NULL, UT_LOCATE_SOURCE);
        if (src) {
            /* If source is in bake environment, pull latest version */
            ut_log("pull '%s' in '%s'\n", dep, src);
            git_pull(src);
        } else {
            /* If source is a project under development, just build it */
            src = ut_locate(dep, NULL, UT_LOCATE_DEVSRC);
            ut_log("found '%s' in '%s' (in dev tree, not pulling)\n", dep, src);
        }

        if (src) {
            bake_project *dep_project = bake_project_new(src, config);
            bake_crawler_add(config, dep_project);
            ut_try( bake_update_dependencies(config, base_url, dep_project), NULL);
        } else {
            const char *deppath = ut_locate(dep, NULL, UT_LOCATE_PROJECT);
            if (deppath) {
                if (ut_locate(dep, NULL, UT_LOCATE_BIN)) {
                    ut_log("found dependency '%s' locally in '%s'\n", dep, deppath);
                } else {
                    ut_log(
        "found project '%s' but no source or binary. Attempting to reclone.\n");
                    deppath = NULL;
                }
            }

            if (!deppath) {
                char *url = ut_asprintf("%s/%s", base_url, dep_tmp);
                if (bake_clone(config, url)) {
                    ut_catch();
                    ut_throw(
                        "cannot find repository '%s' in '%s'", dep_tmp, base_url);
                    goto error;
                }
                free(url);
            }
        }

        free(dep_tmp);
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_update_dependencies(
    bake_config *config,
    const char *base_url,
    bake_project *project)
{
    ut_try(bake_update_dependency_list(config, base_url, project->use), NULL);
    ut_try(bake_update_dependency_list(config, base_url, project->use_private), NULL);
    return 0;
error:
    return -1;
}

int16_t bake_update(
    bake_config *config,
    const char *url)
{
    char *full_url = NULL, *base_url = NULL, *name = NULL, *src_path = NULL;
    bake_project *project = NULL;
    ut_try( bake_parse_repo_url(url, &full_url, &base_url, &name, &src_path), NULL);

    ut_log("update '%s' in '%s'\n", full_url, src_path);

    git_pull(src_path);

    project = bake_project_new(src_path, config);
    if (!project) {
        ut_throw("repository '%s' is not a valid bake project", full_url);
        goto error;
    }

    ut_try(bake_update_dependencies(config, base_url, project), NULL);

    bake_crawler_add(config, project);

    free(full_url);
    free(base_url);
    free(src_path);

    return 0;
error:
    return -1;
}

int16_t bake_clone(
    bake_config *config,
    const char *url)
{
    char *full_url = NULL, *base_url = NULL, *name = NULL, *src_path = NULL;
    bake_project *project = NULL;
    ut_try( bake_parse_repo_url(url, &full_url, &base_url, &name, &src_path), NULL);

    if (ut_file_test(src_path) == 1) {
        ut_trace("project '%s' already cloned, updating instead", name);
        free(full_url);
        free(src_path);
        return bake_update(config, url);
    } else {
        ut_log("clone '%s' into '%s'\n", full_url, src_path);
        char *gitcmd = ut_envparse("git clone -q %s %s", full_url, src_path);
        ut_try (cmd(gitcmd), NULL);
    }

    project = bake_project_new(src_path, config);
    if (!project) {
        ut_throw("repository '%s' is not a valid bake project", full_url);
        goto error;
    }

    ut_try( bake_update_dependencies(config, base_url, project), NULL);
    ut_try( bake_crawler_add(config, project), NULL);

    free(full_url);
    free(base_url);
    free(src_path);

    return 0;
error:
    return -1;
}

int16_t bake_publish(
    bake_config *config,
    bake_project *project,
    const char *publish_cmd)
{
    ut_version_kind kind;
    if (!strcmp(publish_cmd, "patch")) {
        kind = UT_VERSION_PATCH;
    } else if (!strcmp(publish_cmd, "minor")) {
        kind = UT_VERSION_MINOR;
    } else if (!strcmp(publish_cmd, "major")) {
        kind = UT_VERSION_MAJOR;
    }

    char *new_version = ut_version_inc(project->version, kind);
    if (!new_version) {
        goto error;
    }

    free(project->version);
    project->version = new_version;

    JSON_Value *project_json = json_parse_file_with_comments("project.json");
    if (!project_json) {
        ut_throw("failed to parse 'project.json'");
        goto error;
    }

    JSON_Object *obj = json_value_get_object(project_json);
    JSON_Object *value = bake_json_find_or_create_object(obj, "value");
    if (!value) {
        ut_throw("failed to obtain 'value' member of project.json");
        goto error;
    }

    json_object_set_string(value, "version", new_version);

    json_set_escape_slashes(0);

    ut_try (json_serialize_to_file_pretty(project_json, "project.json"), NULL);

    char *command = ut_asprintf("git add project.json", project->version);
    ut_try( cmd(command), NULL);

    command = ut_asprintf("git commit -m \"Published version %s\"", project->version);
    ut_try( cmd(command), NULL);
    ut_log("#[green]OK #[normal]Committed version '%s'\n", project->version);

    command = ut_asprintf("git tag -a v%s -m \"Version v%s\"",
        project->version, project->version);
    ut_try( cmd(command), NULL);
    ut_log("#[green]OK #[normal]Created tag 'v%s'\n", project->version);

    ut_log("#[green]OK #[normal]Published %s:%s\n", project->id, project->version);

    return 0;
error:
    return -1;
}
