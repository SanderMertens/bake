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

#include "bake.h"

static
bake_attr* bake_attr_parse_value(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    bake_attr *existing,
    JSON_Value *v);

static
int16_t bake_project_func_locate(
    bake_project *project,
    const char *package_id, /* Different from project when parsing dependee config */
    ut_strbuf *buffer,
    const char *argument)
{
    const char *value = NULL;
    if (!package_id) {
        package_id = project->id;
    }
    if (!strcmp(argument, "package")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_PROJECT);
    } else if (!strcmp(argument, "include")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_INCLUDE);
    } else if (!strcmp(argument, "etc")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_ETC);
    } else if (!strcmp(argument, "env")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_ENV);
    } else if (!strcmp(argument, "lib")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_LIB);
    } else if (!strcmp(argument, "app")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_APP);
    } else if (!strcmp(argument, "bin")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_BIN);
    }
    if (value) {
        ut_strbuf_appendstr(buffer, value);
        return 0;
    } else {
        ut_throw("${locate %s} failed for project '%s'",
            argument, package_id);
        return -1;
    }
}

static
int16_t bake_project_func_os(
    bake_project *project,
    const char *package_id,
    ut_strbuf *buffer,
    const char *argument)
{
    if (ut_os_match(argument)) {
        ut_strbuf_appendstr(buffer, "1");
    } else {
        ut_strbuf_appendstr(buffer, "0");
    }
    return 0;
}

static
bool bake_language_is_cpp(
    const char *str)
{
    if (!stricmp(str, "cpp") || !stricmp(str, "c++")) {
        return true;
    } else {
        return false;
    }
}

static
bool bake_project_language_match(
    const char *l1,
    const char *l2)
{
    if (!l1 && !l2) {
        return true;
    }
    if (!l1 || !l2) {
        return false;
    }

    bool l1_is_cpp = bake_language_is_cpp(l1);
    bool l2_is_cpp = bake_language_is_cpp(l2);

    if (l1_is_cpp && l2_is_cpp) {
        return true;
    }

    if (!stricmp(l1, l2)) {
        return true;
    }

    return false;
}

static
int16_t bake_project_func_language(
  bake_project *project,
  const char *package_id,
  ut_strbuf *buffer,
  const char *argument)
{
    const char *language = project->language;

    if (bake_project_language_match(language, argument)) {
        ut_strbuf_appendstr(buffer, "1");
    } else {
        ut_strbuf_appendstr(buffer, "0");
    }

    return 0;
}

static
int16_t bake_attr_func(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    ut_strbuf *buffer,
    const char *function,
    const char *argument)
{
    if (!strcmp(function, "locate")) {
        return bake_project_func_locate(project, package_id, buffer, argument);
    } else if (!strcmp(function, "os") || !strcmp(function, "target")) {
        return bake_project_func_os(project, package_id, buffer, argument);
    } else if (!strcmp(function, "language") || !strcmp(function, "lang")) {
        return bake_project_func_language(project, package_id, buffer, argument);
    } else {
        ut_throw("unknown function '%s'", function);
        return -1;
    }
    return 0;
}

char* bake_attr_replace(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    const char *input)
{
    const char *func = input, *next = NULL;
    ut_strbuf output = UT_STRBUF_INIT;
    bool replaced = false;

    while ((next = strchr(func, '$'))) {
        replaced = true;

        /* Add everything up until next $ */
        ut_strbuf_appendstrn(&output, (char*)func, next - func);

        /* Check whether value is a function */
        if (next[1] == '{') {
            /* Find end of function identifier */
            const char *start = &next[2], *end = strchr(next, ' ');
            const char *func_end = strchr(end ? end : start, '}');
            if (!end) {
                end = func_end;
            }
            if (!end) {
                ut_throw("no matching '}' in '%s'", input);
                free (ut_strbuf_get(&output));
                goto error;
            }

            /* Obtain identifier & check if it contains invalid characters */
            ut_id func_id = {0}, arg_id = {0};
            const char *ptr;
            for (ptr = start; ptr < end; ptr ++) {
                if (!isalpha(*ptr) && *ptr != '_' && !isdigit(*ptr)) {
                    ut_throw("invalid function identifier in '%s'", input);
                    free (ut_strbuf_get(&output));
                    goto error;
                }
                func_id[ptr - start] = *ptr;
            }
            func_id[ptr - start] = '\0';

            /* Obtain function argument (only one arg supported) */
            if (*end == ' ') {
                start = end + 1;
                end = func_end;
                for (ptr = start; ptr < end; ptr ++) {
                    if (!isalpha(*ptr) && *ptr != '_' && !isdigit(*ptr)) {
                        ut_throw("invalid function argument in '%s'", input);
                        free (ut_strbuf_get(&output));
                        goto error;
                    }
                    arg_id[ptr - start] = *ptr;
                }
                arg_id[ptr - start] = '\0';
            }

            if (bake_attr_func(
                config,
                project,
                package_id,
                &output,
                func_id,
                arg_id))
            {
                free (ut_strbuf_get(&output));
                goto error;
            }

            func = func_end + 1;
        } else {
            /* Keep $ */
            ut_strbuf_appendstrn(&output, "$", 1);
            func = next + 1;
        }
    }

    /* Append remaining input */
    if (func) {
        ut_strbuf_appendstr(&output, func);
    }

    if (replaced) {
        char *str = ut_strbuf_get(&output);
        return str;
    } else {
        return ut_strdup(input);
    }
error:
    return NULL;
}


static
bake_attr* bake_attr_parse_array(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    bake_attr *existing,
    JSON_Array *a)
{
    uint32_t i, count = json_array_get_count(a);
    bake_attr *result = existing;
    if (!result) {
        result = ut_calloc(sizeof(bake_attr));
        result->kind = BAKE_ARRAY;
        result->is.array = ut_ll_new();
    }

    for (i = 0; i < count; i ++) {
        JSON_Value *v = json_array_get_value(a, i);
        bake_attr *attr = bake_attr_parse_value(
            config, project, package_id, NULL, v);
        if (attr) {
            ut_ll_append(result->is.array, attr);
        } else {
            goto error;
        }
    }

    return result;
error:
    /* TODO: cleanup memory */
    return NULL;
}

static
bake_attr* bake_attr_parse_string(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    const char *str)
{
    bake_attr *result = ut_calloc(sizeof(bake_attr));
    result->kind = BAKE_STRING;

    if (str) {
        result->is.string = bake_attr_replace(
            config, project, package_id, str);
        if (!result->is.string) {
            free(result);
            return NULL;
        }
    } else {
        result->is.string = NULL;
    }

    return result;
}

static
bake_attr* bake_attr_parse_number(
    double v)
{
    bake_attr *result = ut_calloc(sizeof(bake_attr));
    result->kind = BAKE_NUMBER;

    result->is.number = v;

    return result;
}

static
bake_attr* bake_attr_parse_bool(
    bool v)
{
    bake_attr *result = ut_calloc(sizeof(bake_attr));
    result->kind = BAKE_BOOLEAN;

    result->is.boolean = v;

    return result;
}

static
bake_attr* bake_attr_parse_value(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    bake_attr *existing,
    JSON_Value *v)
{
    bake_attr *attr = NULL;

    JSON_Value_Type t = json_value_get_type(v);
    switch(t) {
    case JSONArray:
        attr = bake_attr_parse_array(
            config, project, package_id, existing, json_value_get_array(v));
        break;
    case JSONNull:
    case JSONString:
        attr = bake_attr_parse_string(
            config, project, package_id, json_value_get_string(v));
        break;
    case JSONNumber:
        attr = bake_attr_parse_number(json_value_get_number(v));
        break;
    case JSONBoolean:
        attr = bake_attr_parse_bool(json_value_get_boolean(v));
        break;
    case JSONObject:
        /* Ignore objects- placeholder */
        attr = ut_calloc(sizeof(bake_attr));
        attr->kind = BAKE_BOOLEAN;
        break;
    default:
        break;
    }

    return attr;
}

static
ut_ll bake_attr_parse_object(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    JSON_Object *jo,
    ut_ll existing)
{
    if (!jo) {
        ut_throw("bake_attr_parse_object: no object to parse");
        printf("no object error\n ");
        return NULL;
    }

    uint32_t i, count = json_object_get_count(jo);
    ut_ll result = existing ? existing : ut_ll_new();

    for (i = 0; i < count; i ++) {
        bake_attr *attr = NULL;
        JSON_Value *v = json_object_get_value_at(jo, i);
        const char *json_name = json_object_get_name(jo, i);
        char *name = (char*)json_name;

        if (name[0] == '$') {
            /* If name contains function, parse it */
            name = bake_attr_replace(
                config, project, NULL, json_name);
            if (!name) {
                printf("function error\n");
                goto error;
            }
        }

        if (!strcmp(name, "1") || !stricmp(name, "true")) {
            JSON_Value *value = json_object_get_value_at(jo, i);
            if (json_value_get_type(value) == JSONObject) {
                JSON_Object *obj = json_value_get_object(value);

                ut_try (!bake_attr_parse_object(
                    config, project, project_id, obj, result), NULL);
            } else {
                ut_throw("JSON object expected for expression '%s'", json_name);
                goto error;
            }
        } else

        if (!strcmp(name, "0") || !stricmp(name, "false")) {
            /* Ignore */
        } else {
            /* Check if attribute already exists */
            attr = bake_attr_get(result, name);

            /* Add member to list of project attributes */
            attr = bake_attr_parse_value(config, project, project_id, attr, v);
            if (attr) {
                attr->name = ut_strdup(name);
                ut_ll_append(result, attr);
            } else {
                ut_throw("failed to parse member '%s'", name);
                goto error;
            }
        }
    }

    return result;
error:
    return NULL;
}

ut_ll bake_attrs_parse(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    JSON_Object *object,
    ut_ll existing)
{
    return bake_attr_parse_object(
        config, project, project_id, object, existing);
}

bake_attr *bake_attr_get(
    ut_ll attributes,
    const char *name)
{
    if (attributes) {
        ut_iter it = ut_ll_iter(attributes);
        while (ut_iter_hasNext(&it)) {
            bake_attr *attr = ut_iter_next(&it);

            if (!strcmp(attr->name, name)) {
                return attr;
            }
        }
    }

    return NULL;
}

void bake_clean_string_array(
    ut_ll list)
{
    ut_iter it = ut_ll_iter(list);
    while (ut_iter_hasNext(&it)) {
        char *str = ut_iter_next(&it);
        free(str);
    }
    ut_ll_free(list);
}

void bake_attr_free_string_array(
    ut_ll list)
{
    ut_iter it = ut_ll_iter(list);
    while (ut_iter_hasNext(&it)) {
        free( ut_iter_next(&it));
    }
    ut_ll_free(list);
}

void bake_attr_free_attr_array(
    ut_ll list)
{
    ut_iter it = ut_ll_iter(list);
    while (ut_iter_hasNext(&it)) {
        bake_attr_free( ut_iter_next(&it));
    }
    ut_ll_free(list);
}

void bake_attr_free(
    bake_attr *attr)
{
    if (attr->kind == BAKE_STRING) {
        free(attr->is.string);
    } else if (attr->kind == BAKE_ARRAY) {
        bake_attr_free_string_array(attr->is.array);
    }
}
