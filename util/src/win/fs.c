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

int ut_symlink(
    const char *oldname,
    const char *newname)
{
    /* Windows requires elevated privileges to allow for the creation of
     * symlinks (even in USERPROFILE), which would require users to either
     * run bake in an elevated prompt, or lower security settings of the
     * Windows installation.
     * That's too much of a hassle, so instead bake will just revert to
     * copying files on Windows. The drawback is that now to get anything
     * updated in the bake environment, a user will have to bake the 
     * project first. */
    return ut_cp(oldname, newname);
}

int16_t ut_setperm(
    const char *name,
    int perm)
{
    ut_trace("#[cyan]setperm %s %d", name, perm);
    if (_chmod(name, perm)) {
        ut_throw("chmod: %s", ut_last_win_error());
        return -1;
    }
    return 0;
}

int16_t ut_getperm(
    const char *name,
    int *perm_out)
{
    struct _stat st;

    if (_stat(name, &st) < 0) {
        ut_throw("getperm: %s", ut_last_win_error());
        return -1;
    }

    *perm_out = st.st_mode;

    return 0;
}

bool ut_isdir(const char *path) {
    return PathIsDirectoryA(path);
}

int ut_rename(const char *oldName, const char *newName) {

    ut_trace("#[cyan]rename %s %s", oldName, newName);

    if (!_access_s(newName, 0))
        ut_rm(newName);

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
    if (PathFileExistsA(name)) {
        if (!DeleteFile(name)) {
            if (ut_isdir(name)) {
                ut_debug("deleting '%s' as normal file failed: %s", name, ut_last_win_error());
                ut_debug("trying to delete '%s' as directory", name);
                ut_trace("#[cyan]rm %s (D)", name);
                return ut_rmtree(name);
            } else {
                goto error;
            }
        }
    } else {
        /* Don't care if file doesn't exist */
    }

    ut_trace("#[cyan]rm %s", name);
    
    return 0;
error:
    ut_throw( ut_last_win_error());
    return -1;
}

/* Recursively remove a directory */
int ut_rmtree(const char *name) {
    char *fullname;
    if (strlen(name) > 1 && name[0] == '.' && name[1] == UT_OS_PS[0]) {
        fullname = ut_asprintf("%s"UT_OS_PS"%s\0", ut_cwd(), name+2);
    } else {
        fullname = ut_asprintf("%s\0", name);
    }

    SHFILEOPSTRUCT fileOp = { 0 };
    ZeroMemory(&fileOp, sizeof(SHFILEOPSTRUCT));
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = ut_asprintf("%s\0", name);
    fileOp.pTo = "\0";
    fileOp.fFlags = FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR |
        FOF_MULTIDESTFILES | FOF_SILENT;
    fileOp.lpszProgressTitle = "";
    int result = SHFileOperation(&fileOp);
    
    if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) {
        return 0;
    }
    
    if (result) {
        ut_throw("failed to delete directory %s", fullname);
    }

    free(fullname);

    return result;
}

/* Read the contents of a directory */
ut_ll ut_opendir(const char *name) {

    WIN32_FIND_DATA ffd;
    TCHAR szDir[MAX_PATH];
    HANDLE hFind = INVALID_HANDLE_VALUE;
    ut_ll result = NULL;

    strcpy_s(szDir, MAX_PATH, name);
    strcat_s(szDir, MAX_PATH, TEXT("\\*"));
    if( PathFileExistsA(name) )
        result = ut_ll_new();
    else
        ut_throw("%s: %s", name, ut_last_win_error());

    hFind = FindFirstFile(szDir, &ffd);
    if(INVALID_HANDLE_VALUE != hFind)
    {
        do
        {
            ut_ll_append(result, strdup(ffd.cFileName));
        } while (FindNextFile(hFind, &ffd));
        FindClose(hFind);
    }
    else
        ut_throw("%s: %s", name, ut_last_win_error());
    return result;
}

/* opendir is POSIX function which is not available on windows platform */
ut_dirent* opendir(const char *name)
{
    WIN32_FIND_DATA ffd;
    TCHAR szDir[MAX_PATH];
    HANDLE hFind = INVALID_HANDLE_VALUE;

    strcpy_s(szDir, MAX_PATH, name);
    strcat_s(szDir, MAX_PATH, TEXT("\\*"));
    ut_dirent* ep = NULL;
    if (PathFileExistsA(name))
        ep = (ut_dirent*)malloc(sizeof(ut_dirent));
    else
        ut_throw("%s: %s", name, ut_last_win_error());

    hFind = FindFirstFile(szDir, &ffd);
    if (INVALID_HANDLE_VALUE != hFind)
    {
        ep->hFind = hFind;
        sprintf(ep->cFileName, "%s", ffd.cFileName);
    }
    else
        ut_throw("%s: %s", name, ut_last_win_error());
    return ep;
}

bool ut_dir_hasNext(
    ut_iter *it)
{
    ut_dirent* ep = (ut_dirent*)it->ctx;
    if (!ep)
        return false;

    WIN32_FIND_DATA ffd;
    if (ep->hFind != INVALID_HANDLE_VALUE)
    {
        while (FindNextFile(ep->hFind, &ffd))
        {
            if (*ffd.cFileName == '.')
                continue;
            it->data = ut_strdup(ffd.cFileName);
            return true;
        }
    }
    return false;
}

void* ut_dir_next(
    ut_iter *it)
{
    return it->data;
}

void ut_dir_release(
    ut_iter *it)
{
    ut_dirent* ep = (ut_dirent*)it->ctx;
    FindClose(ep->hFind);
}

