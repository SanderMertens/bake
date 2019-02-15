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
    result->path = ut_strdup(file->path);
    result->file_path = ut_strdup(file->file_path);
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
            if (f->path) free(f->path);
            if (f->file_path) free(f->file_path);
            free(f);
        }
        ut_ll_free(fl->files);
    }
    free(fl);
}

static
bake_file* bake_filelist_add_intern(
    bake_filelist *fl,
    const char *path,
    const char *filename,
    time_t timestamp)
{
    if (timestamp < 0) {
        ut_throw(NULL);
        goto error;
    }

    bake_file *bfile = malloc(sizeof(bake_file));
    bfile->name = ut_strdup(filename);
    bfile->path = ut_strdup(path);

    if (!ut_path_is_relative(filename)) {
        bfile->file_path = ut_strdup(filename);
    } else {
        bfile->file_path = ut_asprintf("%s%c%s", path, UT_OS_PS[0],  filename);
    }
    bfile->timestamp = timestamp;

    ut_ll_append(fl->files, bfile);

    if (timestamp) {
        ut_trace("#[grey]%s (modified=%d, path='%s')", filename, timestamp, path);
    } else {
        ut_trace("#[grey]%s (modified=0, path='%s')", filename, path);
    }

    return bfile;
error:
    return NULL;
}

static
int16_t bake_filelist_populate(
    bake_filelist *fl,
    const char *path,
    const char *pattern)
{
    ut_iter it;
    ut_try (ut_dir_iter(path, pattern, &it), NULL);

    char *clean_path = ut_strdup(path);
    ut_path_clean(clean_path, clean_path);

    while (ut_iter_hasNext(&it)) {
        const char *file = ut_iter_next(&it);
        char *file_path = ut_asprintf("%s"UT_OS_PS"%s", path, file);

        time_t last_modified = ut_lastmodified(file_path);
        if (last_modified == -1) {
            ut_catch();
        }

        free(file_path);

        bake_filelist_add_intern(
            fl, path, file, last_modified);
    }

    free (clean_path);

    return 0;
error:
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
    if (!path) path = ".";
    result->path = ut_strdup(path);
    result->pattern = ut_strdup(pattern);
    result->files = ut_ll_new();
    result->set = bake_filelist_set_cb;

    if (pattern) {
        ut_try( bake_filelist_populate(result, path, pattern), NULL);
    }

    return result;
error:
    if (result) bake_filelist_free(result);
    return NULL;
}

bake_file* bake_filelist_add_file(
    bake_filelist *fl,
    const char *filepath,
    const char *file)
{
    char *path;
    time_t lastmodified = 0;

    if (!filepath) {
        filepath = fl->path;
    }

    if ((file && !ut_path_is_relative(file)) || (fl->path && !strcmp(fl->path, "."))) {
        path = ut_strdup(file);
    } else {
        path = ut_asprintf("%s"UT_OS_PS"%s", filepath, file);
    }

    if (ut_file_test(path) == 1) {
        lastmodified = ut_lastmodified(path);
    }

    char *name = strrchr(path, UT_OS_PS[0]);
    if (name) {
        *name = '\0';
        name ++;
    } else {
        free(path);
        path = (char*)filepath;
        name = (char*)file;
    }

    bake_file *result = bake_filelist_add_intern(fl, path, name, lastmodified);
    if (path != fl->path && path != filepath) free(path);
    return result;
}

int16_t bake_filelist_add_pattern(
    bake_filelist *fl,
    const char *path,
    const char *pattern)
{
    char *search_path = (char*)path;
    if (fl->path) {
        search_path = ut_asprintf("%s"UT_OS_PS"%s", fl->path, path);
    }
    int16_t result = bake_filelist_populate(fl, search_path, pattern);
    if (search_path != path) free(search_path);
    return result;
}

int16_t bake_filelist_merge(
    bake_filelist *fl,
    bake_filelist *src)
{
    ut_iter it = ut_ll_iter(src->files);
    while (ut_iter_hasNext(&it)) {
        ut_ll_append(fl->files, bake_file_copy(ut_iter_next(&it)));
    }

    return 0;
error:
    return -1;
}

int bake_filelist_count(
    bake_filelist *fl)
{
    return ut_ll_count(fl->files);
}
