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

extern ut_tls BAKE_FILELIST_KEY;

bake_file* bake_file_copy(
    bake_file *file)
{
    bake_file *result = malloc(sizeof(bake_file));
    result->name = ut_strdup(file->name);
    result->offset = file->offset ? ut_strdup(file->offset) : NULL;
    result->timestamp = file->timestamp;
    return result;
}

void bake_filelist_free(
    bake_filelist *fl)
{
    if (fl->pattern) {
        free(fl->pattern);
    }
    if (fl->files) {
        ut_iter it = ut_ll_iter(fl->files);
        while (ut_iter_hasNext(&it)) {
            bake_file *f = ut_iter_next(&it);
            if (f->name) free(f->name);
            free(f);
        }
        ut_ll_free(fl->files);
    }
    free(fl);
}

static
bake_file* bake_filelist_add_intern(
    bake_filelist *fl,
    const char *filename,
    const char *offset,
    time_t timestamp)
{
    if (timestamp < 0) {
        ut_throw(NULL);
        goto error;
    }

    const char *relative_file = filename;
    if (offset) {
        relative_file += strlen(offset);
    }

    if (relative_file[0] == '/') {
        relative_file ++;
    }

    bake_file *bfile = malloc(sizeof(bake_file));
    bfile->name = ut_strdup(relative_file);
    bfile->offset = offset ? ut_strdup(offset) : NULL;
    bfile->timestamp = timestamp;

    if (!fl->files) {
        fl->files = ut_ll_new();
    }
    ut_ll_append(fl->files, bfile);

    if (timestamp) {
        ut_debug("add '%s' with timestamp %d", filename, timestamp);
    } else {
        ut_debug("add '%s' with timestamp 0", filename);
    }

    return bfile;
error:
    return NULL;
}

static
int16_t bake_filelist_populate(
    bake_filelist *fl,
    const char *offset,
    const char *pattern)
{
    char *dir = NULL;
    bool skip = false;

    /* Optimize filter evaluation by extracting static path from pattern */
    const char *ptr = pattern, *end = pattern;
    char ch;
    while ((ch = *ptr)) {
        if (ch == '/') {
            if (ptr[1] == '/') {
                end = ptr;
                break;
            } else {
                end = ptr;
            }
        } else if (ut_expr_isOperator(ch)) {
            break;
        }
        ptr ++;
    }

    ut_iter it;
    if (end != pattern) {
        dir = strdup(pattern);
        dir[end - pattern] = '\0';

        if (end[-1] == '/') {
            end --;
        } else {
            end ++;
            if (!*end) {
                end = "*";
            }
        }

        char *cur = ut_asprintf("%s/%s", offset, dir);

        ut_trace("match pattern '%s' in '%s'", end, cur);
        if (ut_file_test(cur)) {
            if (ut_dir_iter(cur, end, &it)) {
                ut_throw(NULL);
                goto error;
            }
        } else {
            ut_trace("directory '%s' does not exist, skipping pattern", cur);
            skip = true;
        }

        free(cur);
    } else {
        ut_trace("match pattern '%s' in '%s'", pattern, offset);
        if (ut_dir_iter(offset, pattern, &it)) {
            ut_throw(NULL);
            goto error;
        }
    }

    /* Iterate directory, add matched files */
    if (!skip) {
        int count = 0;
        while (ut_iter_hasNext(&it)) {
            char *file = ut_iter_next(&it);
            char *path = (dir && dir[0]) ? ut_asprintf("%s/%s", dir, file) : strdup(file);
            if (!bake_filelist_add_intern(fl, path, offset, ut_lastmodified(path))) {
                free(path);
                ut_throw(NULL);
                goto error;
            }
            free(path);
            count ++;
        }

        if (!count && !ut_expr_hasOperators(end)) {
            char *path = dir ? ut_asprintf("%s/%s", dir, end) : strdup(end);
            if (!bake_filelist_add_intern(fl, path, offset, 0)) {
                free(path);
                ut_throw(NULL);
                goto error;
            }
            free(path);
        }
    }

   if (dir) free(dir);
    return 0;
error:
    if (dir) free(dir);
    return -1;
}

ut_iter bake_filelist_iter(
    bake_filelist *fl)
{
    return ut_ll_iterAlloc(fl->files);
}

int16_t bake_filelist_set(
    bake_filelist *fl,
    const char *pattern)
{
    ut_assert(fl->pattern == NULL, "setPattern invalid: filelist already set a pattern");
    fl->pattern = strdup(pattern);
    if (bake_filelist_populate(fl, NULL, fl->pattern)) {
        free(fl->pattern);
        fl->pattern = NULL;
        return -1;
    } else {
        return 0;
    }
}

static
int16_t bake_filelist_set_cb(
    const char *pattern)
{
    bake_filelist *fl = ut_tls_get(BAKE_FILELIST_KEY);
    return bake_filelist_set(fl, pattern);
}

bake_filelist* bake_filelist_new(
    const char *path,
    const char *pattern)
{
    bake_filelist *result = malloc(sizeof(bake_filelist));
    result->pattern = pattern ? strdup(pattern) : NULL;
    result->files = ut_ll_new();
    result->set = bake_filelist_set_cb;

    /* Extract starting directory from pattern */
    if (pattern) {
        if (bake_filelist_populate(result, NULL, result->pattern)) {
            ut_throw(NULL);
            goto error;
        }
    }

    return result;
error:
    if (result) bake_filelist_free(result);
    return NULL;
}

bake_file* bake_filelist_add(
    bake_filelist *fl,
    const char *file)
{
    ut_assert(fl != NULL, "passed NULL filelist to filelist_add");
    ut_assert(file != NULL, "passed NULL file to filelist_add");
    if (ut_file_test(file)) {
        return bake_filelist_add_intern(fl, file, NULL, ut_lastmodified(file));
    } else {
        return bake_filelist_add_intern(fl, file, NULL, 0);
    }
}

int16_t bake_filelist_addPattern(
    bake_filelist *fl,
    const char *offset,
    const char *pattern)
{
    ut_assert(fl != NULL, "passed NULL to filelist_addPattern");
    if (!fl) {
        ut_throw("no filelist specified");
        goto error;
    }

    if (bake_filelist_populate(fl, offset, pattern)) {
        ut_throw(NULL);
        goto error;
    }

    return 0;
error:
    return -1;
}

int16_t bake_filelist_addList(
    bake_filelist *fl,
    bake_filelist *src)
{
    ut_assert(fl != NULL, "passed NULL to filelist_addList");
    if (!src) {
        ut_throw("no source filelist specified");
        goto error;
    }

    ut_iter it = ut_ll_iter(src->files);
    while (ut_iter_hasNext(&it)) {
        ut_ll_append(fl->files, bake_file_copy(ut_iter_next(&it)));
    }

    return 0;
error:
    return -1;
}

uint64_t bake_filelist_count(
    bake_filelist *fl)
{
    return ut_ll_count(fl->files);
}
