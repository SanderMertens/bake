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

#include <bake_test.h>

typedef char cdiff_identifier[512];

/* Optimistic parsing of C / C++ sourcefiles, extract function headers & bodies
 * so they can be replaced by updated definitions & new definitions can be added
 * without losing original content. */

static
void cdiff_addElem(
    ut_ll elements,
    char *id,
    char *header,
    char *body)
{
    cdiff_elem *elem = malloc (sizeof(cdiff_elem));

    if (id) {
        elem->kind = CDIFF_FUNCTION;
        elem->header = header;
        elem->body = body;
        elem->id = id;
    } else {
        elem->kind = CDIFF_TEXT;
        elem->id = NULL;
        elem->header = NULL;
        elem->body = body;
    }
    elem->isNew = false;

    ut_ll_append(elements, elem);
}

static
char* cdiff_skipMultiLineComment(
    char *src)
{
    char *ptr, ch;
    for (ptr = src; (ch = *ptr); ptr ++) {
        if (ch == '*') {
            if (ptr[1] == '/') {
                ptr += 2;
                break;
            }
        }
    }

    return ptr;
}

static
char* cdiff_skipLineComment(
    char *src)
{
    char *ptr, ch;
    for (ptr = src; (ch = *ptr); ptr ++) {
        if (ch == '\n') {
            break;
        }
    }

    return ptr;
}

static
char* cdiff_skipString(
    char *src,
    char delim)
{
    char *ptr, ch;
    for (ptr = src; (ch = *ptr) && (ch != delim); ptr ++) {
        if (ch == '\\') {
            ptr ++;
        }
    }

    return ptr;
}

static
char* cdiff_next(
    char *src,
    char match)
{
    char *ptr, ch;
    int nesting = 0;

    for (ptr = src; ptr && (ch = *ptr); ptr ++) {
        if (match == '}' && ch == '{') {
            nesting ++;
        }
        if (match == ')' && ch == '(') {
            nesting ++;
        }
        if (ch == match) {
            if (nesting) {
                nesting --;
            } else {
                break;
            }
        } else if (isspace(ch)) {
            /* Keep scanning */
        } else if (ch == '"') {
            ptr = cdiff_skipString(&ptr[1], '"');
        } else if (ch == '\'') {
            ptr = cdiff_skipString(&ptr[1], '\'');
        } else if (ch == '/') {
            /* Comment? */
            if (ptr[1] == '*') {
                ptr = cdiff_skipMultiLineComment(&ptr[2]);
            } else if (ptr[1] == '/') {
                ptr = cdiff_skipLineComment(&ptr[2]);
            }
        } else if (!nesting) {
            break;
        }
    }

    return ptr;
}

static
char* cdiff_scanId(
    char *src,
    ut_strbuf *buffer,
    char** t_start)
{
    char ch, *ptr = cdiff_next(src, 0);

    *t_start = NULL;

    /* Identifier found */
    if (isalpha(*ptr) || *ptr == '_') {
        *t_start = ptr;
    }

    /* Scan remainder of identifier */
    if (*t_start) {
        for (ptr ++; (ch = *ptr); ptr ++) {
            if (!isalpha(ch) && !isdigit(ch) && ch != '_' && ch != ':') {
                break;
            }
        }
    }

    /* Add scanned characters to buffer */
    ut_strbuf_appendstrn(buffer, src, ptr - src);

    return ptr;
}

static
char* cdiff_scanExpect(
    char *src,
    ut_strbuf *buffer,
    char match,
    char** t_start)
{
    char *ptr = cdiff_next(src, match);

    *t_start = NULL;

    /* Match found */
    if (match == *ptr) {
        *t_start = ptr;
        ptr ++;
    }

    /* Add scanned characters to buffer */
    ut_strbuf_appendstrn(buffer, src, ptr - src);

    return ptr;
}

static
char* cdiff_scanUntil(
    char *src,
    ut_strbuf *buffer,
    char match,
    char** t_start)
{
    char ch, *ptr;

    for (ptr = src; (ch = *ptr) && (ch != match); ptr = cdiff_next(ptr + 1, match));

    *t_start = NULL;

    /* Match found */
    if (match == ch) {
        *t_start = ptr;
        ptr ++;
    }

    /* Add scanned characters to buffer */
    ut_strbuf_appendstrn(buffer, src, ptr - src);

    return ptr;
}

static
char *cdiff_skipExtern(
    char *ptr,
    ut_strbuf *header,
    char **t_start)
{
    /* Test if last parsed token was 'extern' */
    if ((ptr - *t_start) == strlen("extern") &&
        !memcmp(*t_start, "extern", (size_t)(ptr - *t_start)))
    {
        /* If extern is found, it should be part of the header */
        char ch;
        for (; (ch = *ptr) && (ch != '"') && (ch != '\n'); ptr ++);
        if (ch == '"') {
            ptr ++;

            /* Language identifier found. Scan for next '"'. Expect on the same
             * line. */
            for (; (ch = *ptr) && (ch != '"') && (ch != '\n'); ptr ++);
            if (ch == '"') {
                ptr ++;

                /* End of language identifier found. Search until next non-
                 * whitespace character is found (may include newline) */
                for (; (ch = *ptr) && (isspace(ch)); ptr ++);

                /* Now parse type identifier, so the result points to where
                 * the function identifier starts */
                ptr = cdiff_scanId(ptr, header, t_start);
                if (!t_start) goto stop;
                return ptr;
            } else {
                ut_throw("missing language identifier for 'extern'");
                goto stop;
            }
        } else {
            goto stop;
        }

    } else {
        return ptr;
    }
stop:
    *t_start = NULL;
    return ptr;
}


static
char *cdiff_parseFunction(
    char *src,
    char *id,
    ut_strbuf *header,
    ut_strbuf *body)
{
    char *t_start;

    /* Parse type identifier */
    char *ptr = cdiff_scanId(src, header, &t_start);
    if (!t_start) goto stop;

    /* If token is 'extern', this is an extern possibly preceding a function */
    ptr = cdiff_skipExtern(ptr, header, &t_start);
    if (!t_start) goto stop;

    /* Parse identifier */
    ptr = cdiff_scanId(ptr, header, &t_start);
    if (!t_start) goto stop;

    /* Copy identifier into buffer */
    if (t_start[0] == '_') t_start++; /* Support legacy files */
    strncpy(id, t_start, (size_t)(ptr - t_start));
    id[ptr - t_start] = '\0';

    /* Expect ( */
    ptr = cdiff_scanExpect(ptr, header, '(', &t_start);
    if (!t_start) goto stop;

    /* Scan for ) */
    ptr = cdiff_scanUntil(ptr, header, ')', &t_start);
    if (!t_start) goto stop;

    /* Scan for { */
    ptr = cdiff_scanExpect(ptr, header, '{', &t_start);
    if (!t_start) goto stop;

    /* Scan for } */
    ptr = cdiff_scanUntil(ptr, body, '}', &t_start);
    if (!t_start) goto stop;

    return ptr;
stop:
    id[0] = '\0';
    ut_strbuf_reset(header);
    ut_strbuf_reset(body);
    return src;
}

static
char* cdiff_parseElem(
    char *src,
    char *id,
    ut_strbuf *header,
    ut_strbuf *body)
{
    char ch, *ptr = src;
    id[0] = '\0';

    if (isalpha(src[0])) {
        /* Potential function */
        ptr = cdiff_parseFunction(src, id, header, body);

        /* It is possible that this was not a function. In that case, continue
         * parsing until hitting newline character */
    }

    /* Keep adding until newline is found */
    for (; (ch = *ptr) && (ch != '\n'); ptr++) {
        ut_strbuf_appendstrn(body, &ch, 1);
    }
    if (ch) ut_strbuf_appendstrn(body, &ch, 1);

    return ch ? ptr + 1 : ptr;
}

static
ut_ll cdiff_parse(char *src) {
    ut_ll elements = ut_ll_new();
    cdiff_identifier id;
    char *ptr = src;

    ut_strbuf header = UT_STRBUF_INIT, body = UT_STRBUF_INIT;
    id[0] = '\0';

    while ((ptr = cdiff_parseElem(ptr, id, &header, &body)) && *ptr) {
        char *h = ut_strbuf_get(&header);
        char *b = ut_strbuf_get(&body);
        cdiff_addElem(elements, id[0] ? strdup(id) : NULL, h, b);
    }

    char *h = ut_strbuf_get(&header);
    char *b = ut_strbuf_get(&body);
    cdiff_addElem(elements, id[0] ? strdup(id) : NULL, h, b);

    return elements;
}

cdiff_file cdiff_file_open (
    const char* filename)
{
    cdiff_file result = malloc(sizeof(struct cdiff_file_s));

    /* Store current working directory in case the application using this
     * function changes the cwd before closing the file. */
    result->name = ut_asprintf("%s/%s", ut_cwd(), filename);

    result->elements = NULL;
    result->legacyElements = NULL;
    result->isChanged = false;
    result->writeBuffer = UT_STRBUF_INIT;
    result->writeTo = 0;
    result->indent = 0;
    result->newLine = true;
    result->cur = NULL;

    char *source = ut_file_load(result->name);
    if (source) {
        result->isNew = false;

        /* Parse file (only when not parsing legacy file) */
        if (!result->legacyElements) {
            result->elements = cdiff_parse(source);
            if (!result->elements) {
                goto error;
            }
        }

        free(source);

    } else {
        ut_catch();
        result->isNew = true;
    }

    return result;
error:
    if (source) {
        free(source);
    }
    return NULL;
}

static
void cdiff_file_writeElement(
    FILE *f,
    char *element)
{
    ut_assert(f != NULL, "NULL file passed to cdiff_file_writeElem");

    fprintf(f, "%s", element);
    free(element);
}

static
void cdiff_file_writeElements(
    FILE *f,
    ut_ll elements)
{
    ut_assert(f != NULL, "NULL file passed to cdiff_file_writeElements");

    ut_iter it = ut_ll_iter(elements);

    while (ut_iter_hasNext(&it)) {
        cdiff_elem *el = ut_iter_next(&it);

        if (el->header) {
            if (el->isNew && el->kind == CDIFF_FUNCTION) {
                /* If this is a new function, insert a newline. This allows the
                 * code that inserts the new function to not worry about
                 * newlines, and also prevents easy-to-make bugs where every
                 * generation adds a newline. */
                fprintf(f, "\n");
            }
            cdiff_file_writeElement(f, el->header);
        }

        if (el->body) {
            cdiff_file_writeElement(f, el->body);
        }

        if (el->id) free(el->id);
        free(el);
    }

    ut_ll_free(elements);
}

int16_t cdiff_file_close (
    cdiff_file file)
{
    /* If file didn't change, no need to overwrite file (speeds up building) */
    if (file->isChanged) {
        /* Open file for writing */
        FILE *f = fopen(file->name, "w");
        if (!f) {
            ut_throw("cannot open file '%s'", file->name);
            goto error;
        }

        /* Write & cleanup elements */
        cdiff_file_writeElements(f, file->elements);

        if (file->legacyElements) {
            cdiff_file_writeElements(f, file->legacyElements);
        }

        fclose(f);

        free(file->name);
        free(file);
    }

    return 0;
error:
    return -1;
}

static
cdiff_elem* cdiff_file_elemFind(
    ut_ll elements,
    char *id)
{
    ut_iter it = ut_ll_iter(elements);
    cdiff_elem *el = NULL;

    while (ut_iter_hasNext(&it)) {
        el = ut_iter_next(&it);

        if (el->id) {
            if (el->kind == CDIFF_FUNCTION_LEGACY) {
                cdiff_identifier legacyId, queryId;
                strcpy(queryId, id);

                /* Strip namespace from id as using different generator prefix
                 * can impact how package names map to the full id */
                char *ptr = strchr(el->id, '(');
                if (!ptr) {
                    ptr = &el->id[strlen(el->id) - 1];
                } else {
                    ptr[0] = '\0';
                    ptr --;
                }

                int count = 0;
                while (count < 2 && (--ptr) != el->id) {
                    if (ptr[0] == '/') count ++;
                }
                if (ptr[0] == '/') ptr ++;

                strcpy(legacyId, ptr);
                char ch;
                for (ptr = legacyId; (ch = *ptr); ptr ++) {
                    if (ch == '/') *ptr = '_';
                }

                /* Strip _v from query */
                size_t length = strlen(queryId);
                if (queryId[length - 2] == '_' && queryId[length - 1] == 'v') {
                    queryId[length - 2] = '\0';
                }

                /* Legacy id can be the full name, not relative to the
                 * package so there is no straightforward translation from
                 * id to function name. If id string occurs anywhere in the
                 * legacy id, it's a match. */
                if ((ptr = strstr(queryId, legacyId))) {
                    /* Ensure there aren't any trailing characters */
                    if (strlen(ptr) == strlen(legacyId)) {
                        break;
                    }
                }

                /* If query is in the form '...Main' it could refer to the main
                 * function of a package which is stored with id 'main' */
                if (strstr(queryId, "Main")) {
                    if (!strcmp(legacyId, "main")) {
                        break;
                    }
                }
            } else if (el->id) {
                if (!strcmp(el->id, id)) {
                    break;
                }
            }
        }
        el = NULL;
    }

    return el;
}

void cdiff_file_elemBegin(
    cdiff_file file,
    const char *fmt, ...)
{
    ut_assert(file != NULL, "NULL specified for file");

    if (!file->elements) {
        file->elements = ut_ll_new();
    }

    cdiff_elem *el = NULL;
    char *id = NULL;

    if (fmt) {
        va_list args;
        va_start(args, fmt);
        id = ut_vasprintf(fmt, args);
        va_end(args);

        el = cdiff_file_elemFind(file->elements, id);
        if (!el && file->legacyElements) {
            el = cdiff_file_elemFind(file->legacyElements, id);
        }
    }

    if (!el) {
        el = malloc (sizeof(cdiff_elem));
        el->id = id;
        el->header = NULL;
        el->body = NULL;
        el->kind = id ? CDIFF_FUNCTION : CDIFF_TEXT;
        el->isNew = true;
        file->isChanged = true;

        if (!file->elements) {
            file->elements = ut_ll_new();
        }
        ut_ll_append(file->elements, el);
    } else {
        free(id);
    }

    file->cur = el;
    file->writeTo = 0;
}

void cdiff_file_elemEnd(
    cdiff_file file)
{
    ut_assert(file != NULL, "NULL specified for file");
    ut_assert(file->writeTo == 0, "finish header or body before closing element");
    file->cur = NULL;
}

void cdiff_file_headerBegin(
    cdiff_file file)
{
    ut_assert(file != NULL, "NULL specified for file");
    ut_assert(file->cur != NULL, "select element before starting header");
    ut_assert(file->writeTo != 2, "end body before start writing to header");

    file->writeTo = 1;
    file->writeBuffer = UT_STRBUF_INIT;
}

void cdiff_file_headerEnd(
    cdiff_file file)
{
    ut_assert(file != NULL, "NULL specified for file");
    ut_assert(file->writeTo == 1, "not writing to header");

    char *header = ut_strbuf_get(&file->writeBuffer);
    if (!file->cur->header) {
        file->cur->header = header;
    } else if (strcmp(header, file->cur->header)) {
        free(file->cur->header);
        file->cur->header = ut_strdup(header);
        file->isChanged = TRUE;
    }

    file->writeTo = 0;
}

int16_t cdiff_file_bodyBegin(
    cdiff_file file)
{
    ut_assert(file != NULL, "NULL specified for file");
    ut_assert(file->cur != NULL, "select element before starting body");
    ut_assert(file->writeTo != 1, "end header before start writing to body");

    /* Test if body is already set */
    if (file->cur->body) {
        return 1;
    }

    file->writeBuffer = UT_STRBUF_INIT;
    file->writeTo = 2;

    return 0;
}

void cdiff_file_bodyEnd(
    cdiff_file file)
{
    ut_assert(file != NULL, "NULL specified for file");
    ut_assert(file->writeTo == 2, "not writing to body");

    file->cur->body = ut_strbuf_get(&file->writeBuffer);

    file->writeTo = 0;
    file->isChanged = TRUE;
}

void cdiff_file_write(
    cdiff_file file,
    char *fmt,
    ...)
{
    ut_assert(file != NULL, "NULL specified for file");

    if (file->newLine && file->indent) {
        int i;
        for (i = 0; i < file->indent * 4; i++) {
            ut_strbuf_appendstrn(&file->writeBuffer, " ", 1);
        }
    }

    va_list args;
    va_start(args, fmt);
    ut_strbuf_vappend(&file->writeBuffer, fmt, args);
    va_end(args);

    /* If no element specified for writing, write to new text element. */
    if (!file->cur && file->isNew) {
        cdiff_elem *el = calloc(sizeof(cdiff_elem), 1);
        el->kind = CDIFF_TEXT;
        el->body = ut_strbuf_get(&file->writeBuffer);
        if (!file->elements) {
            file->elements = ut_ll_new();
        }
        ut_ll_append(file->elements, el);
        file->writeBuffer = UT_STRBUF_INIT;
    }

    file->newLine = fmt[strlen(fmt) - 1] == '\n';
}

void cdiff_file_writeBuffer(
    cdiff_file file,
    ut_strbuf *buffer)
{
    char *str = ut_strbuf_get(buffer);
    cdiff_file_write(file, "%s", str);
    free(str);
}

void cdiff_file_indent(
    cdiff_file file)
{
    ut_assert(file != NULL, "NULL specified for file");
    file->indent ++;
}

void cdiff_file_dedent(
    cdiff_file file)
{
    ut_assert(file != NULL, "NULL specified for file");
    ut_assert(file->indent != 0, "too many dedents");
    file->indent --;
}

