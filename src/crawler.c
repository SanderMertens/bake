/* Copyright (c) 2010-2018 the corto developers
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

struct bake_crawler_s {
    corto_rb nodes; /* tree optimizes looking up dependencies */
    corto_ll leafs; /* projects that cannot act as dependencies */
    uint32_t count;
    bake_config *cfg;
};

static
int project_cmp(void *ctx, const void* key1, const void* key2) {
    return strcmp(key1, key2);
}

static
void bake_crawler_addDependency(
    bake_crawler _this,
    bake_project *p,
    char *use)
{
    bake_project *dep = corto_rb_find(_this->nodes, use);
    if (!dep) {
        /* Create placeholder */
        dep = bake_project_new(NULL, NULL);
        dep->id = corto_strdup(use);
        corto_rb_set(_this->nodes, dep->id, dep);
    }

    if (!dep->dependents) {
        dep->dependents = corto_ll_new();
    }

    corto_ll_append(dep->dependents, p);
}

bake_project* bake_crawler_addProject(
    bake_crawler _this,
    const char *path)
{
    bake_project *p = bake_project_new(path, _this->cfg);
    if (!p) {
        return NULL;
    }

    if (!_this->nodes) _this->nodes = corto_rb_new(project_cmp, NULL);

    if (p->kind == BAKE_PACKAGE && p->public) {
        bake_project *found;
        if ((found = corto_rb_findOrSet(_this->nodes, p->id, p)) && found != p) {
            if (found->path) {
                corto_throw(
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
                corto_rb_set(_this->nodes, p->id, p);
            }
        }
    } else {
        if (!_this->leafs) _this->leafs = corto_ll_new();
        corto_ll_append(_this->leafs, p);
    }

    /* Initialize project before building dependency administration */
    if (p->language) {
        bake_language *l = bake_language_get(p->language);
        if (l) { /* During setup language may not yet be installed */
            bake_language_init(l, p);
        } else {
            corto_catch();
        }
    }

    /* Add dependency information */
    p->unresolved_dependencies = corto_ll_count(p->use);
    p->unresolved_dependencies += corto_ll_count(p->use_build);
    p->unresolved_dependencies += corto_ll_count(p->use_private);

    /* Add project to dependent lists of dependencies */
    corto_iter it = corto_ll_iter(p->use);
    while (corto_iter_hasNext(&it)) {
        char *use = corto_iter_next(&it);
        bake_crawler_addDependency(_this, p, use);
    }

    /* Add project to dependent lists of private dependencies */
    it = corto_ll_iter(p->use_private);
    while (corto_iter_hasNext(&it)) {
        char *use = corto_iter_next(&it);
        bake_crawler_addDependency(_this, p, use);
    }

    /* Add project to dependent lists of build dependencies */
    it = corto_ll_iter(p->use_build);
    while (corto_iter_hasNext(&it)) {
        char *use = corto_iter_next(&it);
        bake_crawler_addDependency(_this, p, use);
    }

    _this->count ++;

    corto_trace("found project '%s'", p->id);

    return p;
error:
    return NULL;
}

static
int16_t bake_crawler_crawl(
    bake_crawler _this,
    const char *wd,
    const char *path)
{
    char *prev = strdup(corto_cwd());
    char *fullpath;
    if (path[0] != '/') {
        fullpath = corto_asprintf("%s/%s", wd, path);
        corto_path_clean(fullpath, fullpath);
    } else {
        fullpath = corto_strdup(path);
    }

    bool isProject = false;
    bake_project *p = NULL;

    if (corto_chdir(path)) {
        goto error;
    }

    if (corto_file_test("project.json")) {
        isProject = true;
        if (!(p = bake_crawler_addProject(_this, fullpath))) {
            goto error;
        }

        if (corto_file_test("rakefile")) {
            corto_warning(
                "path '%s' contains redundant rakefile",
                fullpath);
        }
    } else {
        if (corto_file_test("rakefile")) {
            corto_warning(
                "path '%s' contains rake-based project, skipping",
                fullpath);
            goto skip;
        }
    }

    corto_iter it;
    if (corto_dir_iter(".", NULL, &it)) {
        corto_throw("failed to open directory '%s'", fullpath);
        goto error;
    }

    while (corto_iter_hasNext(&it)) {
        char *file = corto_iter_next(&it);

        if (corto_isdir(file)) {


            /* If this is a corto project, filter out directories that have
             * special meaning. */
            if (isProject) {
                if (file[0] == '.' ||
                    !strcmp(file, "src") ||
                    !strcmp(file, "include") ||
                    !strcmp(file, "config") ||
                    !strcmp(file, "data") ||
                    !strcmp(file, "test") ||
                    !strcmp(file, "etc") ||
                    !strcmp(file, "lib") ||
                    !strcmp(file, "bin") ||
                    !strcmp(file, "install") ||
                    !strcmp(file, ".bake_cache"))
                {
                    corto_debug("ignoring directory '%s'", file);
                    continue;
                }

                corto_trace("looking for projects in '%s'", file);

                if (p->managed && p->language) {
                    if (!strcmp(file, "c") || !strcmp(file, "cpp")) {
                        continue;
                    }
                }
            } else {
                corto_trace("looking for projects in '%s'", file);
            }

            if (bake_crawler_crawl(_this, fullpath, file)) {
                corto_iter_release(&it);
                goto error;
            }
        }
    }

skip:
    if (corto_chdir(prev)) {
        corto_throw("failed to restore directory to '%s'", prev);
        goto error;
    }

    return 0;
error:
    if (prev) free(prev);
    return -1;
}

bake_crawler bake_crawler_new(
    bake_config *cfg)
{
    bake_crawler result = corto_calloc(sizeof(struct bake_crawler_s));
    result->cfg = cfg;
    return result;
}

void bake_crawler_free(bake_crawler _this)
{
    if (_this->nodes) {
        corto_iter it = corto_rb_iter(_this->nodes);
        while (corto_iter_hasNext(&it)) {
            bake_project *p = corto_iter_next(&it);
            bake_project_free(p);
        }
        corto_rb_free(_this->nodes);
    }
    if (_this->leafs) {
        corto_iter it = corto_ll_iter(_this->leafs);
        while (corto_iter_hasNext(&it)) {
            bake_project *p = corto_iter_next(&it);
            bake_project_free(p);
        }
        corto_ll_free(_this->leafs);
    }
    free (_this);
}

uint32_t bake_crawler_count(
    bake_crawler _this)
{
    return (_this->nodes ? corto_rb_count(_this->nodes) : 0) +
        (_this->leafs ? corto_ll_count(_this->leafs) : 0);
}

int16_t bake_crawler_search(
    bake_crawler _this,
    const char *path)
{
    int ret = 0;
    int count = bake_crawler_count(_this);

    if (corto_file_test(path)) {
        ret = bake_crawler_crawl(_this, ".", path);
    } else {
        corto_throw("path '%s' not found", path);
        goto error;
    }

    if (!ret && bake_crawler_count(_this) == count) {
        corto_trace("no projects found in path '%s'", path);
    }

    return ret;
error:
    return -1;
}

static
const char* bake_project_kind_str(
    bake_project_kind kind)
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
    corto_ll readyForBuild)
{
    if (p->dependents) {
        corto_iter dep_it = corto_ll_iter(p->dependents);
        while (corto_iter_hasNext(&dep_it)) {
            bake_project *dependent = corto_iter_next(&dep_it);
            dependent->unresolved_dependencies --;
            if (!dependent->unresolved_dependencies) {
                if (readyForBuild) {
                    corto_ll_append(readyForBuild, dependent);
                }
            }
        }
    }
}

static
int16_t bake_crawler_build_project(
    bake_crawler _this,
    const char *action_name,
    bake_crawler_cb action,
    bake_project *p,
    void *ctx,
    corto_ll readyForBuild)
{
    corto_ok(
        "begin %s %s '%s' in '%s'",
        action_name, bake_project_kind_str(p->kind), p->id, p->path);

    char *prev = strdup(corto_cwd());
    if (corto_chdir(p->path)) {
        free(prev);
        goto error;
    }

    if (!action(_this, p, ctx)) {
        corto_throw("build interrupted by '%s' in '%s'", p->id, p->path);
        free(prev);
        goto error;
    }

    if (p->changed) {
        corto_info(
            "#[green]√#[normal] %s %s '%s' in '%s'",
            action_name, bake_project_kind_str(p->kind), p->id, p->path);
    } else {
        corto_info(
            "  #[grey]up to date#[normal] '%s'",
            p->id);
    }
    if (corto_chdir(prev)) {
        free(prev);
        goto error;
    }
    free(prev);

    /* Decrease unresolved_dependencies of dependents */
    bake_crawler_decrease_dependents(p, readyForBuild);

    return 0;
error:
    return -1;
}

static
void bake_crawler_collect_projects(
    bake_crawler _this,
    corto_iter *it,
    corto_ll readyForBuild)
{
    while (corto_iter_hasNext(it)) {
        bake_project *p = corto_iter_next(it);
        if (p->path && !p->unresolved_dependencies) {
            corto_ll_append(readyForBuild, p);
        }
    }
}

int16_t bake_crawler_walk(
    bake_crawler _this,
    const char *action_name,
    bake_crawler_cb action,
    void *ctx)
{
    corto_ll readyForBuild = corto_ll_new();
    uint32_t built = 0;

    /* Decrease unresolved dependencies for placeholder projects */
    if (_this->nodes) {
        corto_iter it = corto_rb_iter(_this->nodes);
        while (corto_iter_hasNext(&it)) {
            bake_project *p = corto_iter_next(&it);
            if (!p->path) {
                bake_crawler_decrease_dependents(p, NULL);
            }
        }
    }

    /* Collect initial projects */
    if (_this->nodes) {
        corto_iter it = corto_rb_iter(_this->nodes);
        bake_crawler_collect_projects(_this, &it, readyForBuild);
    }

    if (_this->leafs) {
        corto_iter it = corto_ll_iter(_this->leafs);
        bake_crawler_collect_projects(_this, &it, readyForBuild);
    }

    /* Walk projects (when dependencies are resolved the list will populate) */
    bake_project *p;
    while ((p = corto_ll_takeFirst(readyForBuild))) {
        if (bake_crawler_build_project(_this, action_name, action, p, ctx, readyForBuild)) {
            corto_throw(NULL);
            goto error;
        }
        built ++;
    }

    /* If there are still unbuilt projects there must be a cycle in the graph */
    if (built != _this->count) {
        corto_throw("project dependency graph contains cycles (%d built vs %d total)",
            built, _this->count);
        goto error;
    }

    corto_ll_free(readyForBuild);

    return 1;
error:
    return 0;
}
