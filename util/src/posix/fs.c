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

#include "../../include/util.h"

static
bool ut_checklink(
    const char *link,
    const char *file)
{
    char buf[512];
    char *ptr = buf;
    int length = strlen(file);
    if (length >= 512) {
        ptr = malloc(length + 1);
    }
    int res;
    if (((res = readlink(link, ptr, length)) < 0)) {
        goto nomatch;
    }
    if (res != length) {
        goto nomatch;
    }
    if (strncmp(file, ptr, length)) {
        goto nomatch;
    }
    if (ptr != buf) free(ptr);
    return true;
nomatch:
    if (ptr != buf) free(ptr);
    return false;
}

int ut_symlink(
    const char *oldname,
    const char *newname)
{
    char *fullname = NULL;
    if (ut_path_is_relative(oldname)) {
        fullname = ut_asprintf("%s"UT_OS_PS"%s", ut_cwd(), oldname);
        ut_path_clean(fullname, fullname);
    }
    else {
        /* Safe- the variable will not be modified if it's equal to newname */
        fullname = (char*)oldname;
    }

    ut_trace("#[cyan]symlink %s %s", newname, fullname);

    if (symlink(fullname, newname)) {

        if (errno == ENOENT) {
            /* If error is ENOENT, try creating directory */
            char *dir = ut_path_dirname(newname);
            int old_errno = errno;

            if (dir[0] && !ut_mkdir(dir)) {
                /* Retry */
                if (ut_symlink(fullname, newname)) {
                    goto error;
                }
            }
            else {
                ut_throw("%s: %s", newname, strerror(old_errno));
            }
            free(dir);

        }
        else if (errno == EEXIST) {
            if (!ut_checklink(newname, fullname)) {
                /* If a file with specified name already exists, remove existing file */
                if (ut_rm(newname)) {
                    goto error;
                }

                /* Retry */
                if (ut_symlink(fullname, newname)) {
                    goto error;
                }
            }
            else {
                /* Existing file is a link that points to the same location */
            }
        }
    }
    if (fullname != oldname) free(fullname);
    return 0;
error:
    if (fullname != oldname) free(fullname);
    ut_throw("symlink failed");
    return -1;
}

int16_t ut_setperm(
    const char *name,
    int perm)
{
    ut_trace("setperm %s %d", name, perm);
    if (chmod(name, perm)) {
        ut_throw("chmod: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int16_t ut_getperm(
    const char *name,
    int *perm_out)
{
    struct stat st;

    if (stat(name, &st) < 0) {
        ut_throw("getperm: %s", strerror(errno));
        return -1;
    }

    *perm_out = st.st_mode;

    return 0;
}

bool ut_isdir(const char *path) {
    struct stat buff;
    if (stat(path, &buff) < 0) {
        return 0;
    }
    return S_ISDIR(buff.st_mode) ? true : false;
}

int ut_rename(const char *oldName, const char *newName) {

    if (rename(oldName, newName)) {
        ut_throw("failed to move %s %s: %s",
            oldName, newName, strerror(errno));
        goto error;
    }
    return 0;
error:
    return -1;
}

/* Remove a file. Returns 0 if OK, -1 if failed */
int ut_rm(const char *name) {
    int result = 0;

    /* First try to remove file. The 'remove' function may fail if 'name' is a
    * directory that is not empty, however it may also be a link to a directory
    * in which case ut_isdir would also return true.
    *
    * For that reason, the function should not always perform a recursive delete
    * if a directory is encountered, because in case of a link, only the link
    * should be removed, not the contents of its target directory.
    *
    * Trying to remove the file first is a solution to this problem that works
    * on any platform, even the ones that do not support links (as opposed to
    * checking if the file is a link first).
    */
    if (remove(name)) {
        if (errno != ENOENT) {
            if (ut_isdir(name)) {
                ut_trace("#[cyan]rm %s (D)", name);
                return ut_rmtree(name);
            }
            else {
                result = -1;
                ut_throw(strerror(errno));
            }
        }
        else {
            /* Don't care if file doesn't exist */
        }
    }

    if (!result) {
        ut_trace("#[cyan]rm %s", name);
    }

    return result;
}

static 
int ut_rmtreeCallback(
  const char *path,
  const struct stat *sb,
  int typeflag,
  struct FTW *ftwbuf)
{
    UT_UNUSED(sb);
    UT_UNUSED(typeflag);
    UT_UNUSED(ftwbuf);
    if (remove(path)) {
        ut_throw(strerror(errno));
        goto error;
    }
    return 0;
error:
    return -1;
}

/* Recursively remove a directory */
int ut_rmtree(const char *name) {
    return nftw(name, ut_rmtreeCallback, 20, FTW_DEPTH | FTW_PHYS);
}

/* Read the contents of a directory */
ut_ll ut_opendir(const char *name) {
    DIR *dp;
    struct dirent *ep;
    ut_ll result = NULL;

    dp = opendir(name);

    if (dp != NULL) {
        result = ut_ll_new();
        while ((ep = readdir(dp))) {
            if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, "..")) {
                ut_ll_append(result, ut_strdup(ep->d_name));
            }
        }
        closedir(dp);
    }
    else {
        ut_throw("%s: %s", name, strerror(errno));
    }

    return result;
}

bool ut_dir_hasNext(
    ut_iter *it)
{
    struct dirent *ep = readdir(it->ctx);
    while (ep && (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))) {
        ep = readdir(it->ctx);
    }

    if (ep) {
        it->data = ep->d_name;
    }

    return ep ? true : false;
}

void* ut_dir_next(
    ut_iter *it)
{
    return it->data;
}

void ut_dir_release(
    ut_iter *it)
{
    closedir(it->ctx);
}


