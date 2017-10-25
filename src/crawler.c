/* Copyright (c) 2010-2017 the corto developers
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
    corto_ll projects;
};

bake_project* bake_crawler_addProject(
    bake_crawler _this,
    const char *path)
{
    bake_project *p = bake_project_new(path);

    if (!_this->projects) {
        _this->projects = corto_ll_new();
    }

    corto_ll_append(_this->projects, p);

    return p;
}

static 
int16_t bake_crawler_crawl(
    bake_crawler _this,
    const char *wd,
    const char *path)
{
    char *prev = strdup(corto_cwd());
    char *fullpath = corto_asprintf("%s/%s", wd, path);
    bool isProject = false;

    corto_path_clean(fullpath, fullpath);

    if (corto_chdir(path)) {
        goto error;
    }

    if (corto_file_test("project.json")) {
        isProject = true;
        if (!bake_crawler_addProject(_this, fullpath)) {
            goto error;
        }
    }

    corto_iter it;
    if (corto_dir_iter(".", NULL, &it)) {
        corto_seterr("failed to open directory '%s': %s", fullpath, corto_lasterr());
        goto error;
    }

    while (corto_iter_hasNext(&it)) {
        char *file = corto_iter_next(&it);

        if (corto_isdir(file)) {

            /* If this is a corto project, filter out directories that have
             * special meaning. */
            if (isProject) {
                if (!strcmp(file, "src") ||
                    !strcmp(file, "include") ||
                    !strcmp(file, "config") ||
                    !strcmp(file, "data") ||
                    !strcmp(file, "test") ||
                    !strcmp(file, "etc") ||
                    !strcmp(file, "lib") ||
                    !strcmp(file, "install") ||
                    !strcmp(file, ".corto"))
                {
                    continue;
                }
            }

            if (bake_crawler_crawl(_this, fullpath, file)) {
                corto_iter_release(&it);
                goto error;
            }
        }
    }

    if (corto_chdir(prev)) {
        corto_seterr("failed to restore directory to '%s': %s", prev, corto_lasterr());
        goto error;
    }

    return 0;
error:
    if (prev) free(prev);
    return -1;
}

bake_crawler bake_crawler_new(void)
{
    return corto_calloc(sizeof(struct bake_crawler_s));
}

void bake_crawler_free(bake_crawler _this)
{
    if (_this->projects) {
        corto_iter it = corto_ll_iter(_this->projects);
        while (corto_iter_hasNext(&it)) {
            bake_project *p = corto_iter_next(&it);
        }
    }
    free (_this);
}

int16_t bake_crawler_search(
    bake_crawler _this, 
    const char *path)
{
    return bake_crawler_crawl(_this, ".", path);
}

uint32_t bake_crawler_count(
    bake_crawler _this)
{
    return _this->projects ? corto_ll_size(_this->projects) : 0;
}

int16_t bake_crawler_walk(
    bake_crawler _this, 
    bake_crawler_cb action, 
    void *ctx)
{
    if (_this->projects) {
        corto_iter it = corto_ll_iter(_this->projects);
        while (corto_iter_hasNext(&it)) {
            bake_project *p = corto_iter_next(&it);
            corto_info("building '%s'", p->id);
            corto_log_push(p->id);
            corto_ok("entering directory '%s'", p->path);
            char *prev = strdup(corto_cwd());
            if (corto_chdir(p->path)) {
                free(prev);
                goto error;
            }

            if (!action(_this, p, ctx)) {
                corto_seterr("project interrupted build");
                corto_log_pop();
                free(prev);
                goto error;
            }

            corto_ok("leaving directory '%s'", p->path);
            if (corto_chdir(prev)) {
                free(prev);
                goto error;
            }
            free(prev);
            corto_log_pop();
        }
    }

    return 1;
error:
    return 0;
}


