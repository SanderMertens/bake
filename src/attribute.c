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

#include "bake.h"

extern ut_tls BAKE_DRIVER_KEY;

static
bake_attr* bake_attr_parse_value(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    bake_attr *existing,
    JSON_Value *v);

/** ${locate ...} bake function.
 * This function returns paths associated with the bake project, like the
 * include path, etc path, metadata path and path to the project binary. */
static
int16_t bake_project_func_locate(
    bake_project *project,
    const char *package_id,
    ut_strbuf *buffer,
    const char *argument)
{
    const char *value = NULL;
    if (!package_id) {
        package_id = project->id;
    }

    if (!argument) {
        ut_throw("missing argument for locate function");
        return -1;
    }

    if (!strcmp(argument, "package")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_PROJECT);
    } else if (!strcmp(argument, "template")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_TEMPLATE);
    } else if (!strcmp(argument, "include")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_INCLUDE);
    } else if (!strcmp(argument, "etc")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_ETC);
    } else if (!strcmp(argument, "lib")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_LIB);
    } else if (!strcmp(argument, "app")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_APP);
    } else if (!strcmp(argument, "bin")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_BIN);
    } else if (!strcmp(argument, "src")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_SOURCE);
    } else if (!strcmp(argument, "devsrc")) {
        value = ut_locate(package_id, NULL, UT_LOCATE_DEVSRC);
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

/** ${os ...} bake function.
 * This function returns 'true' if the argument passes into the function matches
 * the current operating system, or false if it doesn't. */
static
int16_t bake_project_func_os(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    ut_strbuf *buffer,
    const char *argument)
{
    if (!argument) {
        ut_strbuf_appendstr(buffer, config->build_target);
    } else {
        if (!strcmp(config->build_target, UT_PLATFORM_STRING)) {
            if (ut_os_match(argument)) {
                ut_strbuf_appendstr(buffer, "1");
            } else {
                ut_strbuf_appendstr(buffer, "0");
            }
        } else {
            if (!stricmp(config->build_target, argument)) {
                ut_strbuf_appendstr(buffer, "1");
            } else {
                ut_strbuf_appendstr(buffer, "0");   
            }
        }
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

/** ${language ...} bake function.
 * This function returns 'true' if the argument passes into the function matches
 * the current language system, or false if it doesn't. */
static
int16_t bake_project_func_language(
    bake_project *project,
    const char *package_id,
    ut_strbuf *buffer,
    const char *argument)
{
    const char *language = project->language;

    if (!argument) {
        ut_strbuf_appendstr(buffer, project->language);
    } else {
        if (bake_project_language_match(language, argument)) {
            ut_strbuf_appendstr(buffer, "1");
        } else {
            ut_strbuf_appendstr(buffer, "0");
        }
    }

    return 0;
}

static
int16_t bake_project_func_id(
    bake_project *project,
    const char *package_id,
    ut_strbuf *buffer,
    const char *argument)
{
    if (!argument) {
        ut_strbuf_appendstr(buffer, package_id);
    } else if (!strcmp(argument, "base")) {
        const char *result = strrchr(package_id, '.');
        if (!result) {
            result = package_id;
        } else {
            result ++;
        }
        ut_strbuf_appendstr(buffer, result);
    } else if (!strcmp(argument, "upper")) {
        char *result = ut_strdup(package_id);
        char *ptr, ch;
        for (ptr = result; (ch = *ptr); ptr ++) {
            if (ch == '.') {
                ptr[0] = '_';
            } else {
                ptr[0] = toupper(ch);
            }
        }
        ut_strbuf_appendstr(buffer, result);
    } else if (!strcmp(argument, "dash")) {
        char *result = ut_strdup(package_id);
        char *ptr, ch;
        for (ptr = result; (ch = *ptr); ptr ++) {
            if (ch == '.') {
                ptr[0] = '-';
            }
        }
        ut_strbuf_appendstr(buffer, result);
    } else if (!strcmp(argument, "underscore")) {
        char *result = ut_strdup(package_id);
        char *ptr, ch;
        for (ptr = result; (ch = *ptr); ptr ++) {
            if (ch == '.') {
                ptr[0] = '_';
            }
        }
        ut_strbuf_appendstr(buffer, result);
    } else if (!strcmp(argument, "camelcase")) {
        char *result = ut_strdup(package_id);
        char *ptr, ch;
        for (ptr = result; (ch = *ptr); ptr ++) {
            if (ch == '.') {
                ptr[0] = toupper(ptr[1]);
                ptr ++;
            }
        }
        ut_strbuf_appendstr(buffer, result);
    } else if (!strcmp(argument, "pascalcase")) {
        char *result = ut_strdup(package_id);
        char *ptr, ch;
        for (ptr = result; (ch = *ptr); ptr ++) {
            if (ch == '.') {
                ptr[0] = toupper(ptr[1]);
                ptr ++;
            } else if (ptr == result) {
                ptr[0] = toupper(ch);
            }
        }
        ut_strbuf_appendstr(buffer, result);
    }

    return 0;
}

static
int16_t bake_project_func_driver_attr(
    bake_project *project,
    const char *package_id,
    ut_strbuf *buffer,
    const char *argument)
{
    if (!argument) {
        ut_throw("argument is mandatory for driver-attr function");
        goto error;
    }

    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    if (!driver) {
        ut_throw("cannot get driver-attr '%s', no driver active", argument);
        goto error;
    }

    bake_attr *attr = bake_project_get_attr(project, driver->id, argument);
    if (!attr) {
        ut_throw("attribute '%s' not found in driver configuration '%s'",
            argument, driver->id);
        goto error;
    }

    if (attr->kind == BAKE_BOOLEAN) {
        ut_strbuf_append(buffer, "%s", attr->is.boolean ? "true" : "false");
    } else if (attr->kind == BAKE_NUMBER) {
        ut_strbuf_append(buffer, "%d", attr->is.number);
    } else if (attr->kind == BAKE_STRING) {
        ut_strbuf_appendstr(buffer, attr->is.string);
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_project_func_config(
    bake_config *config,
    ut_strbuf *buffer,
    const char *argument)
{
    if (!argument) {
        ut_strbuf_appendstr(buffer, config->configuration);
    } else {
        if (!stricmp(argument, config->configuration)) {
            ut_strbuf_appendstr(buffer, "1");
        } else {
            ut_strbuf_appendstr(buffer, "0");
        }
    }

    return 0;
}

/** Forward a function call from project.json to the right implementation. */
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
        return bake_project_func_os(config, project, package_id, buffer, argument);
    } else if (!strcmp(function, "language") || !strcmp(function, "lang")) {
        return bake_project_func_language(project, package_id, buffer, argument);
    } else if (!strcmp(function, "id")) {
        return bake_project_func_id(project, package_id, buffer, argument);
    } else if (!strcmp(function, "driver-attr")) {
        return bake_project_func_driver_attr(project, package_id, buffer, argument);
    } else if (!strcmp(function, "config") || !strcmp(function, "cfg")) {
        return bake_project_func_config(config, buffer, argument);
    } else {
        ut_throw("unknown function '%s'", function);
        return -1;
    }
    return 0;
}

/** Parse a string from a project.json configuration, parse functions and
 * environment variables. */
char* bake_attr_replace(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    const char *input)
{
    const char *func = input, *next = NULL;
    ut_strbuf output = UT_STRBUF_INIT;
    bool replaced = false;
    bool indirect = false;
    bool has_arg = false;

    while ((next = strchr(func, '$'))) {
        replaced = true;
        indirect = false;
        has_arg = false;

        /* Add everything up until next $ */
        ut_strbuf_appendstrn(&output, (char*)func, next - func);

        /* If two subsequent $s, use dependee project instead of dependency */
        if (next[1] == '$') {
            next ++;
            indirect = true;
        }

        /* Check whether value is a function */
        if (next[1] == '{') {
            /* Find end of function identifier */
            const char *start = &next[2], *end = strchr(next, ' ');
            const char *func_end = strchr(next, '}');
            if (!end || func_end < end) {
                end = func_end;
            }

            if (!end) {
                ut_throw("no matching '}' in '%s'", input);
                free (ut_strbuf_get(&output));
                goto error;
            }

            /* Obtain identifier & check if it contains invalid characters */
            char func_id[512] = {0}, arg_id[512] = {0};
            const char *ptr;
            for (ptr = start; ptr < end; ptr ++) {
                if (!isalpha(*ptr) && *ptr != '_' && *ptr != '-' && !isdigit(*ptr)) {
                    ut_throw("invalid function identifier in '%s'", input);
                    ut_strbuf_reset(&output);
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
                    arg_id[ptr - start] = *ptr;
                }
                arg_id[ptr - start] = '\0';

                if (strlen(arg_id)) {
                    has_arg = true;
                }
            }

            if (bake_attr_func(
                config,
                project,
                indirect ? project->id : package_id,
                &output,
                func_id,
                has_arg ? arg_id : NULL))
            {
                ut_strbuf_reset(&output);
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

/* Parse JSON array, return ARRAY attribute */
static
bake_attr* bake_attr_parse_array(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    bake_attr *existing,
    JSON_Array *a)
{
    if (existing && existing->kind != BAKE_ARRAY) {
        ut_throw("attribute '%s' is not an array", existing->name);
        goto error;
    }

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

/* Return STRING attribute */
static
bake_attr* bake_attr_parse_string(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    bake_attr *attr,
    const char *str)
{
    if (attr && attr->kind != BAKE_STRING) {
        ut_throw("attribute '%s' is not a string", attr->name);
        goto error;
    }

    if (attr && attr->is.string) {
        free(attr->is.string);
    }

    bake_attr *result = attr ? attr : ut_calloc(sizeof(bake_attr));
    result->kind = BAKE_STRING;

    if (str) {
        result->is.string = bake_attr_replace(
            config, project, package_id, str);
        if (!result->is.string) {
            free(result);
            goto error;
        }
    } else {
        result->is.string = NULL;
    }

    return result;
error:
    return NULL;
}

/* Return NUMBER attribute */
static
bake_attr* bake_attr_parse_number(
    bake_attr *attr,
    double v)
{
    if (attr && attr->kind != BAKE_NUMBER) {
        ut_throw("attribute '%s' is not a number", attr->name);
        goto error;
    }

    bake_attr *result = attr ? attr : ut_calloc(sizeof(bake_attr));
    result->kind = BAKE_NUMBER;
    result->is.number = v;

    return result;
error:
    return NULL;
}

/* Return BOOLEAN attribute */
static
bake_attr* bake_attr_parse_bool(
    bake_attr *attr,
    bool v)
{
    if (attr && attr->kind != BAKE_BOOLEAN) {
        ut_throw("attribute '%s' is not a boolean", attr->name);
        goto error;
    }

    bake_attr *result = attr ? attr : ut_calloc(sizeof(bake_attr));
    result->kind = BAKE_BOOLEAN;
    result->is.boolean = v;

    return result;
error:
    return NULL;
}

/* Parse JSON value */
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
            config, project, package_id, existing, json_value_get_string(v));
        break;
    case JSONNumber:
        attr = bake_attr_parse_number(existing, json_value_get_number(v));
        break;
    case JSONBoolean:
        attr = bake_attr_parse_bool(existing, json_value_get_boolean(v));
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

/* Parse JSON object */
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
            name = bake_attr_replace(config, project, project_id, json_name);
            if (!name) {
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
            ut_try( bake_attr_add(
                config, project, project_id, result, name, v), NULL);
        }
    }

    return result;
error:
    return NULL;
}

/* Parse JSON object */
int16_t bake_attr_add(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    ut_ll attributes,
    const char *name,
    JSON_Value *value)
{
    bool new_attr = false;

    /* Check if attribute already exists */
    bake_attr *attr = bake_attr_get(attributes, name);
    if (!attr) {
        new_attr = true;
    }

    /* Add member to list of project attributes */
    attr = bake_attr_parse_value(config, project, project_id, attr, value);
    if (attr && new_attr) {
        attr->name = ut_strdup(name);
        ut_ll_append(attributes, attr);
    } else if (!attr) {
        ut_throw("failed to parse member '%s'", name);
        goto error;
    }

    return 0;
error:
    return -1;
}

/** Parse JSON into new attribute list, or append to existing list */
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

/** Find attribute in list */
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
        bake_attr *attr = ut_iter_next(&it);
        bake_attr_free(attr);
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
    free(attr->name);
    free(attr);
}
