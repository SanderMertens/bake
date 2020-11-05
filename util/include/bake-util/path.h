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
 * @section path Path utility functions
 * @brief API that makes it easier to work with paths.
 */


#ifndef UT_PATH_H_
#define UT_PATH_H_

#define UT_MAX_PATH_LENGTH (512)

#ifdef __cplusplus
extern "C" {
#endif

/** Create a canonical version of a path.
 * This function reduces a path to its canonical form by resolving . and .. operators.
 * @param str An id buffer in which to store the id. If NULL, a corto-managed
 * string is returned which may change with subsequent calls to ut_fullpath and
 * other functions that use the corto stringcache.
 *
 * @param path The input path. Can be the same as buffer.
 * @return The path. If buffer is NULL, the returned string is not owned by the application. Otherwise, buffer is returned.
 * @see ut_idof ut_fullname ut_path ut_pathname
 */
UT_API
char* ut_path_clean(
    char *buf,
    char *path);

/** Get directory name from path.
 *
 * @param path The input path.
 * @return The directory name from the specified path. Needs to be deallocated.
 */
UT_API
char* ut_path_dirname(
    const char *path);


/** Turn package id into path in place */
UT_API
void ut_package_to_path(
    char *package);

/** Obtain an array with the individual elements of a path.
 * This function splits up a path using the specified separator. An element with
 * at least *UT_MAX_SCOPE_DEPTH* elements must be provided.
 *
 * ```warning
 * This function modifies the input path.
 * ```
 *
 * @param path The path to be split up.
 * @param elements The result array with the path elements.
 * @param sep The separator used to split up the path.
 * @return The number of elements in the array.
 * @see ut_idof ut_fullname ut_path ut_pathname
 */
UT_API
int32_t ut_pathToArray(
    char *path,
    const char *elements[],
    char *sep);

/* Walk tokens in a path */
UT_API
char *ut_path_tok(
    char **id_ptr_ptr,
    char **parent_id,
    char *full_id);

/* Construct path from parent and id */
UT_API
void ut_path_combine(
    char *full_id,
    const char *parent,
    const char *id);

/* Function returns the 'to' path relative to 'from', and ensures that the
 * output can be used in an "from + / + out + / + id" expression to reconstruct
 * a correct full path to an object. */

/* Compute relative path between two path strings.
 *
 * This function is the equivalent of ut_path, but instead of operating on
 * objects, it takes strings as input.
 *
 * ut_path_offset will compute a path that, when combined with the 'from' path
 * will result in the 'to' path. For example, when 'from' is '/data' and 'to' is
 * '/data/foo', 'out' will be set to 'foo'.
 *
 * When both 'from' and 'to' are equal, 'out' will be set to `.`, as appending a '.'
 * to the from parameter would be semantically equal to 'from' (and thus 'to').
 *
 * When 'from' is not a parent of 'to', '..' will be added to the string to move up
 * in the hierarchy until the first common ancestor is reached. This behavior is
 * disabled when false is passed to 'travel'. In that case, when no common
 * ancestor is found, 'to' is copied to 'out'.
 *
 * When 'from' is set to '/', the resulting path will be relative to root and
 * will not contain an initial '/'. If 'from' is set to an empty string, 'out'
 * will contain an initial '/'.
 *
 * When 'from' is set to NULL, the function will behave as if an empty string is
 * provided.
 *
 * The function is O(n+m), where n is the length of from and m is the length of
 * to.
 */
UT_API
void ut_path_offset(
    char *out,
    const char *from,
    const char *to,
    int tolen,
    bool travel);

/* Test if provided path is relative */
UT_API
bool ut_path_is_relative(
    const char *path);

#ifdef __cplusplus
}
#endif

#endif
