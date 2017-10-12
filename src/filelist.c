
#include "bake.h"

extern corto_tls BAKE_FILELIST_KEY;

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
int16_t bake_filelist_add(
    bake_filelist *fl,
    const char *filename,
    time_t timestamp)
{
    bake_file *bfile = corto_alloc(sizeof(bake_file));
    bfile->name = strdup(filename);
    bfile->timestamp = timestamp;

    if (!fl->files) {
        fl->files = corto_ll_new();
    }
    corto_ll_append(fl->files, bfile);
}

static
int16_t bake_filelist_populate(
    bake_filelist *fl)
{
    char *cwd = strdup(corto_cwd());

    if (fl->path) {
        if (corto_chdir(fl->path)) {
            goto error;
        }
    }

    /* Scan for path to start from */
    const char *pattern = fl->pattern;
    const char *ptr = pattern, *end = pattern;
    char ch;
    while ((ch = *ptr)) {
        if (ch == '/') {
            end = ptr;
        } else if (corto_idmatch_isOperator(ch)) {
            break;
        }
        ptr ++;
    }

    corto_iter it;
    if (end != pattern) {
        /* Change directory */
        corto_id dir;
        strcpy(dir, pattern);
        dir[end - pattern - 1] = '\0';

        if (corto_chdir(dir)) {
            corto_seterr("directory '%s' does not exist", dir);
            goto error;
        }

        if (end[-1] == '/') {
            end --;
        } else {
            end ++;
            if (!*end) {
                end = "*";
            }
        }

        printf("end = %s\n", end);

        if (corto_dir_iter(".", end, &it)) {
            goto error;
        }
    } else {
        if (corto_dir_iter(".", pattern, &it)) {
            goto error;
        }
    }

    /* Iterate directory, add matched files */
    int count = 0;
    while (corto_iter_hasNext(&it)) {
        char *file = corto_iter_next(&it);
        bake_filelist_add(fl, file, corto_lastmodified(file));
        count ++;
    }

    if (!count && !corto_idmatch_hasOperators(end)) {
        corto_trace(
            "pattern '%s' matched no files, adding '%s' with timestamp 0", 
            end, strarg("%s/%s", fl->path, end));
        bake_filelist_add(fl, strarg("%s/%s", fl->path, end), 0);
    }

    if (cwd) {
        if (corto_chdir(cwd)) {
            corto_seterr("failed to restore current directory to '%s'", cwd);
            goto error;
        }
    }

   if (cwd) free(cwd); 
    return 0;
error:
    if (cwd) free(cwd);
    return -1;
}

int16_t bake_filelist_set(
    bake_filelist *fl,
    const char *pattern)
{
    corto_assert(fl->pattern == NULL, "setPattern invalid: filelist already set a pattern");
    fl->pattern = strdup(pattern);
    if (bake_filelist_populate(fl)) {
        free(fl->pattern);
        fl->pattern = NULL;
        return -1;
    } else {
        return 0;
    }
}


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
    result->path = path ? strdup(path) : strdup(corto_cwd());
    result->pattern = pattern ? strdup(pattern) : NULL;
    result->files = corto_ll_new();
    result->set = bake_filelist_set_cb;

    /* Extract starting directory from pattern */
    if (pattern) {
        if (bake_filelist_populate(result)) {
            goto error;
        }
    }

    return result;
error:
    if (result) bake_filelist_free(result);
    return NULL;
}

uint64_t bake_filelist_count(
    bake_filelist *fl)
{
    return corto_ll_size(fl->files);
}
