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

#ifndef _WIN32
#define _setenv(var, val) setenv(var, val, 1)
#define _unsetenv(var) unsetenv(var)
#else
#define _setenv(var, val) _putenv_s(var, val)
#define _unsetenv(var) _putenv_s(var, "")
#endif

int16_t ut_setenv(const char *varname, const char *value, ...) {
    if (value) {
        va_list arglist;
        va_start(arglist, value);
        char *buff = ut_venvparse(value, arglist);
        if (!buff) {
            va_end(arglist);
            goto error;
        }
        va_end(arglist);
        if (_setenv(varname, buff)) {
            ut_throw("failed to set variable '%s'", varname);
            goto error;
        }
        free(buff);
    } else {
        _unsetenv(varname);
    }
    return 0;
error:
    return -1;
}

char* ut_getenv(const char *varname) {
    return getenv(varname);
}

int path_included(
    const char *path_list,
    const char *new_path)
{
    int len = strlen(new_path);
    const char *ptr = path_list;
    char ch;

    do {
        if (ptr[0] == UT_ENV_PATH_SEPARATOR[0]) ptr ++;

        if (!strncmp(ptr, new_path, len)) {
            return true;
        }
    } while ((ptr = strchr(ptr, UT_ENV_PATH_SEPARATOR[0])));

    return false;
}

int16_t ut_appendenv(
    const char *varname, 
    const char *value, 
    ...) 
{
    if (value) {
        char *orig_value = ut_getenv(varname);
        va_list arglist;
        va_start(arglist, value);
        char *buff = ut_venvparse(value, arglist);
        if (!buff) {
            va_end(arglist);
            goto error;
        }
        va_end(arglist);

        char *new_value = NULL;
        if (orig_value) {
            if (!path_included(orig_value, buff)) {
                new_value = ut_asprintf(
                    "%s" UT_ENV_PATH_SEPARATOR "%s", buff, orig_value);
            }
        } else {
            new_value = buff;
        }

        if (new_value) {
            if (_setenv(varname, new_value)) {
                ut_throw("failed to set variable '%s'", varname);
                goto error;
            }

            if (new_value != buff) {
                free(new_value);
            }
        }

        free(buff);
    }

    return 0;
error:
    return -1;
}

static
int16_t ut_append_var(
    ut_strbuf *output,
    bool separator,
    const char *var)
{
    char *val = ut_getenv(var);
    if (!val) {
        ut_throw("environment variable '%s' is not set", var);
        goto error;
    }

    if (strlen(val)) {
        if (separator) {
            ut_strbuf_appendstr(output, UT_ENV_PATH_SEPARATOR);
        }
        ut_strbuf_appendstr(output, val);
    }

    return 0;
error:
    return -1;
}

static
int16_t ut_expand_path(
    ut_strbuf *output,
    const char *path)
{
    char *sep = strrchr(path, UT_OS_PS[0]);
    char *wildcard = NULL;
    if (sep) {
        wildcard = strchr(sep, '*');
    } else {
        wildcard = strchr(path, '*');
    }

    if (wildcard) {
        const char *dir = ".";
        const char *expr = path;

        if (sep && sep != path) {
            *sep = '\0';
            dir = path;
            expr = sep + 1;
        }

        ut_iter it;
        if (ut_dir_iter(dir, expr, &it)) {
            goto error;
        }

        int count = 0;
        while (ut_iter_hasNext(&it)) {
            char *file = ut_iter_next(&it);
            if (count) {
                ut_strbuf_appendstrn(output, " ", 1);
            }
            if (strcmp(dir, ".")) {
                ut_strbuf_append(output, "%s"UT_OS_PS"%s", dir, file);
            } else {
                ut_strbuf_append(output, "%s", file);
            }
            count ++;
        }
    } else {
        ut_strbuf_appendstr(output, path);
    }

    return 0;
error:
    return -1;
}

bool valid_file_char(char ch) {
    if (isdigit(ch) || isalpha(ch)) {
        return true;
    } else {
        switch(ch) {
        case '_':
        case '$':
        case '-':
        case '.':
        case '~':
            return true;
        default:
            return false;
        }
    }
}

char* ut_venvparse(
    const char* input,
    va_list arglist)
{
    const char *ptr;
    char *str = NULL, *result = NULL;
    char ch;
    ut_strbuf output = UT_STRBUF_INIT;
    ut_strbuf token = UT_STRBUF_INIT;
    ut_strbuf path = UT_STRBUF_INIT;

    str = ut_vasprintf(input, arglist);

    ptr = str;

    bool separator = false;
    bool env_has_separator = false;
    do {
        ch = *ptr;

        if (!isalpha(ch) && ch != '_') {
            char *token_str = ut_strbuf_get(&token);

            if (token_str) {
                if (token_str[0] == '$') {
                    if (ut_append_var(&path, env_has_separator, &token_str[1])){
                        free(token_str);
                        goto error;
                    }
                    env_has_separator = false;
                } else if (!strcmp(token_str, "~")) {
                    if (ut_append_var(&path, env_has_separator, UT_ENV_HOME)) {
                        free(token_str);
                        goto error;
                    }
                    env_has_separator = false;
                } else {
                    ut_strbuf_appendstr(&path, token_str);
                }
                free(token_str);
            }
        }

        if (ch == ':' && (ptr[1] == '$' || ptr[1] == '~')) {
            separator = true;
            env_has_separator = true;
        } else {
            separator = false;
        }

        if (!separator) {
            if (!valid_file_char(ch)) {
                if (isspace(ch) || !ch) {
                    char *path_str = ut_strbuf_get(&path);
                    if (path_str) {
                        if (ut_expand_path(&output, path_str)) {
                            free(path_str);
                            goto error;
                        }
                        free(path_str);
                    }

                    ut_strbuf_appendstrn(&output, &ch, 1);
                } else {
                    ut_strbuf_appendstrn(&path, &ch, 1);
                }

                ptr++;
            } else {
                char ch_out;
                ptr = chrparse(ptr, &ch_out);
                ut_strbuf_appendstrn(&token, &ch_out, 1);
            }
        } else {
            ptr ++;
        }
    } while (ch);

    free(str);

    result = ut_strbuf_get(&output);

    return result;
error:
    if (str) free(str);
    ut_strbuf_reset(&output);
    return NULL;
}

char* ut_envparse(const char* str, ...) {
    va_list arglist;
    char *result;

    va_start(arglist, str);
    result = ut_venvparse(str, arglist);
    va_end(arglist);
    return result;
}
