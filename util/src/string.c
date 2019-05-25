/* Copyright (c) 2010-2019 the corto developers
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
int stricmp(const char *str1, const char *str2) {
    const char *ptr1, *ptr2;
    char ch1, ch2;
    ptr1 = str1;
    ptr2 = str2;

    while((ch1 = *ptr1) && (ch2 = *ptr2)) {
        if (ch1 == ch2) {
            ptr1++; ptr2++;
            continue;
        }
        if (ch1 < 97) ch1 = tolower(ch1);
        if (ch2 < 97) ch2 = tolower(ch2);
        if (ch1 != ch2) {
            return ch1 - ch2;
        }
        ptr1++;
        ptr2++;
    }

    return tolower(*ptr1) - tolower(*ptr2);
}

int strnicmp(const char *str1, int length, const char *str2) {
    const char *ptr1, *ptr2;
    char ch1, ch2;
    ptr1 = str1;
    ptr2 = str2;

    while((ch1 = *ptr1) && (ch2 = *ptr2)) {
        if (ptr1 - str1 >= (length - 1)) break;
        if (ch1 == ch2) {
            ptr1++; ptr2++;
            continue;
        }
        if (ch1 < 97) ch1 = tolower(ch1);
        if (ch2 < 97) ch2 = tolower(ch2);
        if (ch1 != ch2) {
            return ch1 - ch2;
        }
        ptr1++;
        ptr2++;
    }

    return tolower(*ptr1) - tolower(*ptr2);
}
#endif // !_WIN32

int idcmp(const char *str1, const char *str2) {
    const char *ptr1, *ptr2;
    char ch1, ch2;
    ptr1 = str1;
    ptr2 = str2;

    while((ch1 = *ptr1) && (ch2 = *ptr2)) {
        if ((ch1 == ch2) ||
            ((ch1 == '/' || ch1 == '.') && (ch2 == '/' || ch2 == '.'))) {
            ptr1++; ptr2++;
            continue;
        }

        if (ch1 < 97) ch1 = tolower(ch1);
        if (ch2 < 97) ch2 = tolower(ch2);
        if (ch1 != ch2) {
            return ch1 - ch2;
        }
        ptr1++;
        ptr2++;
    }

    return tolower(*ptr1) - tolower(*ptr2);
}

int tokicmp(char ** const str1, const char *str2, char sep) {
    const char *ptr2;
    char ch1, ch2, *ptr1;
    ptr1 = *str1;
    ptr2 = str2;
    int result = 0;

    while((ch1 = *ptr1) && (ch2 = *ptr2) && (ch1 != sep) && (ch2 != sep)) {
        if (ch1 == ch2) {
            ptr1++; ptr2++;
            continue;
        }
        if (ch1 < 97) ch1 = tolower(ch1);
        if (ch2 < 97) ch2 = tolower(ch2);
        if (ch1 != ch2) {
            return ch1 - ch2;
        }
        ptr1++;
        ptr2++;
    }

    result = tolower(*ptr1) - tolower(*ptr2);
    if (!*ptr2 && (*ptr1 == sep)) result = 0;
    if (!*ptr1 && (*ptr2 == sep)) result = 0;
    if ((*ptr1 == sep) && (*ptr2 == sep)) result = 0;

    if (!result) {
        *str1 = ptr1;
    }

    return result;
}

const char *strelem(const char *str) {
    const char *ptr;
    char ch;
    for (ptr = str; (ch = *ptr); ptr++) {
        if (ch == '(' || ch == '/') {
            break;
        }

        /* Skip '.', '..', but stop if '.' is used as scope separator */
        if (ch == '.') {
            if (ptr == str) {
                /* if . is first, it cannot be a scope separator */
            } else if (ptr[-1] == '/' || ptr[-1] == '.') {
                /* If prev is '/' or '.', it cannot be a scope operator */
            } else {
                /* It has to be a scope operator */
                break;
            }
        }
    }

    return ch ? ptr : NULL;
}

char *strappend(char *src, const char *fmt, ...) {
    char buff[1024];
    va_list args;
    unsigned int sourceLength = 0;

    va_start(args, fmt);
    vsprintf(buff, fmt, args);
    va_end(args);

    if (src) {
        sourceLength = strlen(src);
    }

    src = realloc(src, sourceLength + strlen(buff) + 1);
    strcpy(&src[sourceLength], buff);

    return src;
}

char *chresc(char *out, char in, char delimiter) {
    char *bptr = out;
    switch(in) {
    case '\a':
        *bptr++ = '\\';
        *bptr = 'a';
        break;
    case '\b':
        *bptr++ = '\\';
        *bptr = 'b';
        break;
    case '\f':
        *bptr++ = '\\';
        *bptr = 'f';
        break;
    case '\n':
        *bptr++ = '\\';
        *bptr = 'n';
        break;
    case '\r':
        *bptr++ = '\\';
        *bptr = 'r';
        break;
    case '\t':
        *bptr++ = '\\';
        *bptr = 't';
        break;
    case '\v':
        *bptr++ = '\\';
        *bptr = 'v';
        break;
    case '\\':
        *bptr++ = '\\';
        *bptr = '\\';
        break;
    default:
        if (in == delimiter) {
            *bptr++ = '\\';
            *bptr = delimiter;
        } else {
            *bptr = in;
        }
        break;
    }

    *(++bptr) = '\0';

    return bptr;
}

const char* chrparse(const char *in, char *out) {
    const char *result = in + 1;
    char ch;

    if (in[0] == '\\') {
        result ++;

        switch(in[1]) {
        case 'a':
            ch = '\a';
            break;
        case 'b':
            ch = '\b';
            break;
        case 'f':
            ch = '\f';
            break;
        case 'n':
            ch = '\n';
            break;
        case 'r':
            ch = '\r';
            break;
        case 't':
            ch = '\t';
            break;
        case 'v':
            ch = '\v';
            break;
        case '\\':
            ch = '\\';
            break;
        case '"':
            ch = '"';
            break;
        case '0':
            ch = '\0';
            break;
        case ' ':
            ch = ' ';
            break;
        case '$':
            ch = '$';
            break;
        default:
            ut_throw("unsupported escape character '%c'", in[1]);
            goto error;
        }
    } else {
        ch = in[0];
    }

    if (out) {
        *out = ch;
    }

    return result;
error:
    return NULL;
}

size_t stresc(char *out, size_t n, char delimiter, const char *in) {
    const char *ptr = in;
    char ch, *bptr = out, buff[3];
    size_t written = 0;
    while ((ch = *ptr++)) {
        if ((written += (chresc(buff, ch, delimiter) - buff)) <= n) {
            *bptr++ = buff[0];
            if ((ch = buff[1])) {
                *bptr = ch;
                bptr++;
            }
        }
    }

    if (bptr) {
        while (written < n) {
            *bptr = '\0';
            bptr++;
            written++;
        }
    }
    return written;
}

void ut_strset(char **out, const char *str) {
    if (*out) {
        free(*out);
    }
    if (str) {
        *out = ut_strdup(str);
    } else {
        *out = NULL;
    }
}

/* strdup is not a standard C function, so provide own implementation. */
char* ut_strdup(const char* str) {
    if (!str) {
        return NULL;
    } else {
        char *result = malloc(strlen(str) + 1);
        strcpy(result, str);
        return result;
    }
}

/**
 * From:
 * `asprintf.c' - asprintf
 *
 * copyright (c) 2014 joseph werle <joseph.werle@gmail.com>
 */

char* ut_asprintf (const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    char *result = ut_vasprintf(fmt, args);

    va_end(args);

    return result;
}

char* ut_vasprintf (const char *fmt, va_list args) {
    int size = 0;
    char *result  = NULL;
    va_list tmpa;

    va_copy(tmpa, args);

    size = vsnprintf(result, size, fmt, tmpa);

    va_end(tmpa);

    if (size < 0) { return NULL; }

    // alloc with size plus 1 for `\0'
    result = (char *) malloc(size + 1);

    if (!result) { return NULL; }

    vsprintf(result, fmt, args);
    return result;
}

char* strarg(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *tmp = ut_vasprintf(fmt, args);
    va_end(args);
    char *result = ut_setThreadString(tmp);
    free(tmp);
    return result;
}

/* Convert characters in string to uppercase */
char* strupper(char *str) {
    char *ptr, ch;
    ptr = str;
    while ((ch = *ptr)) {
        if (ch >= 97) *ptr = toupper(ch);
        ptr++;
    }
    return str;
}

/* Convert characters in string to lowercase */
char* strlower(char *str) {
    char *ptr, ch;
    ptr = str;
    while ((ch = *ptr)) {
        if (ch < 97) *ptr = tolower(ch);
        ptr++;
    }
    return str;
}

// You must free the result if result is non-NULL.
char* strreplace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep
    int len_with; // length of with
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    if (!orig)
        return NULL;
    if (!rep)
        rep = "";
    len_rep = strlen(rep);
    if (!with)
        with = "";
    len_with = strlen(with);

    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

void strreverse(
    char *str,
    int length)
{
    int start = 0;
    int end = length -1;
    while (start < end)
    {
        char tmp = str[end];
        str[end] = str[start];
        str[start] = tmp;

        start++;
        end--;
    }
}
