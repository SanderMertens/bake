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

#include <include/bake.h>

struct bake_crawler_s {
    corto_ll projects;
};

static
int16_t bake_crawler_addProject(
    bake_crawler _this,
    char *path)
{
    bake_project p = bake_project_new();
    if (!_this->projects) {
        _this->projects = corto_ll_new();
    }

    corto_ll_append(_this->projects, p);

    return 0;
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

    if (corto_chdir(fullpath)) {
        goto error;
    }

    if (corto_file_test("project.json")) {
        isProject = true;
        if (bake_crawler_addProject(_this, fullpath)) {
            goto error;
        }
    }

    corto_ll files = corto_opendir(path);
    corto_iter it = corto_ll_iter(files);

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
                goto error;
            }
        }
    }

    corto_closedir(files);

    return 0;
error:
    if (prev) free(prev);
    return -1;
}

bake_crawler bake_crawler_new(void)
{
    return corto_calloc(sizeof(struct bake_crawler_s));
}

int16_t bake_crawler_search(
    bake_crawler _this, 
    const char *path)
{
    return bake_crawler_crawl(_this, "", path);
}

int16_t bake_crawler_walk(
    bake_crawler _this, 
    bake_crawler_cb action, 
    void *ctx)
{

}