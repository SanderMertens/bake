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

/** @file
 * @section Filesystem functions.
 * @brief Functions for doing basic operations on a filesystem.
 */

#ifndef UT_FS_H
#define UT_FS_H

#ifdef _WIN32
#include <sys/stat.h>
#include <errno.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Create a directory.
 * The function creates any directories in the provided name that do not yet
 * exist, as if the Linux command "mkdir -p" is specified. If the directory
 * already exists, the function will not throw an error.
 *
 * @param fmt printf-style format string for building the directory name.
 * @return zero when success, non-zero when failed.
 */
UT_API
int ut_mkdir(
    const char *fmt,
    ...);

/** Read the contents of a directory.
 * Returns a list of strings that represents the files in a directory specified
 * by "name". To clean up the resources, use ut_closedir.
 *
 * @param name The name of the directory to open.
 * @return Linked list that contains the files in the directory.
 */
UT_API
ut_ll ut_opendir(
    const char *name);

/** Release resources from ut_opendir.
 *
 * @param dir Linked list returned by ut_opendir.
 */
UT_API
void ut_closedir(
    ut_ll dir);

/** Returns contents of a directory in iterator.
 * Resources will be automatically cleaned up when the iterator yields no more
 * results. When iteration is prematurely stopped, call ut_iter_release.
 *
 * @param name The name of the directory to open.
 * @param iter_out Iterator to contents in directory.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int16_t ut_dir_iter(
    const char *name,
    const char *filter,
    ut_iter *iter_out);

/** Returns whether directory is empty or not.
 *
 * @param name The name of the directory to check.
 * @return true if empty, false if not.
 */
UT_API
bool ut_dir_isEmpty(
    const char *name);

/** Type holding a stack for usage with ut_dir_push / ut_dir_pop */
typedef void* ut_dirstack;

/** Change working directory, push to stack.
 * This function facilitates recursively walking through directories by being
 * able to keep track of a history of previously visited directories.
 *
 * The working directory will be changed to the specified directory. Each push
 * has to be followed up by a pop, otherwise resources will leak.
 *
 * @param stack A stack object as returned by this function. NULL if this is the first call.
 * @param dir Directory to be pushed to stack.
 * @return Stack object if success, NULL if failed.
 */
UT_API
ut_dirstack ut_dirstack_push(
    ut_dirstack stack,
    const char *dir);

/** Restore working directory, pop from stack.
 * This function facilitates recursively walking through directories by being
 * able to keep track of a history of previously visited directories.
 *
 * The working directory will be changed to previous directory on the stack.
 *
 * @param stack A stack object as returned by ut_dirstack_push.
 * @return 0 if success, non-zero if failed.
 */
UT_API
void ut_dirstack_pop(
    ut_dirstack stack);

/** Obtain working directory relative to bottom of directory stack.
 *
 * @param stack A stack object as returned by ut_dirstack_push.
 * @return Relative path. This value does not need to be freed.
 */
UT_API
const char* ut_dirstack_wd(
    ut_dirstack stack);

/** Creates an empty file.
 *
 * @param name Name of the file.
 * @return zero if success, non-zero if failed
 */
UT_API
int ut_touch(
    const char *name);

/* Get current working directory.
 *
 * @return NULL if failed, an internal buffer set to current working directory otherwise
 */
UT_API
char* ut_cwd(void);

/** Change working directory.
 *
 * @return zero if success, non-zero if failed
 */
UT_API
int ut_chdir(
    const char *name);

/** Copy a file.
 * If the destination file did not yet exist, it will be created.
 *
 * @param source Source file.
 * @param destination Destination file.
 * @return zero if success, non-zero if failed
 */
UT_API
int16_t ut_cp(
    const char *source,
    const char *destination);

/** Create a symbolic link.
 * On operating systems where symbolic links are not supported, this function
 * may revert to doing a copy of the file.
 *
 * @param oldname Name of the file to link to.
 * @param newname Name of the link.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_symlink(
    const char *oldname,
    const char *newname);

/** Set permissions of a file.
 * This function uses the UNIX convention to specify permissions. To specify
 * permissions using the familiar octal notation, use:
 *   ut_setperm("myfile.txt", strtol("777", 0, 8));
 *
 * @param name The filename
 * @param perm Integer indicating the permissions to set.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int16_t ut_setperm(
    const char *name,
    int perm);

/** Get permissions of a file.
 * This function uses the UNIX convention to specify permissions.
 *
 * @param name The filename
 * @param perm_out Integer specifying the permissions of the file.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int16_t ut_getperm(
    const char *name,
    int *perm_out);

/** Test if name is a directory.
 *
 * @param name Name to test.
 * @return true if a directory, false if a regular file.
 */
UT_API
bool ut_isdir(
    const char *name);

/** Rename a file or directory.
 *
 * @param oldName Current filename.
 * @param newName New filename.
 * @return 0 if success, non-zero if failed
 */
UT_API
int ut_rename(
    const char *oldName,
    const char *newName);

/** Remove a file or directory.
 * This function will recursively remove subdirectories.
 *
 * @param name Name of file to remove.
 * @return 0 if success, non-zero if failed
 */
UT_API
int ut_rm(
    const char *name);

/** Recursively remove a directory .
 *
 * @param name Name of directory to remove.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_rmtree(
    const char *name);

/** Get last modified date for file.
 *
 * @param name Name of the file.
 */
UT_API
time_t ut_lastmodified(
    const char *name);

bool ut_dir_hasNext(
    ut_iter *it);

void* ut_dir_next(
    ut_iter *it);

void ut_dir_release(
    ut_iter *it);


#ifdef _WIN32
typedef struct win_dirent_tag {
    HANDLE hFind;
    TCHAR cFileName[MAX_PATH];
} ut_dirent;

/* opendir is POSIX function which is not available on windows platform */
ut_dirent* opendir(
    const char *name);
#endif

#ifdef __cplusplus
}
#endif

#endif
