
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

/* Open file */
ut_code* ut_code_open(
    const char* fmt,
    ...)
{
    va_list args;
    va_start(args, fmt);
    char *name = ut_vasprintf(fmt, args);
    va_end(args);

    char ext[255];

    ut_code *result = malloc(sizeof(struct ut_code));
    result->file = NULL;
    result->indent = 0;
    result->name = name;

    ut_file_extension(name, ext);

    result->file = fopen(name, "w");
    if (!result->file) {
        ut_throw("%s: %s", name, strerror(errno));
        free(result);
        goto error;
    }

    return result;
error:
    ut_throw("failed to open file '%s'", name);
    return NULL;
}

/* Increase indentation */
void ut_code_indent(
    ut_code *file)
{
    file->indent++;
}

/* Decrease indentation */
void ut_code_dedent(
    ut_code *file)
{
    file->indent--;
}

/* Write to file */
int ut_code_write(
    ut_code *file,
    char* fmt,
    ...)
{
    va_list args;

    va_start(args, fmt);
    char *buffer = ut_vasprintf(fmt, args);
    va_end(args);

    /* Write indentation & string */
    if (file->indent && file->endLine) {
        if (fprintf(file->file, "%*s%s", file->indent * 4, " ", buffer) < 0) {
            ut_throw("ut_codeWrite: writing to outputfile failed.");
            goto error;
        }
    } else {
        if (fprintf(file->file, "%s", buffer) < 0) {
            ut_throw("ut_codeWrite: writing to outputfile failed.");
            goto error;
        }
    }

    file->endLine = buffer[strlen(buffer)-1] == '\n';

    free(buffer);

    return 0;
error:
    return -1;
}

void ut_code_close(ut_code *file) {
    fclose(file->file);
    free(file->name);
    free(file);
}

