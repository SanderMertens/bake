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

extern ut_tls UT_KEY_THREAD_STRING;

typedef struct ut_threadString_t {
    char* strings[UT_MAX_TLS_STRINGS];
    int32_t max[UT_MAX_TLS_STRINGS];
    uint8_t current;
} ut_threadString_t;

/* Set intern TLS string */
char* ut_setThreadString(char* string) {
    ut_threadString_t *data = ut_tls_get(UT_KEY_THREAD_STRING);
    int32_t len = strlen(string);
    int32_t max = 0;

    if (!data) {
        data = ut_calloc(sizeof(ut_threadString_t));
        ut_tls_set(UT_KEY_THREAD_STRING, data);
    }

    /* Select next string */
    data->current = (data->current + 1) % UT_MAX_TLS_STRINGS;
    max = data->max[data->current];

    if (data->strings[data->current] &&
       ((max < len) || (max > UT_MAX_TLS_STRINGS_MAX))) {
        free(data->strings[data->current]);
        data->strings[data->current] = ut_strdup(string);
        data->max[data->current] = len;
    } else {
        if (data->strings[data->current]) {
            strcpy(data->strings[data->current], string);
        } else {
            data->strings[data->current] = ut_strdup(string);
            data->max[data->current] = len;
        }
    }

    return data->strings[data->current];
}

/* Clean TLS string data */
void ut_threadStringDealloc(void *tdata) {
    ut_threadString_t *data = tdata;

    if (data) {
        int32_t i;
        for (i = 0; i < UT_MAX_TLS_STRINGS; i++) {
            if (data->strings[i]) {
                free(data->strings[i]);
            }
        }
    }

    free(data);
}

static char* g_num = "0123456789";
static int nmax = 1;

char* ut_itoa(
    int num,
    char* buff) {
    char* buffptr;
    unsigned int ch;
    unsigned int ntest;
    int i;
    int ignoreNull;

    if (nmax == 1) {
        i = log10(INT_MAX);
        for (; i>=1; i--) {
            nmax *= 10;
        }
    }

    ntest = nmax;
    buffptr = buff;
    ignoreNull = TRUE;

    if (!num) {
        *buffptr = '0';
        buffptr++;
    } else {
        if (num < 0) {
            *buffptr = '-';
            buffptr++;
            num *= -1;
        }

        while (ntest) {
            ch = num / ntest;
            if (ch) ignoreNull = FALSE;

            if (!ignoreNull) {
                *buffptr = g_num[ch];
                buffptr++;
            }

            num -= ch * ntest;
            ntest /= 10;
        }
    }

    *buffptr = '\0';

    return buffptr;
}

char* ut_ulltoa(
    uint64_t value,
    char *str,
    int base) {
    int i = 0;

    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (value == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    /* Process individual digits */
    while (value != 0)
    {
        int rem = value % base;
        if (rem > 9) {
            str[i++] = (rem-10) + 'a';
        } else {
            str[i++] = rem + '0';
        }

        value = value / base;
    }

    str[i] = '\0';

    strreverse(str, i);

    return str;
}

int32_t ut_pathToArray(
    char *path,
    const char *elements[],
    char *sep)
{
    int32_t count = 0;
    char *ptr = path;

    if (!path) {
        ut_throw("passed NULL to pathToArray");
        goto error;
    }

    if (!path[0]) {
        elements[0] = path;
        return 1;
    } else if (*ptr == *sep) {
        elements[count ++] = ptr;
        *ptr = '\0';
        ptr ++;
    }

    if (*ptr) {
        do {
            /* Never parse more elements than maximum scope depth */
            if (count == UT_MAX_SCOPE_DEPTH) {
                ut_throw(
                    "number of elements in path exceeds MAX_SCOPE_DEPTH (%d)",
                    UT_MAX_SCOPE_DEPTH);
                goto error;
            }
            if (*ptr == *sep) {
                *ptr = '\0';
                ptr++;
            }
            elements[count ++] = ptr;
        } while ((ptr = strchr(ptr, *sep)));
    }

    return count;
error:
    return -1;
}
