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

struct bake_crawler {
    ut_rb nodes; /* tree optimizes looking up dependencies */
    ut_ll leafs; /* projects that cannot act as dependencies */
    uint32_t count;
};

static bake_crawler *crawler;

typedef int16_t (*bake_dependency_action)(
    bake_config *cfg,
    bake_project *project,
    const char *dependency);

static
int project_cmp(
    void *ctx,
    const void* key1,
    const void* key2)
{
    return strcmp(key1, key2);
}

/* Build the list of dependees during project finalization */
static
int16_t bake_crawler_addDependency(
    bake_config *config,
    bake_project *p,
    const char *use)
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

    return 0;
}

/* Lookup a dependency in the bake environment (used for recursive builds) */
static
int16_t bake_crawler_lookupDependency(
    bake_config *config,
    bake_project *p,
    const char *use)
{    
    /* Don't try to rebuild bake dependencies (like bake.util) */
    if (!strncmp(use, "bake.", 5)) {
        return 0;
    }

    bake_project *dep = ut_rb_find(crawler->nodes, use);

    if (!dep || !dep->path) {
        const char *src = ut_locate(use, NULL, UT_LOCATE_DEVSRC);
        if (!src) {
            src = ut_locate(use, NULL, UT_LOCATE_SOURCE);
        }

        if (src) {
            dep = bake_project_new(src, config);
            if (!dep) {
                ut_warning("ignoring '%s' because of errors", src);
            } else {
                if (bake_crawler_add(config, dep)) {
                    ut_warning("ignoring '%s' because of errors", src);
                } else {
                    dep->recursive = true;
                }
            }
        } else {
            /* If source for dependency is not available, that doesn't mean we
             * have no binary. If a binary exists in the environment, there is
             * no need to take further action */
            if (!ut_locate(use, NULL, UT_LOCATE_BIN)) {

                /* If the binary doesn't exist, check if the project can be
                 * found in any of the registered repositories */
                bake_repository *repo = bake_find_repository(config, use);
                if (repo) {
                    /* If a repository is found, install it. At this point the
                     * project repository is guaranteed to be found in bake, so
                     * the only reason this function can fail is if the clone
                     * fails, which would be a real error. */
                    ut_try( bake_install(config, use), NULL);

                    /* Reset locate cache, as project has just been installed */
                    ut_locate_reset(use);
                } else {
                    /* Nothing to be done here. It is possible that a dependency
                     * is not yet discoverable at this point, as is the case
                     * with projects that are generated during the build. At
                     * this point we cannot know for sure if a dependency is
                     * actually missing, so don't throw an error. */
                    ut_trace(
                        "dependency '%s' not found in environment or bundles, possible error", 
                        use); 
                }
            } else {
                ut_trace("binary found for dependency '%s' (no source)", use);
            }
        }
    }

    return 0;
error:
    return -1;
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
int16_t bake_crawler_walk_dependencies(
    bake_config *config,
    bake_project *p,
    bake_dependency_action action)
{
    /* Add dependency information */
    p->unresolved_dependencies = ut_ll_count(p->use);
    p->unresolved_dependencies += ut_ll_count(p->use_build);
    p->unresolved_dependencies += ut_ll_count(p->use_private);
    p->unresolved_dependencies += ut_ll_count(p->use_runtime);

    /* Add project to dependent lists of dependencies */
    ut_iter it = ut_ll_iter(p->use);
    while (ut_iter_hasNext(&it)) {
        char *use = ut_iter_next(&it);
        ut_try( action(config, p, use), NULL);
    }

    /* Add project to dependent lists of private dependencies */
    it = ut_ll_iter(p->use_private);
    while (ut_iter_hasNext(&it)) {
        char *use = ut_iter_next(&it);
        ut_try( action(config, p, use), NULL);
    }

    /* Add project to dependent lists of build dependencies */
    it = ut_ll_iter(p->use_build);
    while (ut_iter_hasNext(&it)) {
        char *use = ut_iter_next(&it);
        ut_try( action(config, p, use), NULL);
    }

    /* Add project to dependent lists of runtime dependencies */
    it = ut_ll_iter(p->use_runtime);
    while (ut_iter_hasNext(&it)) {
        char *use = ut_iter_next(&it);
        ut_try( action(config, p, use), NULL);
    }

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

    ut_try( bake_crawler_walk_dependencies(
        config, p, bake_crawler_addDependency), NULL);

    ut_trace("initialized project '%s' in '%s'", p->id, p->path);

    return 0;
error:
    return -1;
}

static
ut_ll bake_crawler_collect_projects()
{
    ut_ll projects = ut_ll_new();
    ut_iter it = ut_rb_iter(crawler->nodes);
    while (ut_iter_hasNext(&it)) {
        bake_project *p = ut_iter_next(&it);
        if (!p->path) {
            continue;
        }

        ut_ll_append(projects, p);
    }

    return projects;
}

static
int16_t bake_crawler_finalize(
    bake_config *config)
{
    /* Collect projects in list before finalizing. Finalization step may mutate
     * the tree, and cannot mutate tree while walking over it. */
    ut_ll projects = bake_crawler_collect_projects();

    /* Now finalize projects */
    ut_iter it = ut_ll_iter(projects);
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
    if (ut_path_is_relative(path)) {
        fullpath = ut_asprintf("%s"UT_OS_PS"%s", wd, path);
        ut_path_clean(fullpath, fullpath);
    } else {
        fullpath = ut_strdup(path);
    }

    bool isProject = false;
    bake_project *p = NULL;

    if (ut_file_test(strarg("%s"UT_OS_PS"project.json", fullpath))) {
        isProject = true;

        p = bake_project_new(fullpath, config);
        if (!p) {
            ut_warning("ignoring '%s' because of errors", fullpath);
        } else {
            if (bake_crawler_add(config, p)) {
                ut_warning("ignoring '%s' because of errors", fullpath);
            }
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

        if (ut_isdir(strarg("%s"UT_OS_PS"%s", fullpath, file))) {

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
                    !strcmp(file, "bake") ||
                    !strcmp(file, ".bake_cache") ||
                    (p && bake_project_should_ignore(p, file)))
                {
                    ut_debug("ignoring directory '%s'", file);
                    continue;
                }

                ut_debug("looking for projects in '%s'", file);

                /* TODO: ignore generated directories */

            /* Never try to build bake with bake, in case it is found in the source tree */
            } else if (!strcmp(file, "bake")) {
                ut_debug("ignoring directory 'bake'");
                continue;
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

static
int16_t bake_crawler_recursive(
    bake_config *config)
{
    /* First collect unresolved dependencies for leafs (applications) */
    ut_iter it = ut_ll_iter(crawler->leafs);
    while (ut_iter_hasNext(&it)) {
        bake_project *project = ut_iter_next(&it);
        ut_try( 
            bake_crawler_walk_dependencies(
                config, project, bake_crawler_lookupDependency), NULL);

    }

    /* Collect packages in tree before search, as it may mutate
     * the tree, and cannot mutate tree while walking over it. */
    ut_ll projects = bake_crawler_collect_projects();

    it = ut_ll_iter(projects);
    while (ut_iter_hasNext(&it)) {
        bake_project *project = ut_iter_next(&it);
        ut_try( 
            bake_crawler_walk_dependencies(
                config, project, bake_crawler_lookupDependency), NULL);
    }

    ut_ll_free(projects);

    return 0;
error:
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
    const char *path,
    bool recursive)
{
    int count = bake_crawler_count();

    /* Test if in bake repository root. Bake should not attempt to build itself. */
    if (ut_file_test("%s/bake", path) == 1 && ut_file_test("%s/include/bake.h", path)) {
        ut_throw("you shall not build bake with bake");
        goto error;
    }

    if (ut_file_test(path)) {
        ut_try( bake_crawler_crawl(config, ".", path), NULL);

        /* If crawling recursively, discover unresolved depdendencies. Do this
         * after discovering projects in the provided directory, so these take
         * precedence (in case one project is found in two locations). */
        if (recursive) {
            ut_trace("recursively looking for dependencies");
            ut_try( bake_crawler_recursive(config), NULL);
        }
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
    if (!ut_getenv("BAKE_SETUP")) {
        bake_message(UT_LOG, action_name, "#[green]%s#[reset] %s #[grey]=>#[reset] '%s'", 
            bake_project_type_str(p->type), p->id, p->path);
    }

    if (action(config, p)) {
        ut_raise();
        bake_message(UT_ERROR, "error", "build interrupted for %s in %s", p->id, p->path);
        goto error;
    }

    /* Decrease unresolved_dependencies of dependents */
    bake_crawler_decrease_dependents(p, readyForBuild);

    return 0;
error:
    return -1;
}

static
void bake_crawler_collect_ready_for_build(
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
    int16_t result = 0;

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
        bake_crawler_collect_ready_for_build(&it, readyForBuild);
    }

    if (crawler->leafs) {
        ut_iter it = ut_ll_iter(crawler->leafs);
        bake_crawler_collect_ready_for_build(&it, readyForBuild);
    }

    /* Walk projects (when dependencies are resolved the list will populate) */
    bake_project *p;
    while ((p = ut_ll_takeFirst(readyForBuild))) {
        if (bake_crawler_build_project(
                config, action_name, action, p, readyForBuild))
        {
            ut_error("project '%s' built with errors, skipping", p->id);
            result = -1;
        }
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

    return result;
error:
    return -1;
}
