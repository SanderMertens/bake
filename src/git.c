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

#include "bake.h"

static
int16_t bake_update_dependencies(
    bake_config *config,
    const char *base_url,
    bake_project *project,
    bool to_env,
    bool override_env,
    bake_notify_state *notify_state);

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
    ut_raise();
    return -1;
}

static
int16_t bake_parse_repo_url(
    const char *url,
    char **full_url_out,
    char **base_url_out,
    char **name_out,
    char **src_out,
    bool to_env)
{
    bool has_protocol = false;
    bool has_url = false;

    *full_url_out = NULL;
    *base_url_out = NULL;
    *name_out = NULL;
    *src_out = NULL;

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

    char *src_path;
    if (to_env) {
        src_path = ut_envparse("%s"UT_OS_PS"%s", UT_SRC_PATH, name);
    } else {
        src_path = ut_envparse("."UT_OS_PS"%s", name);
    }

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
    ut_ll dependencies,
    bool to_env,
    bool override_env,
    bake_notify_state *notify_state)
{
    ut_iter it = ut_ll_iter(dependencies);
    while (ut_iter_hasNext(&it)) {
        char *dep = ut_iter_next(&it);

        if (ut_project_is_buildtool(dep)) {
            /* Skip build utility dependencies */
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

        const char *src_path = NULL;
        if (to_env) {
            src_path = ut_locate(dep, NULL, UT_LOCATE_SOURCE);
        }

        if (src_path) {
            /* If source is in bake environment, pull latest version */
            bake_message(UT_LOG, "pull", "#[green]package#[reset] %s in '%s'", dep, src_path);
            git_pull(src_path);
        } else if (!override_env) {
            /* If source is a project under development, just build it */
            src_path = ut_locate(dep, NULL, UT_LOCATE_DEVSRC);
            if (src_path) {
                bake_message(UT_LOG, 
                    "skip", 
                    "#[green]package#[reset] %s found in '%s' (in dev tree, not pulling)", 
                    dep, src_path);

                if (!notify_state->override_env) {
                    bake_message(UT_LOG, 
                        "note", 
                        "to pull anyway, add --override-env to the bake clone command", 
                        dep, src_path);
                    notify_state->override_env = true;                    
                }
            }
        }

        if (src_path) {
            bake_project *dep_project = bake_project_new(src_path, config);
            if (bake_crawler_search(config, src_path) == -1) {
                goto error;
            }           
            ut_try( bake_update_dependencies(config, base_url, dep_project, to_env, override_env, notify_state), NULL);
        } else if (base_url) {
            if (!override_env && to_env) {
                if (ut_locate(dep, NULL, UT_LOCATE_PROJECT)) {
                    bake_message(UT_LOG, "note", "found '%s' but cloning anyway because source is missing", dep);
                }
            }

            char *url = ut_asprintf("%s/%s", base_url, dep_tmp);
            if (bake_clone(config, url, to_env, override_env, notify_state)) {
                ut_catch();
                ut_throw(
                    "cannot find repository '%s' in '%s'", dep_tmp, base_url);
                goto error;
            }

            free(url);
        } else {
            ut_throw("cannot clone unresolved dependency '%s'", dep);
            free(dep_tmp);
            goto error;
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
    bake_project *project,
    bool to_env,
    bool override_env,
    bake_notify_state *notify_state)
{
    ut_try(bake_update_dependency_list(config, base_url, project->use, to_env, override_env, notify_state), NULL);
    ut_try(bake_update_dependency_list(config, base_url, project->use_private, to_env, override_env, notify_state), NULL);
    return 0;
error:
    return -1;
}

int bake_update_action(
    bake_config *config,
    bake_project *project)
{
    char *git_path = ut_asprintf("%s/.git", project->path);

    if (ut_file_test(git_path)) {
        git_pull(project->path);
    }

    free(git_path);
    
    return 0;
error:
    return -1;
}

int16_t bake_clone(
    bake_config *config,
    const char *url,
    bool to_env,
    bool override_env,
    bake_notify_state *notify_state)
{
    char *full_url = NULL, *base_url = NULL, *name = NULL, *src_path = NULL;
    bake_project *project = NULL;
    bake_notify_state notify_state_var = {0};
    if (!notify_state) {
        notify_state = &notify_state_var;
    }

    ut_try( bake_parse_repo_url(url, &full_url, &base_url, &name, &src_path, to_env), NULL);

    if (ut_file_test(src_path) == 1) {
        bake_message(UT_LOG, "skip", "#[green]package#[reset] %s (already cloned)", name);
        bake_message(UT_LOG, "note", "to pull the latest state of %s, run 'bake update'", name);
    } else {
        bake_message(UT_LOG, "clone", "'%s' into '%s'", full_url, src_path);
        char *gitcmd = ut_envparse("git clone -q %s %s", full_url, src_path);
        ut_try (cmd(gitcmd), NULL);
    }

    project = bake_project_new(src_path, config);
    if (!project) {
        ut_throw("repository '%s' is not a valid bake project", full_url);
        goto error;
    }

    ut_try( bake_update_dependencies(config, base_url, project, to_env, override_env, notify_state), NULL);

    if (bake_crawler_search(config, src_path) == -1) {
        goto error;
    }

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
    bake_message(UT_LOG, "done", "Committed version '%s'", project->version);

    command = ut_asprintf("git tag -a v%s -m \"Version v%s\"",
        project->version, project->version);
    ut_try( cmd(command), NULL);
    bake_message(UT_LOG, "done", "Created tag 'v%s'", project->version);

    bake_message(UT_LOG, "done", "Published %s:%s", project->id, project->version);

    return 0;
error:
    return -1;
}
