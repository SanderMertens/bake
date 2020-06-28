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

#include <bake_util.h>

#ifndef _WIN32
#define __mkdir(name) mkdir(name, 0755)
#else
#define __mkdir(name) _mkdir(name)
#endif

int ut_touch(const char *file) {
    FILE* touch = NULL;

    if (file) {
        touch = fopen(file, "ab");
        if (touch) {
            fclose(touch);
        }
    }

    return touch ? 0 : -1;
}

int ut_chdir(const char *dir) {
    if (chdir(dir)) {
        ut_throw("%s '%s'", strerror(errno), dir);
        return -1;
    }
    return 0;
}

char* ut_cwd(void) {
    char cwd[512];
    if (getcwd(cwd, sizeof(cwd))) {
        return ut_setThreadString(cwd);
    } else {
        ut_throw("%s", strerror(errno));
        return NULL;
    }
}

int ut_mkdir(const char *fmt, ...) {
    int local_errno = 0;

    va_list args;
    va_start(args, fmt);
    char *name = ut_venvparse(fmt, args);
    va_end(args);
    if (!name) {
        goto error_name;
    }

    bool exists = ut_file_test(name);

    /* Remove a file if it already exists and is not a directory */
    if (exists && !ut_isdir(name)) {
        if (ut_rm(name)) {
            goto error;
        }
    } else if (exists) {
        free(name);
        /* Directory already exists */
        return 0;
    }

    if (__mkdir(name)) {
        local_errno = errno;

        /* If error is ENOENT an element in the prefix of the name
         * doesn't exist. Recursively create pathname, and retry. */
        if (errno == ENOENT) {
            /* Allocate so as to not run out of stackspace in recursive call */
            char *prefix = ut_strdup(name);
            char *ptr = strrchr(prefix, UT_OS_PS[0]);

            if (ptr && ptr != prefix && ptr[-1] != '.') {
                ptr[0] = '\0';
                if (!ut_mkdir(prefix)) {
                    /* Retry current directory */
                    if (!__mkdir(name)) {
                        local_errno = 0;
                    } else {
                        local_errno = errno;
                    }
                } else {
                    goto error;
                }
            } else {
                free(prefix);
                goto error; /* If no prefix is found, report error */
            }
            free(prefix);
        }

        /* Post condition for function is that directory exists so don't
         * report an error if it already did. */
        if (local_errno && (local_errno != EEXIST)) {
            goto error;
        }
    }

    ut_trace("#[cyan]mkdir %s", name);

    free(name);

    return 0;
error:
    ut_throw("%s: %s", name, strerror(errno));
    free(name);
error_name:
    return -1;
}

static
int ut_cp_file(
    const char *src,
    const char *dst)
{
    FILE *destinationFile = NULL;
    FILE *sourceFile = NULL;
    char *fullDst = (char*)dst;
    int perm = 0;
    bool exists = ut_file_test(dst);

    if (exists && ut_isdir(dst) && !ut_isdir(src)) {
        const char *base = strrchr(src, UT_OS_PS[0]);
        if (!base) base = src; else base = base + 1;
        fullDst = ut_asprintf("%s"UT_OS_PS"%s", dst, base);
        exists = ut_file_test(fullDst);
    }

    if (exists) {
        ut_rm(fullDst);
    }

    if (!(sourceFile = fopen(src, "rb"))) {
        ut_throw("cannot open '%s': %s", src, strerror(errno));
        goto error;
    }

    if (!(destinationFile = fopen(fullDst, "wb"))) {
        if (errno == ENOENT) {
            /* If error is ENOENT, try creating directory */
            char *dir = ut_path_dirname(fullDst);
            int oldlocal_errno = errno;

            if (dir[0] && !ut_mkdir(dir)) {
                /* Retry */
                if (!(destinationFile = fopen(fullDst, "wb"))) {
                    free(dir);
                    goto error_CloseFiles;
                }
            } else {
                ut_throw("cannot open %s: %s", fullDst, strerror(oldlocal_errno));
            }
            free(dir);
        } else {
            ut_throw("cannot open '%s': %s", fullDst, strerror(errno));
            goto error_CloseFiles;
        }
    }

    if (ut_getperm(src, &perm)) {
        ut_throw("cannot get permissions for '%s'", src);
        goto error_CloseFiles;
    }

    if (fseek(sourceFile, 0, SEEK_END)) {
        ut_throw("cannot seek '%s': %s", src, strerror(errno));
        goto error_CloseFiles;
    }

    int fileSize = ftell(sourceFile);
    if (fileSize == -1) {
        ut_throw("cannot get size from '%s': %s", src, strerror(errno));
        goto error_CloseFiles;
    }

    rewind(sourceFile);

    char *buffer = malloc(fileSize);

    int n;
    if ((n = fread(buffer, 1, fileSize, sourceFile)) != fileSize) {
        ut_throw("cannot read '%s': %s", src, strerror(errno));
        goto error_CloseFiles_FreeBuffer;
    }

    if (fwrite(buffer, 1, fileSize, destinationFile) != (size_t)fileSize) {
        ut_throw("cannot write to '%s': %s", fullDst, strerror(errno));
        goto error_CloseFiles_FreeBuffer;
    }

    if (ut_setperm(fullDst, perm)) {
        ut_throw("failed to set permissions of '%s'", fullDst);
        goto error;
    }

    ut_trace("#[cyan]cp %s %s", src, dst);

    if (fullDst != dst) free(fullDst);
    free(buffer);
    fclose(sourceFile);
    fclose(destinationFile);

    return 0;

error_CloseFiles_FreeBuffer:
    free(buffer);
error_CloseFiles:
    if (sourceFile) fclose(sourceFile);
    if (destinationFile) fclose(destinationFile);
error:
    return -1;
}

static
int16_t ut_cp_dir(
    const char *src,
    const char *dst)
{
    if (ut_mkdir(dst)) {
        goto error;
    }

    ut_iter it;

    if (ut_dir_iter(src, NULL, &it)) {
        goto error;
    }

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *src_path = ut_asprintf("%s"UT_OS_PS"%s", src, file);

        if (ut_isdir(src_path)) {
            char *dst_path = ut_asprintf("%s"UT_OS_PS"%s", dst, file);
            if (ut_cp_dir(src_path, dst_path)) {
                goto error;
            }
            free(dst_path);
        } else {
            if (ut_cp_file(src_path, dst)) {
                goto error;
            }
        }
        free(src_path);
    }

    return 0;
error:
    return -1;
}

int16_t ut_cp(
    const char *src,
    const char *dst)
{
    int16_t result;

    char *src_parsed = ut_envparse(src);
    if (!src_parsed) goto error;

    char *dst_parsed = ut_envparse(dst);
    if (!dst_parsed) {
        free(src_parsed);
        goto error;
    }

    if (!ut_file_test(src)) {
        ut_throw("source '%s' does not exist", dst_parsed);
        goto error;
    }

    if (ut_isdir(src)) {
        result = ut_cp_dir(src_parsed, dst_parsed);
        ut_trace("#[cyan]cp %s %s", src, dst);
    } else {
        result = ut_cp_file(src_parsed, dst_parsed);
    }

    free(src_parsed);
    free(dst_parsed);

    return result;
error:
    ut_throw("failed to copy '%s' to '%s'", src, dst);
    return -1;
}

void ut_closedir(ut_ll dir) {
    ut_iter iter = ut_ll_iter(dir);

    while(ut_iter_hasNext(&iter)) {
        free(ut_iter_next(&iter));
    }
    ut_ll_free(dir);
}

struct ut_dir_filteredIter {
    ut_expr_program program;
    void *files;
};

static
void ut_dir_releaseRecursiveFilter(
    ut_iter *it)
{
    /* Free all elements */
    ut_iter _it = ut_ll_iter(it->data);
    while (ut_iter_hasNext(&_it)) {
        free(ut_iter_next(&_it));
    }

    /* Free list iterator context */
    ut_ll_iterRelease(it);
}

static
int16_t ut_dir_collect(
    const char *name,
    ut_dirstack stack,
    ut_expr_program filter,
    const char *offset,
    ut_ll files,
    bool recursive)
{
    ut_iter it;

    /* Move to current directory */
    if (name && name[0]) {
        if (!(stack = ut_dirstack_push(stack, name))) {
            goto stack_error;
        }
    }

    /* Obtain iterator to current directory */
    if (ut_dir_iter(ut_ll_last(stack), NULL, &it)) {
        goto error;
    }

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);

        /* Add file to results if it matches filter */
        char *path;
        if (offset) {
            path = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS"%s", ut_dirstack_wd(stack), offset, file);
        } else {
            path = ut_asprintf("%s"UT_OS_PS"%s", ut_dirstack_wd(stack), file);
        }
        ut_path_clean(path, path);

        if (ut_expr_run(filter, file)) {
            ut_ll_append(files, path);
        }

        if (!recursive) {
            continue;
        }

        /* If directory, crawl recursively */
        char *fullpath = ut_asprintf("%s"UT_OS_PS"%s", ut_ll_last(stack), file);
        if (ut_isdir(fullpath)) {
            if (ut_dir_collect(file, stack, filter, offset, files, true)) {
                free(fullpath);
                goto error;
            }
        }
        free(fullpath);
    }

    if (name && name[0]) {
        ut_dirstack_pop(stack);
    }

    return 0;
error:
    ut_dirstack_pop(stack);
stack_error:
    return -1;
}

int16_t ut_dir_iter(
    const char *name,
    const char *filter,
    ut_iter *it_out)
{
    char *path = (char*)name;
    char *offset = NULL;

    if (!path) {
        ut_throw("invalid 'null' provided as directory name");
        goto error;
    }

    if (filter) {
        const char *ptr, *last_elem = filter;
        char ch;

        for (ptr = filter; (ch = *ptr); ptr ++) {
            if (ut_expr_isOperator(ch)) {
                break;
            } else if (ch == '/') {
                last_elem = ptr;
                if (ptr[1] == '/') {
                    break;
                }
            }
        }

        if (!ch) {
            /* Filter only has path, no wildcards or recursive operators */
            if (last_elem == filter) {
                /* If there are no elements, filter matches single file */
            } else {
                /* If there are elements, append filter to path */
                path = ut_asprintf("%s"UT_OS_PS"%s", path, filter);
                ut_path_clean(path, path);
                filter = NULL;
            }
        } else {
            if (last_elem == filter) {
                /* Whole filter needs to be evaluated, no optimization */
            } else {
                /* Part of filter is path, strip it off and add to path */
                uint32_t old_len = strlen(path);
                path = ut_asprintf("%s"UT_OS_PS"%s", path, filter);
                path[strlen(path) - strlen(last_elem)] = '\0';
                offset = &path[old_len] + 1;
                filter = last_elem;
            }
        }
    }

    if (ut_file_test(path) != 1) {
        if (ut_file_test(name) == 1) {
            *it_out = UT_ITER_EMPTY;
            return 0;
        } else {
            ut_throw("directory '%s' does not exist", name);
            goto error;
        }
    } else if (!filter) {
        if (!ut_isdir(path)) {
            ut_ll result = ut_ll_new();
            char *path_offset = path;

            if (name && strcmp(name, ".")) {
                size_t len = strlen(name);
                path_offset += len;
                if (path_offset[0] == UT_OS_PS[0]) {
                    path_offset ++;
                }
            }

            ut_ll_append(result, path_offset);
            *it_out = ut_ll_iterAlloc(result);
            return 0;
        }
    }

    if (!filter && !offset) {
        ut_iter result = {
            .ctx = opendir(path),
            .data = NULL,
            .hasNext = ut_dir_hasNext,
            .next = ut_dir_next,
            .release = ut_dir_release
        };

        if (!result.ctx) {
            ut_throw("%s: %s", path, strerror(errno));
            goto error;
        }

        *it_out = result;
    } else {
        ut_expr_program program = ut_expr_compile(filter, TRUE, TRUE);
        ut_iter result = UT_ITER_EMPTY;
        ut_ll files = ut_ll_new();

        if (ut_expr_scope(program) == 2) {
            if (ut_dir_collect(path, NULL, program, offset, files, true)) {
                ut_throw("recursive dir_iter failed");
                goto error;
            }
        } else {
            if (ut_dir_collect(path, NULL, program, offset, files, false)) {
                ut_throw("dir_iter failed");
                goto error;
            }
        }

        result = ut_ll_iterAlloc(files);
        result.data = files;
        result.release = ut_dir_releaseRecursiveFilter;

        *it_out = result;
    }

    if (path != name) free(path);

    return 0;
error:
    return -1;
}

bool ut_dir_isEmpty(
    const char *name)
{
    ut_iter it;
    if (ut_dir_iter(name, NULL, &it)) {
        return true; /* If dir can't be opened, it might as well be empty */
    }

    bool isEmpty = !ut_iter_hasNext(&it);
    ut_iter_release(&it); /* clean up resources */
    return isEmpty;
}

ut_dirstack ut_dirstack_push(
    ut_dirstack stack,
    const char *dir)
{
    if (!stack) {
        stack = ut_ll_new();
        ut_ll_append(stack, ut_strdup(dir));
    } else {
        ut_ll_append(stack, ut_asprintf("%s"UT_OS_PS"%s", ut_ll_last(stack), dir));
    }

    return stack;
}

void ut_dirstack_pop(
    ut_dirstack stack)
{
    free( ut_ll_takeLast(stack));
}

const char* ut_dirstack_wd(
    ut_dirstack stack)
{
    char *first = ut_ll_get(stack, 0);
    char *last = ut_ll_last(stack);

    size_t first_len = strlen(first);
    size_t last_len = strlen(last);

    if (first_len == last_len) {
        return ".";
    } else {
        last += first_len;
        if (last[0] == UT_OS_PS[0]) {
            last ++;
        }

        return last;
    }
}

time_t ut_lastmodified(
    const char *name)
{
    struct stat attr;

    if (stat(name, &attr) < 0) {
        ut_throw("failed to stat '%s' (%s)", name, strerror(errno));
        goto error;
    }

    return attr.st_mtime;
error:
    return -1;
}
