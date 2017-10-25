
#include "bake.h"

extern corto_tls BAKE_FILELIST_KEY;

bake_file* bake_file_copy(
    bake_file *file)
{
    bake_file *result = malloc(sizeof(bake_file));
    result->name = corto_strdup(file->name);
    result->offset = file->offset ? corto_strdup(file->offset) : NULL;
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
        corto_iter it = corto_ll_iter(fl->files);
        while (corto_iter_hasNext(&it)) {
            bake_file *f = corto_iter_next(&it);
            if (f->name) free(f->name);
            free(f);
        }
        corto_ll_free(fl->files);
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
        goto error;
    }

    bake_file *bfile = corto_alloc(sizeof(bake_file));
    bfile->name = corto_strdup(filename);
    bfile->offset = offset ? corto_strdup(offset) : NULL;
    bfile->timestamp = timestamp;

    if (!fl->files) {
        fl->files = corto_ll_new();
    }
    corto_ll_append(fl->files, bfile);

    if (timestamp) {
        corto_debug("add '%s' with timestamp %d", filename, timestamp);
    } else {
        corto_debug("add '%s' with timestamp 0", filename);
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

    /* Scan for path to start from */
    const char *ptr = pattern, *end = pattern;
    char ch;
    while ((ch = *ptr)) {
        if (ch == '/') {
            if (ptr[1] == '/') {
                break;
            } else {
                end = ptr;
            }
        } else if (corto_idmatch_isOperator(ch)) {
            break;
        }
        ptr ++;
    }

    corto_iter it;
    if (end != pattern) {
        dir = strdup(pattern);
        dir[end - pattern - 1] = '\0';

        if (end[-1] == '/') {
            end --;
        } else {
            end ++;
            if (!*end) {
                end = "*";
            }
        }

        corto_trace("match pattern '%s' in '%s/%s'", end, corto_cwd(), dir);
        if (corto_dir_iter(dir, end, &it)) {
            goto error;
        }
    } else {
        corto_trace("match pattern '%s' in '%s'", pattern, corto_cwd());        
        if (corto_dir_iter(".", pattern, &it)) {
            goto error;
        }
    }

    /* Iterate directory, add matched files */
    int count = 0;
    while (corto_iter_hasNext(&it)) {
        char *file = corto_iter_next(&it);
        char *path = (dir && dir[0]) ? corto_asprintf("%s/%s", dir, file) : strdup(file);
        if (!bake_filelist_add_intern(fl, path, offset, corto_lastmodified(path))) {
            free(path);
            goto error;
        }
        free(path);
        count ++;
    }

    if (!count && !corto_idmatch_hasOperators(end)) {
        char *path = dir ? corto_asprintf("%s/%s", dir, end) : strdup(end);
        if (!bake_filelist_add_intern(fl, path, offset, 0)) {
            free(path);
            goto error;
        }
        free(path);
    }

   if (dir) free(dir); 
    return 0;
error:
    if (dir) free(dir);
    return -1;
}

corto_iter bake_filelist_iter(
    bake_filelist *fl)
{
    return corto_ll_iterAlloc(fl->files);
}

int16_t bake_filelist_set(
    bake_filelist *fl,
    const char *pattern)
{
    corto_assert(fl->pattern == NULL, "setPattern invalid: filelist already set a pattern");
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
    bake_filelist *fl = corto_tls_get(BAKE_FILELIST_KEY);
    return bake_filelist_set(fl, pattern);
}

bake_filelist* bake_filelist_new(
    const char *path,
    const char *pattern)
{
    bake_filelist *result = corto_alloc(sizeof(bake_filelist));
    result->pattern = pattern ? strdup(pattern) : NULL;
    result->files = corto_ll_new();
    result->set = bake_filelist_set_cb;

    /* Extract starting directory from pattern */
    if (pattern) {
        if (bake_filelist_populate(result, NULL, result->pattern)) {
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
    corto_assert(fl != NULL, "passed NULL filelist to filelist_add");
    corto_assert(file != NULL, "passed NULL file to filelist_add");
    if (corto_file_test(file)) {
        return bake_filelist_add_intern(fl, file, NULL, corto_lastmodified(file));
    } else {
        return bake_filelist_add_intern(fl, file, NULL, 0);
    }
}

int16_t bake_filelist_addPattern(
    bake_filelist *fl,
    const char *offset,
    const char *pattern)
{
    corto_assert(fl != NULL, "passed NULL to filelist_addPattern");
    if (!fl) {
        corto_seterr("no filelist specified");
        goto error;
    }

    if (bake_filelist_populate(fl, offset, pattern)) {
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
    corto_assert(fl != NULL, "passed NULL to filelist_addList");
    if (!src) {
        corto_seterr("no source filelist specified");
        goto error;
    }

    corto_iter it = corto_ll_iter(src->files);
    while (corto_iter_hasNext(&it)) {
        corto_ll_append(fl->files, bake_file_copy(corto_iter_next(&it)));
    }

    return 0;
error:
    return -1;
}

uint64_t bake_filelist_count(
    bake_filelist *fl)
{
    return corto_ll_size(fl->files);
}

