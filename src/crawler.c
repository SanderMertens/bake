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

struct bake_crawler {
    ut_rb nodes; /* tree optimizes looking up dependencies */
    ut_ll leafs; /* projects that cannot act as dependencies */
    uint32_t count;
};

static bake_crawler *crawler;

static
int project_cmp(
    void *ctx,
    const void* key1,
    const void* key2)
{
    return strcmp(key1, key2);
}

static
void bake_crawler_addDependency(
    bake_project *p,
    char *use)
{
    bake_project *dep = ut_rb_find(crawler->nodes, use);
    if (!dep) {
        /* Create placeholder */
        dep = bake_project_new(NULL, NULL);
        if (dep) {
            dep->id = ut_strdup(use);
            ut_rb_set(crawler->nodes, dep->id, dep);
        }
    }

    if (!dep->dependents) {
        dep->dependents = ut_ll_new();
    }

    ut_ll_append(dep->dependents, p);
}

bake_project* bake_crawler_get(
    const char *id)
{
    bake_project *result = NULL;

    if (crawler->nodes) {
        result = ut_rb_find(crawler->nodes, id);
        if (result && !result->path) {
            /* Ignore placeholder projects */
            result = NULL;
        }
    }

    if (!result && crawler->leafs) {
        ut_iter it = ut_ll_iter(crawler->leafs);
        while (ut_iter_hasNext(&it)) {
            bake_project *project = ut_iter_next(&it);
            if (!strcmp(project->id, id)) {
                result = project;
                break;
            }
        }
    }

    return result;
}

int16_t bake_crawler_add(
    bake_config *config,
    bake_project *p)
{
    if (!p) {
        return 0;
    }

    if (!crawler->nodes) crawler->nodes = ut_rb_new(project_cmp, NULL);

    ut_try (bake_do_pre_discovery(config, p), NULL);

    if (p->type == BAKE_PACKAGE && p->public) {
        bake_project *found;
        if ((found = ut_rb_findOrSet(crawler->nodes, p->id, p)) && found != p) {
            if (found->path) {
                ut_throw(
                  "duplicate project '%s' found in '%s' (first found here: '%s')",
                  p->id,
                  found->path,
                  p->path);
                goto error;
            } else {
                /* This is a placeholder. Replace it with the actual project. */
                p->dependents = found->dependents;
                found->dependents = NULL;
                bake_project_free(found);
                ut_rb_set(crawler->nodes, p->id, p);
            }
        }
    } else {
        if (!crawler->leafs) crawler->leafs = ut_ll_new();
        ut_ll_append(crawler->leafs, p);
    }

    crawler->count ++;

    ut_trace("discovered project '%s' in '%s'", p->id, p->path);

    return 0;
error:
    return -1;
}

static
int16_t bake_crawler_finalize_project(
    bake_config *config,
    bake_project *p)
{
    ut_try (bake_do_post_discovery(config, p), NULL);

    /* Add dependency information */
    p->unresolved_dependencies = ut_ll_count(p->use);
    p->unresolved_dependencies += ut_ll_count(p->use_build);
    p->unresolved_dependencies += ut_ll_count(p->use_private);

    /* Add project to dependent lists of dependencies */
    ut_iter it = ut_ll_iter(p->use);
    while (ut_iter_hasNext(&it)) {
        char *use = ut_iter_next(&it);
        bake_crawler_addDependency(p, use);
    }

    /* Add project to dependent lists of private dependencies */
    it = ut_ll_iter(p->use_private);
    while (ut_iter_hasNext(&it)) {
        char *use = ut_iter_next(&it);
        bake_crawler_addDependency(p, use);
    }

    /* Add project to dependent lists of build dependencies */
    it = ut_ll_iter(p->use_build);
    while (ut_iter_hasNext(&it)) {
        char *use = ut_iter_next(&it);
        bake_crawler_addDependency(p, use);
    }

    ut_trace("initialized project '%s' in '%s'", p->id, p->path);

    return 0;
error:
    return -1;
}

static
int16_t bake_crawler_finalize(
    bake_config *config)
{
    /* Collect projects in list before finalizing. Finalization step may mutate
     * the tree, and cannot mutate tree while walking over it. */
    ut_ll projects = ut_ll_new();
    ut_iter it = ut_rb_iter(crawler->nodes);
    while (ut_iter_hasNext(&it)) {
        bake_project *p = ut_iter_next(&it);
        if (!p->path) {
            continue;
        }

        ut_ll_append(projects, p);
    }

    /* Now finalize projects */
    it = ut_ll_iter(projects);
    while (ut_iter_hasNext(&it)) {
        bake_project *p = ut_iter_next(&it);
        ut_try(bake_crawler_finalize_project(config, p), NULL);
    }

    ut_ll_free(projects);

    it = ut_ll_iter(crawler->leafs);
    while (ut_iter_hasNext(&it)) {
        bake_project *p = ut_iter_next(&it);
        ut_try(bake_crawler_finalize_project(config, p), NULL);
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_crawler_crawl(
    bake_config *config,
    const char *wd,
    const char *path)
{
    char *prev = strdup(ut_cwd());
    char *fullpath;
    if (path[0] != PATH_SEPARATOR_C) {
        fullpath = ut_asprintf("%s%c%s", wd, PATH_SEPARATOR_C, path);
        ut_path_clean(fullpath, fullpath);
    } else {
        fullpath = ut_strdup(path);
    }

    bool isProject = false;
    bake_project *p = NULL;

    if (ut_file_test(strarg("%s%cproject.json", fullpath, PATH_SEPARATOR_C))) {
        isProject = true;

        p = bake_project_new(fullpath, config);
        if (!p) {
            goto error;
        }

        if (bake_crawler_add(config, p)) {
            ut_warning("ignoring '%s' because of errors", fullpath);
        } else {
            if (ut_file_test("rakefile")) {
                ut_warning(
                    "path '%s' contains redundant rakefile",
                    fullpath);
            }
        }
    } else {
        if (ut_file_test("rakefile")) {
            ut_warning(
                "path '%s' contains rake-based project, skipping",
                fullpath);
            goto skip;
        }
    }

    ut_iter it;
    if (ut_dir_iter(fullpath, NULL, &it)) {
        ut_throw("failed to open directory '%s'", fullpath);
        goto error;
    }

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);

        if (file[0] == '.') {
            continue;
        }

        if (ut_isdir(strarg("%s%c%s", fullpath, PATH_SEPARATOR_C, file))) {

            /* If this is a bake project, filter out directories that have
             * special meaning. */
            if (isProject) {
                if (!strcmp(file, "src") ||
                    !strcmp(file, "include") ||
                    !strcmp(file, "config") ||
                    !strcmp(file, "data") ||
                    !strcmp(file, "test") ||
                    !strcmp(file, "etc") ||
                    !strcmp(file, "lib") ||
                    !strcmp(file, "bin") ||
                    !strcmp(file, "install") ||
                    !strcmp(file, "examples") ||
                    !strcmp(file, ".bake_cache") ||
                    (p && bake_project_should_ignore(p, file)))
                {
                    ut_debug("ignoring directory '%s'", file);
                    continue;
                }

                ut_debug("looking for projects in '%s'", file);

                /* TODO: ignore generated directories */
            } else {
                ut_debug("looking for projects in '%s'", file);
            }

            if (bake_crawler_crawl(config, fullpath, file)) {
                ut_iter_release(&it);
                goto error;
            }
        }
    }

skip:
    return 0;
error:
    if (prev) free(prev);
    return -1;
}

void bake_crawler_init(void)
{
    crawler = ut_calloc(sizeof(bake_crawler));
}

void bake_crawler_free(void)
{
    if (crawler->nodes) {
        ut_iter it = ut_rb_iter(crawler->nodes);
        while (ut_iter_hasNext(&it)) {
            bake_project *p = ut_iter_next(&it);
            bake_project_free(p);
        }
        ut_rb_free(crawler->nodes);
    }
    if (crawler->leafs) {
        ut_iter it = ut_ll_iter(crawler->leafs);
        while (ut_iter_hasNext(&it)) {
            bake_project *p = ut_iter_next(&it);
            bake_project_free(p);
        }
        ut_ll_free(crawler->leafs);
    }
    free (crawler);
}

uint32_t bake_crawler_count(void)
{
    return crawler->count;
}

uint32_t bake_crawler_search(
    bake_config *config,
    const char *path)
{
    int count = bake_crawler_count();

    if (ut_file_test(path)) {
        ut_try( bake_crawler_crawl(config, ".", path), NULL);
    } else {
        ut_throw("path '%s' not found", path);
        goto error;
    }

    /* Only report how many new projects were found in path */
    return bake_crawler_count() - count;
error:
    return -1;
}

static
const char* bake_project_kind_str(
    bake_project_type kind)
{
    switch(kind) {
    case BAKE_TOOL: return "tool";
    case BAKE_PACKAGE: return "package";
    case BAKE_APPLICATION: return "application";
    }
    return "???";
}

static
void bake_crawler_decrease_dependents(
    bake_project *p,
    ut_ll readyForBuild)
{
    if (p->dependents) {
        ut_iter dep_it = ut_ll_iter(p->dependents);
        while (ut_iter_hasNext(&dep_it)) {
            bake_project *dependent = ut_iter_next(&dep_it);
            dependent->unresolved_dependencies --;

            if (!dependent->unresolved_dependencies) {
                if (readyForBuild) {
                    ut_ll_append(readyForBuild, dependent);
                }
            }
        }
    }
}

static
int16_t bake_crawler_build_project(
    bake_config *config,
    const char *action_name,
    bake_crawler_cb action,
    bake_project *p,
    ut_ll readyForBuild)
{
    ut_ok(
        "#[grey]begin %s of %s '%s' in '%s'",
        action_name, bake_project_kind_str(p->type), p->id, p->path);

    if (action(config, p)) {
        ut_throw("bake interrupted by '%s' in '%s'", p->id, p->path);
        goto error;
    }

    if (p->changed) {
        ut_log(
            "%s '%s' in '%s'\n",
            action_name, p->id, p->path);
    } else if (p->language && strcmp(action_name, "foreach")) {
        ut_log(
            "#[grey]ready '%s'\n",
            p->id);
    }

    /* Decrease unresolved_dependencies of dependents */
    bake_crawler_decrease_dependents(p, readyForBuild);

    return 0;
error:
    return -1;
}

static
void bake_crawler_collect_projects(
    ut_iter *it,
    ut_ll readyForBuild)
{
    while (ut_iter_hasNext(it)) {
        bake_project *p = ut_iter_next(it);

        if (p->path && !p->unresolved_dependencies) {
            ut_ll_append(readyForBuild, p);
        }
    }
}

int16_t bake_crawler_walk(
    bake_config *config,
    const char *action_name,
    bake_crawler_cb action)
{
    ut_ll readyForBuild = ut_ll_new();
    uint32_t built = 0;

    /* Initialize dependency administration */
    ut_try( bake_crawler_finalize(config), NULL);

    /* Decrease unresolved dependencies for placeholder projects */
    if (crawler->nodes) {
        ut_iter it = ut_rb_iter(crawler->nodes);
        while (ut_iter_hasNext(&it)) {
            bake_project *p = ut_iter_next(&it);
            if (!p->path) {
                bake_crawler_decrease_dependents(p, NULL);
            }
        }
    }

    /* Collect initial projects */
    if (crawler->nodes) {
        ut_iter it = ut_rb_iter(crawler->nodes);
        bake_crawler_collect_projects(&it, readyForBuild);
    }

    if (crawler->leafs) {
        ut_iter it = ut_ll_iter(crawler->leafs);
        bake_crawler_collect_projects(&it, readyForBuild);
    }

    /* Walk projects (when dependencies are resolved the list will populate) */
    bake_project *p;
    while ((p = ut_ll_takeFirst(readyForBuild))) {
        ut_try (
            bake_crawler_build_project(
                config, action_name, action, p, readyForBuild), NULL);
        built ++;
    }

    /* If there are still unbuilt projects there must be a cycle in the graph */
    if (built != crawler->count) {
        ut_throw(
            "project dependency graph contains cycles (%d built vs %d total)",
            built, crawler->count);
        goto error;
    }

    ut_ll_free(readyForBuild);

    return 0;
error:
    return -1;
}
