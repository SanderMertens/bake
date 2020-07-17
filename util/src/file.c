
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

static
void printError(
    int e,
    const char *ctx)
{
    ut_throw("%s '%s'", strerror(e), ctx);
}

FILE* ut_file_open(
    const char* filename,
    const char* mode)
{
    FILE *result = fopen(filename, mode);

    if (!result && (strchr(mode, 'a') || strchr(mode, 'w'))) {
        if (errno == ENOENT) {
            char *dir = ut_path_dirname(filename);
            if (ut_mkdir(dir)) {
                free(dir);
                goto error;
            }
            free(dir);

            /* Retry */
            if (!(result = fopen(filename, mode))) {
                printError(errno, filename);
                goto error;
            }
        }
    }

    return result;
error:
    return NULL;
}

char* ut_file_load(
    const char* filename)
{
    FILE* file;
    char* content;
    int size;

    /* Open file for reading */
    file = fopen(filename, "r");
    if (!file) {
        ut_throw("%s (%s)", strerror(errno), filename);
        goto error;
    }

    /* Determine file size */
    fseek (file, 0 , SEEK_END);
    size = ftell (file);
    if (size == -1) {
        goto error;
    }
    rewind(file);

    /* Load contents in memory */
    content = malloc (size + 1);
    if (!(size = fread(content, 1, size, file))) {
        ut_throw("ut_file_load('%s') read zero bytes instead of %d", filename, size);
        free(content);
    } else {
        content[size] = '\0';
    }

    fclose(file);

    return content;
error:
    return NULL;
}

int16_t ut_file_test(
    const char* filefmt,
    ...)
{
    bool exists = false;
    va_list arglist;

    va_start(arglist, filefmt);
    char *file = ut_venvparse(filefmt, arglist);
    va_end(arglist);

    if (file) {
#ifndef _WIN32
        errno = 0;
        FILE *f = fopen(file, "rb");
        if (f) {
            exists = true;
            fclose(f);
        } else if ((errno != ENOENT) && (errno != ENOTDIR)) {
            ut_throw("%s: %s", file, strerror(errno));
            return -1;
        } else {
            errno = 0;
        }
#else
        DWORD dwAttrib = GetFileAttributes(file);
        exists = (dwAttrib != INVALID_FILE_ATTRIBUTES);
#endif
    }

    free(file);

    return (exists != 0) ? true : false;
}

/* Get file size */
unsigned int ut_file_size(
    FILE* file)
{
    unsigned int size;

    /* Determine file size */
    fseek (file, 0 , SEEK_END);
    size = ftell (file);
    rewind(file);

    return size;
}

/* Read line from file */
char* ut_file_readln(
    FILE* file,
    char* buf,
    unsigned int length)
{
    int c;
    char* ptr;

    if (!file) {
        return 0;
    }

    c = EOF;
    ptr = buf;

    while (!feof(file)) {

        c = fgetc(file);
        
        if (c == EOF) {
            break;
        }

        if ((unsigned int)(ptr - buf) < (length - 1)) {
            *ptr = c;
            ptr++;
        }

        if (c == '\n') {
            break;
        }
    }

    *ptr = '\0';

    if (ptr == buf) {
        return NULL;
    } else {
        return buf;
    }
}

static
bool ut_file_hasNext(
    ut_iter *it)
{
    return !feof(it->ctx);
}

static
void* ut_file_next(
    ut_iter *it)
{
    ut_strbuf buf = UT_STRBUF_INIT;

    while (!feof(it->ctx)) {
        char c = fgetc(it->ctx);
        if (c == '\n' || c == (char)EOF) {
            break;
        }

        ut_strbuf_appendstrn(&buf, &c, 1);
    }

    if (it->data) free(it->data);
    it->data = ut_strbuf_get(&buf);
    return it->data;
}

static
void ut_file_release(
    ut_iter *it)
{
    if (it->data) free(it->data);
    if (it->ctx) fclose(it->ctx);
}

int16_t ut_file_iter(
    char *filename,
    ut_iter *iter_out)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        printError(errno, filename);
        goto error;
    }

    iter_out->ctx = f;
    iter_out->data = NULL;
    iter_out->hasNext = ut_file_hasNext;
    iter_out->next = ut_file_next;
    iter_out->release = ut_file_release;

    return 0;
error:
    return -1;
}

/* Get file extension */
char* ut_file_extension(
    const char* file,
    char* buffer)
{
    const char *ptr;
    char *bptr, ch;

    bptr = buffer;

    ptr = file + strlen(file);
    while (ptr >= file) {
        if (*ptr == '.') break;
        ptr--;
    }
    if (ptr >= file) {
        ptr++;
        while ((ch = *ptr) && ((bptr - buffer) < UT_MAX_FILE_EXTENSION)) {
            *bptr = ch;
            ptr++;
            bptr++;
        }
    }
    if ((bptr - buffer) >= UT_MAX_FILE_EXTENSION) {
        return NULL;
    } else {
        *bptr = '\0';
        return buffer;
    }
}

/* Get file base */
char* ut_file_base(char* file, char* buffer) {
    strcpy(buffer, file);
    char *ch = strrchr(buffer, '.');
    if (ch) {
        *ch = '\0';
    }
    return buffer;
}

/* Get file path */
char* ut_file_path(char* file, char* buffer) {
    int32_t i;

    strcpy(buffer, file);
    i = strlen(buffer);
    while (i >= 0) {
        if (buffer[i] == UT_OS_PS[0]) {
            buffer[i] = '\0';
            break;
        }
        i--;
    }

    return (i != -1) ? buffer : NULL;
}
