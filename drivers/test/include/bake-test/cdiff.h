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

#ifndef CDIFF_H
#define CDIFF_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cdiff_elemKind {
    CDIFF_TEXT,
    CDIFF_FUNCTION,
    CDIFF_FUNCTION_LEGACY
} cdiff_elemKind;

typedef struct cdiff_elem {
    cdiff_elemKind kind;
    char *id;
    char *header;
    char *body;
    bool isNew;
} cdiff_elem;

typedef struct cdiff_file_s *cdiff_file;
struct cdiff_file_s {
    char *name;
    bool isNew;
    bool isChanged;
    ut_ll elements;
    ut_ll legacyElements;
    cdiff_elem *cur;
    int writeTo; /* 0 = nothing, 1 = header, 2 = body */
    ut_strbuf writeBuffer;
    int indent;
    bool newLine; /* Is next write starting on new line */
};

cdiff_file cdiff_file_open(const char *file);

void cdiff_file_elemBegin(cdiff_file file, const char *fmt, ...);
void cdiff_file_elemEnd(cdiff_file file);

void cdiff_file_headerBegin(cdiff_file file);
void cdiff_file_headerEnd(cdiff_file file);

int16_t cdiff_file_bodyBegin(cdiff_file file); /* return 1 if body exists */
void cdiff_file_bodyEnd(cdiff_file file);

void cdiff_file_write(
    cdiff_file file,
    char *fmt,
    ...);

void cdiff_file_writeBuffer(
    cdiff_file file,
    ut_strbuf *buffer);

void cdiff_file_indent(cdiff_file file);
void cdiff_file_dedent(cdiff_file file);

int16_t cdiff_file_close(cdiff_file file);

#ifdef __cplusplus
}
#endif

#endif /* CDIFF_H */
