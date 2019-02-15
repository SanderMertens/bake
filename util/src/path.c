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

#include "../include/util.h"

static
char *__strsepr(char **str, char delim) {
    char *result = *str;
    if (result) {
        char *ptr = strchr(result, delim);
        if (ptr) {
            *ptr = 0;
            (*str) = ptr + 1;
        } else {
            *str = NULL;
        }
    }
    return result;
}

char* ut_path_clean(char *buf, char *path) {
    char work[UT_MAX_PATH_LENGTH], tempbuf[UT_MAX_PATH_LENGTH];
    char *cp, *thisp, *nextp = work;
    bool threadStr = false;
    bool equalBuf = true;

    if (!buf) {
        buf = malloc(UT_MAX_PATH_LENGTH);
        threadStr = true;
    } else if (buf == path) {
        equalBuf = true;
        buf = tempbuf;
    }

    cp = strchr(path, UT_OS_PS[0]);

    /* no '/' characters - return as-is */
    if (cp == NULL) {
        return path;
    }

    /* copy leading slash if present */
    if (cp == path) {
        strcpy(buf, UT_OS_PS);
    } else {
        buf[0] = '\0';
    }

    /* tokenization */
    strcpy(work, path);
    while ((thisp = __strsepr(&nextp, UT_OS_PS[0])) != NULL) {
        if (*thisp == '\0') continue;

        if (strcmp(thisp, ".") == 0) continue;

        if (strcmp(thisp, "..") == 0) {
            cp = strrchr(buf, UT_OS_PS[0]);

             /* "/" or "/foo" */
            if (cp == buf) {
                buf[1] = '\0';
                continue;
            }

            /* "..", "foo", or "" */
            else if (cp == NULL) {
                if (buf[0] != '\0' && strcmp(buf, "..") != 0) {
                    buf[0] = '\0';
                    continue;
                }
            }
            /* ".../foo" */
            else {
                *cp = '\0';
                continue;
            }
        }

        if (buf[0] != '\0' && buf[strlen(buf) - 1] != UT_OS_PS[0]) {
            strcat(buf, UT_OS_PS);
        }

        strcat(buf, thisp);
    }

    if (buf[0] == '\0') strcpy(buf, ".");

    if (threadStr) {
        char *tmp = buf;
        path = ut_setThreadString(buf);
        free(tmp);
    } else if (equalBuf) {
        strcpy(path, buf);
    } else {
        path = buf;
    }

    return path;
}

char* ut_path_dirname(
    const char *path)
{
    char *result = strdup(path);

    char *ptr = strrchr(result, UT_OS_PS[0]);
    if (ptr) {
        ptr[0] = '\0';
    } else {
        result = strdup("");
    }

    return result;
}

char* ut_path_tok(
    char **id_ptr_ptr,
    char **parent_id,
    char *full_id)
{
    char *id_ptr = *id_ptr_ptr;
    if (!id_ptr) {
        id_ptr = strrchr(full_id, UT_OS_PS[0]);
        if (!id_ptr) {
            id_ptr = full_id;
        }
    } else {
        char *next_ptr = strrchr(full_id, UT_OS_PS[0]);
        if (next_ptr) {
            id_ptr[-1] = UT_OS_PS[0];
            id_ptr = next_ptr;
        } else {
            id_ptr = full_id;
        }
    }

    if (id_ptr == full_id) {
        if (full_id[0] == UT_OS_PS[0]) {
            *id_ptr_ptr = full_id + 1;
        }
        *parent_id = UT_OS_PS;
        return NULL;
    } else {
        id_ptr[0] = '\0';
        *parent_id = full_id;
        *id_ptr_ptr = id_ptr + 1;
        return id_ptr;
    }
}

void ut_path_combine(
    char *full_id,
    const char *parent,
    const char *id)
{
    if (id[0] == UT_OS_PS[0]) {
        strcpy(full_id, id);
    } else {
        if (parent[0] == UT_OS_PS[0] && !parent[1]) {
            full_id[0] = UT_OS_PS[0];
            strcpy(full_id + 1, id);
        } else if (parent[0] == '.' && !parent[1]) {
            strcpy(full_id, id);
        } else {
            int len = strlen(parent);
            strcpy(full_id, parent);
            full_id[len] = UT_OS_PS[0];
            strcpy(&full_id[len + 1], id);
        }
    }
}

static
char tochar(
    const char *to,
    const char *ptr,
    int len)
{
    if (len > 0) {
        if (ptr - to < len) {
            return ptr[0];
        } else {
            return '\0';
        }
    } else {
        return ptr[0];
    }
}

#define tch tochar(to, tptr, tolen)

static
bool chicmp(
    char ch1,
    char ch2)
{
    if (ch1 < 97) ch1 = tolower(ch1);
    if (ch2 < 97) ch2 = tolower(ch2);
    return ch1 == ch2;
}

void ut_path_offset(
    char *out,
    const char *from,
    const char *to,
    int tolen,
    bool travel)
{
    char *outptr = out, *firstid = out;

    /* If 'from' is empty, and 'to' contains a path to an object other than
     * root, construct a 'from' that includes the root, so that a user can do
     * out + '/' + id to create the full path. If 'to' is pointing to root, do
     * not append root, as out + '/' + id would then result in a double '/' */
    if (!from || !from[0]) {
        bool to_isRoot = to && (to[0] == UT_OS_PS[0]) && !to[1];
        if (!to_isRoot) {
            (outptr ++)[0] = UT_OS_PS[0];
            firstid++;
        } else {
            /* If to is root, and from is empty return an empty string */
            if (travel) {
                (outptr++)[0] = '\0';
            } else {
                (outptr++)[0] = UT_OS_PS[0];
                outptr[0] = '\0';
            }
            return;
        }
    }

    const char *fptr = from ? from[0] == UT_OS_PS[0] ? &from[1] : from : "";
    const char *tptr = to ? to[0] == UT_OS_PS[0] ? &to[1] : to : "";

    /* First, move ptrs until paths diverge */
    while (fptr[0] && chicmp(fptr[0], tch)) {
        fptr ++;
        tptr ++;
    }

    /* Check if paths are equal */
    if (!fptr[0] && !tch) {
        (outptr++)[0] = '.';
    } else if (fptr[0]) {
        /* If fptr was split on a '/', don't insert a '..' for that */
        if (fptr[0] == UT_OS_PS[0]) fptr ++;

        if (travel) {
            /* Next, for every identifier in 'from', add '..' */
            while (fptr[0]) {
                if (fptr[0] == UT_OS_PS[0]) {
                    if (outptr != firstid) (outptr++)[0] = UT_OS_PS[0];
                    (outptr++)[0] = '.';
                    (outptr++)[0] = '.';
                }
                fptr ++;
            }

            /* Add '..' for last identifier */
            if (outptr != firstid) (outptr++)[0] = UT_OS_PS[0];
            (outptr++)[0] = '.';
            (outptr++)[0] = '.';
        } else {
            /* Reset to-pointer to start, so it is entirely copied */
            tptr = to;
            firstid = NULL;
        }
    }

    /* Finally, append remaining elements from to */
    if (tch) {
        if (tch == UT_OS_PS[0]) tptr ++;
        if (outptr != firstid) (outptr++)[0] = UT_OS_PS[0];
        while (tch) {
            (outptr++)[0] = tch;
            tptr ++;
        }
    }

    outptr[0] = '\0';
}

void ut_package_to_path(
    char *package)
{
    char *ptr, ch;
    for (ptr = package; (ch = *ptr); ptr ++) {
        if (ch == '.') {
            *ptr = UT_OS_PS[0];
        }
    }
}

bool ut_path_is_relative(
    const char *path)
{
#ifdef _WIN32
    if (path[0] && path[1] == ':') {
        return false;
    } else {
        return true;
    }
#else
    if (path[0] == UT_OS_PS[0]) {
        return false;
    } else {
        return true;
    }
#endif
}
