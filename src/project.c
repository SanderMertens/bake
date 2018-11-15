/* Copyright (c) 2010-2018 the corto developers
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
#include "parson.h"

extern corto_tls BAKE_PROJECT_KEY;

static
bake_project_attr* bake_project_parseValue(
    bake_project *p,
    const char *package_id,
    bake_project_attr *existing,
    JSON_Value *v);

/* Utility to assing string from JSON value */
static
int16_t bake_assign_json_string(char **ptr, JSON_Value *v) {
    if (json_value_get_type(v) != JSONString &&
        json_value_get_type(v) != JSONNull)
    {
        return -1;
    }
    if (*ptr) {
        free(*ptr);
    }
    const char *str = json_value_get_string(v);
    if (str) {
        *ptr = strdup(str);
    } else {
        *ptr = NULL;
    }
    return 0;
}

static
int16_t bake_project_func_locate(
    bake_project *p,
    const char *package_id, /* Different from p when parsing dependee config */
    corto_buffer *buffer,
    const char *argument)
{
    const char *value = NULL;
    if (!package_id) {
        package_id = p->id;
    }
    if (!strcmp(argument, "package")) {
        value = corto_locate(package_id, NULL, CORTO_LOCATE_PACKAGE);
    } else if (!strcmp(argument, "include")) {
        value = corto_locate(package_id, NULL, CORTO_LOCATE_INCLUDE);
    } else if (!strcmp(argument, "etc")) {
        value = corto_locate(package_id, NULL, CORTO_LOCATE_ETC);
    } else if (!strcmp(argument, "env")) {
        value = corto_locate(package_id, NULL, CORTO_LOCATE_ENV);
    } else if (!strcmp(argument, "lib")) {
        value = corto_locate(package_id, NULL, CORTO_LOCATE_LIB);
    } else if (!strcmp(argument, "app")) {
        value = corto_locate(package_id, NULL, CORTO_LOCATE_APP);
    } else if (!strcmp(argument, "bin")) {
        value = corto_locate(package_id, NULL, CORTO_LOCATE_BIN);
    }
    if (value) {
        corto_buffer_appendstr(buffer, value);
        return 0;
    } else {
        corto_throw("${locate %s} failed for project '%s'",
            argument, package_id);
        return -1;
    }
}

static
int16_t bake_project_func_os(
    bake_project *p,
    const char *package_id,
    corto_buffer *buffer,
    const char *argument)
{
    if (corto_os_match(argument)) {
        corto_buffer_appendstr(buffer, "1");
    } else {
        corto_buffer_appendstr(buffer, "0");
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
  bake_project *p,
  const char *package_id,
  corto_buffer *buffer,
  const char *argument)
{
    const char *language = p->language;
    if (p->c4cpp) {
        language = "cpp";
    }

    if (bake_project_language_match(language, argument)) {
        corto_buffer_appendstr(buffer, "1");
    } else {
        corto_buffer_appendstr(buffer, "0");
    }
    return 0;
}

static
int16_t bake_project_func_call(
    bake_project *p,
    const char *package_id,
    corto_buffer *buffer,
    const char *function,
    const char *argument)
{
    if (!strcmp(function, "locate")) {
        return bake_project_func_locate(p, package_id, buffer, argument);
    } else if (!strcmp(function, "os") || !strcmp(function, "target")) {
        return bake_project_func_os(p, package_id, buffer, argument);
    } else if (!strcmp(function, "language") || !strcmp(function, "lang")) {
        return bake_project_func_language(p, package_id, buffer, argument);
    } else {
        corto_throw("unknown function '%s'", function);
        return -1;
    }
    return 0;
}

static
char* bake_project_replace(
    bake_project *p,
    const char *package_id,
    const char *input)
{
    const char *func = input, *next = NULL;
    corto_buffer output = CORTO_BUFFER_INIT;
    bool replaced = false;

    while ((next = strchr(func, '$'))) {
        replaced = true;

        /* Add everything up until next $ */
        corto_buffer_appendstrn(&output, (char*)func, next - func);

        /* Check whether value is a function */
        if (next[1] == '{') {
            /* Find end of function identifier */
            const char *start = &next[2], *end = strchr(next, ' ');
            const char *func_end = strchr(end ? end : start, '}');
            if (!end) {
                end = func_end;
            }
            if (!end) {
                corto_throw("no matching '}' in '%s'", input);
                free (corto_buffer_str(&output));
                goto error;
            }

            /* Obtain identifier & check if it contains invalid characters */
            corto_id func_id = {0}, arg_id = {0};
            const char *ptr;
            for (ptr = start; ptr < end; ptr ++) {
                if (!isalpha(*ptr) && *ptr != '_' && !isdigit(*ptr)) {
                    corto_throw("invalid function identifier in '%s'", input);
                    free (corto_buffer_str(&output));
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
                        corto_throw("invalid function argument in '%s'", input);
                        free (corto_buffer_str(&output));
                        goto error;
                    }
                    arg_id[ptr - start] = *ptr;
                }
                arg_id[ptr - start] = '\0';
            }

            if (bake_project_func_call(
                p,
                package_id,
                &output,
                func_id,
                arg_id))
            {
                free (corto_buffer_str(&output));
                goto error;
            }

            func = func_end + 1;
        } else {
            /* Keep $ */
            corto_buffer_appendstrn(&output, "$", 1);
            func = next + 1;
        }
    }

    /* Append remaining input */
    if (func) {
        corto_buffer_appendstr(&output, func);
    }

    if (replaced) {
        char *str = corto_buffer_str(&output);
        return str;
    } else {
        return corto_strdup(input);
    }
error:
    return NULL;
}

static
void bake_clean_string_array(
    corto_ll list)
{
    corto_iter it = corto_ll_iter(list);
    while (corto_iter_hasNext(&it)) {
        char *str = corto_iter_next(&it);
        free(str);
    }
    corto_ll_free(list);
}

/* Utility to iterate over strings in a JSON array */
static
int16_t bake_append_json_array(
    bake_project *p,
    const char *project_id,
    JSON_Value *v,
    void(*action)(bake_project*,const char*))
{
    if (json_value_get_type(v) == JSONArray) {
        JSON_Array *a = json_value_get_array(v);
        uint32_t i, count = json_array_get_count(a);
        for (i = 0; i < count; i ++) {
            JSON_Value *v = json_array_get_value(a, i);
            const char *str = json_value_get_string(v);
            if (str) {
                action(p, str);
            }
        }
    } else {
        goto error;
    }
    return 0;
error:
    return -1;
}


bake_project_attr *bake_project_getattr(
    bake_project *p,
    const char *name)
{
    if (p->attributes) {
        corto_iter it = corto_ll_iter(p->attributes);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *attr = corto_iter_next(&it);

            if (!strcmp(attr->name, name)) {
                return attr;
            }
        }
    }

    return NULL;
}

char *bake_project_getattr_tostr(
    bake_project_attr *result)
{
    switch(result->kind) {
    case BAKE_ATTR_STRING:
        if (result->is.string) {
            return corto_strdup(result->is.string);
        } else {
            return NULL;
        }
    case BAKE_ATTR_BOOLEAN:
        if (result->is.boolean) {
            return corto_strdup("true");
        } else {
            return corto_strdup("false");
        }
    case BAKE_ATTR_NUMBER:
        return corto_asprintf("%f", result->is.number);
    case BAKE_ATTR_ARRAY:
        if (result->is.array) {
            corto_buffer buf = CORTO_BUFFER_INIT;
            corto_iter it = corto_ll_iter(result->is.array);
            int count;
            while (corto_iter_hasNext(&it)) {
                bake_project_attr *attr = corto_iter_next(&it);
                if (count) {
                    corto_buffer_appendstr(&buf, " ");
                }
                char *str = bake_project_getattr_tostr(attr);
                corto_buffer_appendstr(&buf, str);
                free(str);
                count ++;
            }
            return corto_buffer_str(&buf);
        }
    }

    return NULL;
}

bool bake_project_getattr_tobool(
    bake_project_attr *result)
{
    switch(result->kind) {
    case BAKE_ATTR_STRING:
        if (result->is.string) {
            return strcmp(result->is.string, "false");
        } else {
            return false;
        }
    case BAKE_ATTR_BOOLEAN:
        return result->is.boolean;
    case BAKE_ATTR_NUMBER:
        return result->is.number != 0;
    case BAKE_ATTR_ARRAY:
        return false;
    }

    return false;
}

char *bake_project_getattr_string_cb(const char *name) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_assert(p != NULL, "project::getattr called without project context");

    bake_project_attr *result = bake_project_getattr(p, name);

    if (result) {
        return bake_project_getattr_tostr(result);
    } else {
        return "";
    }
}

bool bake_project_getattr_bool_cb(const char *name) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_assert(p != NULL, "project::getattr called without project context");

    bake_project_attr *result = bake_project_getattr(p, name);

    if (result) {
        return bake_project_getattr_tobool(result);
    } else {
        return false;
    }
}

static
bake_project_attr* bake_project_getattr_cb(const char *name) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_assert(p != NULL, "project::getattr called without project context");

    return bake_project_getattr(p, name);
}

static
bake_project_attr* bake_project_parseArray(
    bake_project *p,
    const char *package_id,
    bake_project_attr *existing,
    JSON_Array *a)
{
    uint32_t i, count = json_array_get_count(a);
    bake_project_attr *result = existing;
    if (!result) {
        result = corto_calloc(sizeof(bake_project_attr));
        result->kind = BAKE_ATTR_ARRAY;
        result->is.array = corto_ll_new();
    }

    for (i = 0; i < count; i ++) {
        JSON_Value *v = json_array_get_value(a, i);
        bake_project_attr *attr = bake_project_parseValue(p, package_id, NULL, v);
        if (attr) {
            corto_ll_append(result->is.array, attr);
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
bake_project_attr* bake_project_parseString(
    bake_project *p,
    const char *package_id,
    const char *str)
{
    bake_project_attr *result = corto_calloc(sizeof(bake_project_attr));
    result->kind = BAKE_ATTR_STRING;

    if (str) {
        result->is.string = bake_project_replace(p, package_id, str);
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
bake_project_attr* bake_project_parseNumber(
    double v)
{
    bake_project_attr *result = corto_calloc(sizeof(bake_project_attr));
    result->kind = BAKE_ATTR_NUMBER;

    result->is.number = v;

    return result;
}

static
bake_project_attr* bake_project_parseBoolean(
    bool v)
{
    bake_project_attr *result = corto_calloc(sizeof(bake_project_attr));
    result->kind = BAKE_ATTR_BOOLEAN;

    result->is.boolean = v;

    return result;
}

static
bake_project_attr* bake_project_parseValue(
    bake_project *p,
    const char *package_id,
    bake_project_attr *existing,
    JSON_Value *v)
{
    bake_project_attr *attr = NULL;

    JSON_Value_Type t = json_value_get_type(v);
    switch(t) {
    case JSONArray:
        attr = bake_project_parseArray(p, package_id, existing, json_value_get_array(v));
        break;
    case JSONNull:
    case JSONString:
        attr = bake_project_parseString(p, package_id, json_value_get_string(v));
        break;
    case JSONNumber:
        attr = bake_project_parseNumber(json_value_get_number(v));
        break;
    case JSONBoolean:
        attr = bake_project_parseBoolean(json_value_get_boolean(v));
        break;
    case JSONObject:
        /* Ignore objects- placeholder */
        attr = corto_calloc(sizeof(bake_project_attr));
        attr->kind = BAKE_ATTR_BOOLEAN;
        break;
    default:
        break;
    }

    return attr;
}

static
int16_t bake_project_parseMembers(
    bake_project *p,
    const char *package_id,
    JSON_Object *jo)
{
    uint32_t i, count = json_object_get_count(jo);

    if (!p->attributes) {
        p->attributes = corto_ll_new();
    }

    for (i = 0; i < count; i ++) {
        bake_project_attr *attr = NULL;
        JSON_Value *v = json_object_get_value_at(jo, i);
        const char *json_name = json_object_get_name(jo, i);
        char *name = (char*)json_name;

        if (name[0] == '$') {
            /* If name contains function, parse it */
            name = bake_project_replace(p, NULL, json_name);
            if (!name) {
                goto error;
            }
        }

        if (!strcmp(name, "1") || !stricmp(name, "true")) {
            JSON_Value *value = json_object_get_value_at(jo, i);
            if (json_value_get_type(value) == JSONObject) {
                JSON_Object *obj = json_value_get_object(value);
                if (bake_project_parseMembers(p, NULL, obj)) {
                    goto error;
                }
            }
        } else

        if (!strcmp(name, "0") || !stricmp(name, "false")) {
            /* Ignore */
        } else {

            /* Check if attribute already exists */
            attr = bake_project_getattr(p, name);

            /* Add member to list of project attributes */
            attr = bake_project_parseValue(p, package_id, attr, v);
            if (attr) {
                attr->name = corto_strdup(name);
                corto_ll_append(p->attributes, attr);
            } else {
                corto_throw("failed to parse member '%s'", name);
                goto error;
            }

            /* When parsing dependee config, allow dynamically adding packages */
            if (!strcmp(name, "use")) {
                if (bake_append_json_array(p, package_id, v, bake_project_use)) {
                    corto_throw("expected array for 'use' attribute");
                    goto error;
                }
            }
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_project_parse_attributes(
    bake_project *p)
{
    if (p->value_json) {
        if (bake_project_parseMembers(p, p->id, p->value_json)) {
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_project_parse_config_value(
    bake_project *p,
    JSON_Object *jo)
{
    uint32_t i, count = json_object_get_count(jo);

    for (i = 0; i < count; i ++) {
        bake_project_attr *attr = NULL;
        JSON_Value *v = json_object_get_value_at(jo, i);
        const char *json_name = json_object_get_name(jo, i);
        char *name = (char*)json_name;

        if (name[0] == '$') {
            /* If name contains function, parse it */
            name = bake_project_replace(p, NULL, json_name);
            if (!name) {
                goto error;
            }
        }

        if (!strcmp(name, "1") || !stricmp(name, "true")) {
            JSON_Value *value = json_object_get_value_at(jo, i);
            if (json_value_get_type(value) == JSONObject) {
                JSON_Object *obj = json_value_get_object(value);
                if (bake_project_parse_config_value(p, obj)) {
                    goto error;
                }
            }
        } else

        if (!strcmp(name, "0") || !stricmp(name, "false")) {
            /* Ignore */
        } else

        if (!strcmp(name, "language")) {
            if (bake_assign_json_string(&p->language, v)) {
                corto_throw("expected string for 'language' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "c4cpp")) {
            if (json_value_get_type(v) == JSONBoolean) {
                p->c4cpp = json_value_get_boolean(v);
            } else {
                corto_throw("expected boolean for 'c4cpp' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "version")) {
            if (bake_assign_json_string(&p->version, v)) {
                corto_throw("expected string for 'version' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "managed")) {
            if (json_value_get_type(v) == JSONBoolean) {
                p->managed = json_value_get_boolean(v);
            } else {
                corto_throw("expected boolean for 'managed' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "public")) {
            if (json_value_get_type(v) == JSONBoolean) {
                p->public = json_value_get_boolean(v);
            } else {
                corto_throw("expected string for 'public' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "dependee")) {
            /* Dependee contain build instructions for dependee projects */
            p->dependee_json = corto_strdup(json_serialize_to_string(v));
        } else

        if (!strcmp(name, "use_generated_api")) {
            if (json_value_get_type(v) == JSONBoolean) {
                p->use_generated_api = json_value_get_boolean(v);
            } else {
                corto_throw("expected boolean for 'use_generated_api' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "link")) {
            if (bake_append_json_array(p, NULL, v, bake_project_link)) {
                corto_throw("expected array for 'link' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "use")) {
            if (bake_append_json_array(p, NULL, v, bake_project_use)) {
                corto_throw("expected array for 'use' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "use_private")) {
            if (bake_append_json_array(p, NULL, v, bake_project_use_private)) {
                corto_throw("expected array for 'use_private' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "sources")) {
            if (bake_append_json_array(p, NULL, v, bake_project_addSource)) {
                corto_throw("expected array for 'sources' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "includes")) {
            if (bake_append_json_array(p, NULL, v, bake_project_addInclude)) {
                corto_throw("expected array for 'includes' attribute");
                goto error;
            }
        } else

        if (!strcmp(name, "keep_binary")) {
            if (json_value_get_type(v) == JSONBoolean) {
                p->keep_binary = json_value_get_boolean(v);
            } else {
                corto_throw("expected boolean for 'keep_binary' attribute");
                goto error;
            }
        }

        if (name != json_name) {
            free(name);
        }
    }

    return 0;
error:
    return -1;
}

/* Parse project.json if available */
static
int16_t bake_project_parse_config(
    bake_project *p)
{
    const char *file = "project.json";

    if (corto_file_test("project.json")) {
        JSON_Value *j = json_parse_file(file);
        if (!j) {
            corto_throw("failed to parse '%s'", file);
            goto error;
        }

        p->json = j;

        JSON_Object *jo = json_value_get_object(j);
        if (!jo) {
            corto_throw("failed to parse '%s' (expected object)", file);
            goto error;
        }

        const char *j_id_member = json_object_get_string(jo, "id");
        if (!j_id_member) {
            corto_throw("failed to parse '%s': missing 'id' member", file);
            goto error;
        }

        p->id = strdup(j_id_member);
        char *ptr, ch;
        for (ptr = p->id; (ch = *ptr); ptr ++) {
            if (ch == '.') *ptr = '/';
        }

        const char *j_type_member = json_object_get_string(jo, "type");
        if (!j_type_member) {
            corto_throw("failed to parse '%s': missing 'type' member", file);
            goto error;
        }

        if (!strcmp(j_type_member, "application")) {
            p->kind = BAKE_APPLICATION;
        } else if (!strcmp(j_type_member, "package")) {
            p->kind = BAKE_PACKAGE;
        } else if (!strcmp(j_type_member, "tool")) {
            p->kind = BAKE_TOOL;
        } else if (!strcmp(j_type_member, "library")) {
            p->kind = BAKE_PACKAGE;
            p->managed = false;
        } else if (!strcmp(j_type_member, "executable")) {
            p->kind = BAKE_APPLICATION;
            p->managed = false;
        }

        JSON_Value *j_value_member = json_object_get_value(jo, "value");
        if (j_value_member) {
            JSON_Object *jvo = json_value_get_object(j_value_member);
            if (!jvo) {
                corto_throw(
                    "failed to parse '%s': value member must be an object",
                    file);
                goto error;
            }

            p->value_json = jvo;

            /* Parse configuration required to start building project */
            if (bake_project_parse_config_value(p, jvo)) {
                goto error;
            }
        } else {
            /* Project has no attributes, using default values */
        }
    } else {
        /* If there is no project.json, bake can likely not detect if the
         * project was rebuilt or not, so assuming yes. */
        p->freshly_baked = true;
    }

    return 0;
error:
    return -1;
}

static
void bake_project_clean_cb(const char *file) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_ll_append(p->files_to_clean, corto_strdup(file));
}

static
void bake_project_add_build_dependency_cb(const char *package) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_ll_append(p->use_build, corto_strdup(package));
}

static
char *bake_project_modelFile(
    bake_project *p)
{
    char *result = NULL;
    corto_iter it;

    if (corto_dir_iter(".", NULL, &it)) {
        p->error = true;
        goto error;
    }

    while (corto_iter_hasNext(&it)) {
        char *file = corto_iter_next(&it);
        if (!strncmp(file, "model.", strlen("model."))) {
            result = corto_strdup(file);

            /* Release iterator resources when prematurely quitting iteration */
            corto_iter_release(&it);
            break;
        }
    }

    return result;
error:
    return NULL;
}

int16_t bake_project_loadDependeeConfig(
    bake_project *p,
    const char *package_id,
    const char *file)
{
    JSON_Value *j = json_parse_file(file);
    if (!j) {
        corto_throw("failed to parse '%s'", file);
        goto error;
    }

    JSON_Object *jo = json_value_get_object(j);
    if (!jo) {
        corto_throw("failed to parse '%s' (expected object)", file);
        goto error;
    }

    if (bake_project_parseMembers(p, package_id, jo)) {
        corto_throw(NULL);
        goto error;
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_project_add_dependee_config(
    bake_project *p,
    const char *dep)
{
    /* Skip generated packages */
    if (bake_project_is_generated_package(p, dep)) {
        return 0;
    }

    const char *libpath = corto_locate(dep, NULL, CORTO_LOCATE_PACKAGE);
    if (!libpath) {
        corto_throw("failed to locate project path for dependency '%s'", dep);
        goto error;
    }

    /* Check if dependency has a dependee file with build instructions */
    char *dependee_file = corto_asprintf("%s/dependee.json", libpath);
    if (corto_file_test(dependee_file)) {
        if (bake_project_loadDependeeConfig(p, dep, dependee_file)) {
            corto_throw(NULL);
            goto error;
        }
    }

    free(dependee_file);

    return 0;
error:
    return -1;
}

int16_t bake_project_parse_dependee_attributes(
    bake_project *p)
{
    /* Add dependencies to link list */
    if (p->use) {
        corto_iter it = corto_ll_iter(p->use);
        while (corto_iter_hasNext(&it)) {
            char *dep = corto_iter_next(&it);
            if (bake_project_add_dependee_config(p, dep)) {
                goto error;
            }
        }
    }

    /* Add private dependencies to link list */
    if (p->use_private) {
        corto_iter it = corto_ll_iter(p->use_private);
        while (corto_iter_hasNext(&it)) {
            char *dep = corto_iter_next(&it);
            if (bake_project_add_dependee_config(p, dep)) {
                goto error;
            }
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_project_add_generated_dependencies(
    bake_project *p)
{
    if (!p->language) {
        /* If this package does not have a language, no dependencies to add */
        return 0;
    }

    /* For each package, if use_generated_api is enabled, also include
     * the package that contains the generated api for the language of
     * the package */
    if (p->use_generated_api && p->managed) {
        /* First, add own generated language package */
        if (p->model && p->public && p->kind == BAKE_PACKAGE) {
            bake_project_use(p, strarg("%s/%s", p->id, p->language));

            if (p->c4cpp) {
                bake_project_use(p, strarg("%s/cpp", p->id));
            }
        }

        int i = 0, count = corto_ll_count(p->use);
        corto_iter it = corto_ll_iter(p->use);
        while (corto_iter_hasNext(&it)) {
            char *package = corto_iter_next(&it);
            char *lastElem = strrchr(package, '/');
            i ++;
            if (lastElem) {
                lastElem ++;
                /* Don't try to find a language specific
                 * package if this is already a language
                 * specific package */
                if (!strcmp(lastElem, p->language)) {
                    continue;
                }
            }

            /* Insert language-specific package with generated
             * API if it exists */
            const char *lib = corto_locate(
                strarg("%s/%s", package, p->language),
                NULL,
                CORTO_LOCATE_LIB);
            if (lib) {
                bake_project_use(p, strarg("%s/%s", package, p->language));
            }

            /* Reached end of list- don't process packages that
             * we just added */
            if (i == count) {
                break;
            }
        }
    }

    return 0;
}

bool bake_project_is_generated_package(
    bake_project *p,
    const char *dependency)
{
    if (p->managed && p->language) {
        if (!strcmp(dependency, strarg("%s/c", p->id))) {
            return true;
        } else if (!strcmp(dependency, strarg("%s/cpp", p->id))) {
            return true;
        }
    }

    return false;
}

bake_project* bake_project_new(
    const char *path,
    bake_config *cfg)
{
    bake_project* result = corto_calloc(sizeof (bake_project));
    result->sources = corto_ll_new();
    result->includes = corto_ll_new();
    result->use = corto_ll_new();
    result->use_private = corto_ll_new();
    result->use_build = corto_ll_new();
    result->link = corto_ll_new();
    result->files_to_clean = corto_ll_new();
    result->cfg = cfg;

    /* Default values */
    result->path = path ? strdup(path) : NULL;
    result->public = true;
    result->managed = true;
    result->use_generated_api = true;
    result->language = corto_strdup("c");
    result->error = false;
    result->freshly_baked = false;
    result->artefact_outdated = false;
    result->sources_outdated = false;
    result->built = false;

    /* Set interface callbacks */
    result->get_attr = bake_project_getattr_cb;
    result->get_attr_string = bake_project_getattr_string_cb;
    result->get_attr_bool = bake_project_getattr_bool_cb;
    result->clean = bake_project_clean_cb;
    result->add_build_dependency = bake_project_add_build_dependency_cb;

    /* Parse project.json if available */
    if (bake_project_parse_config(result)) {
        corto_throw("failed to parse '%s/project.json'", path);
        goto error;
    }

    /* If 'src' and 'includes' weren't set, use defaults */
    if (!corto_ll_count(result->sources)) {
        corto_ll_append(result->sources, "src");
    }

    if (!corto_ll_count(result->includes)) {
        corto_ll_append(result->includes, "include");
    }

    if (result->language && result->managed) {
        /* Add extension package for model file */
        result->model = bake_project_modelFile(result);
        if (result->model) {
            char *ext = strrchr(result->model, '.');
            if (ext) {
                ext ++;
                corto_ll_append(result->use_build, corto_asprintf("driver/ext/%s", ext));
            }
        } else if (result->error) {
            goto error;
        }

        /* Managed projects need the code generator */
        corto_ll_append(result->use_build, corto_asprintf("driver/tool/pp"));

        /* Add corto as dependency to managed packages */
        bake_project_use(result, "corto");
    }

    if (result->use_generated_api && result->managed) {
        bake_project_use(result, "corto/c");
    }

    if (!result->version) {
        result->version = corto_strdup("0.0.0");
    }

    return result;
error:
    if (result) {
        if (result->path) free(result->path);
        free(result);
    }
    return NULL;
}

void bake_project_free(
    bake_project *p)
{
    if (p->path) free(p->path);
    if (p->use) corto_ll_free(p->use);
    if (p->use_private) corto_ll_free(p->use_private);
    if (p->sources) corto_ll_free(p->sources);
    if (p->includes) corto_ll_free(p->includes);
    if (p->files_to_clean) corto_ll_free(p->files_to_clean);
    free(p);
}

char* bake_project_binaryPath(
    bake_project *p)
{
    if (p->kind == BAKE_TOOL || p->kind == BAKE_APPLICATION) {
        return corto_strdup(p->cfg->binpath);
    } else {
        return corto_strdup(p->cfg->libpath);
    }
}

char* bake_project_includePath(
    bake_project *p)
{
    return corto_asprintf("%s/include", p->cfg->rootpath);
}

char* bake_project_etcPath(
    bake_project *p)
{
    return corto_asprintf("%s/etc", p->cfg->rootpath);
}

void bake_project_addSource(
    bake_project *p,
    const char *source)
{
    corto_iter it = corto_ll_iter(p->sources);
    while (corto_iter_hasNext(&it)) {
        char *project_source = corto_iter_next(&it);
        if (!strcmp(project_source, source)) {
            /* Duplicate */
            return;
        }
    }

    corto_ll_append(p->sources, corto_strdup(source));
}

void bake_project_addInclude(
    bake_project *p,
    const char *include)
{
    corto_iter it = corto_ll_iter(p->includes);
    while (corto_iter_hasNext(&it)) {
        char *project_include = corto_iter_next(&it);
        if (!strcmp(project_include, include)) {
            /* Duplicate */
            return;
        }
    }

    corto_ll_append(p->includes, corto_strdup(include));
}

void bake_project_use(
    bake_project *p,
    const char *use)
{
    corto_iter it = corto_ll_iter(p->use);
    while (corto_iter_hasNext(&it)) {
        char *project_use = corto_iter_next(&it);
        if (!strcmp(project_use, use)) {
            /* Duplicate */
            return;
        }
    }

    corto_ll_append(p->use, corto_strdup(use));
}

void bake_project_use_private(
    bake_project *p,
    const char *use)
{
    corto_iter it = corto_ll_iter(p->use_private);
    while (corto_iter_hasNext(&it)) {
        char *project_use = corto_iter_next(&it);
        if (!strcmp(project_use, use)) {
            /* Duplicate */
            return;
        }
    }

    corto_ll_append(p->use_private, corto_strdup(use));
}

static
char *bake_project_find_link_library(
    const char *name)
{
    char *result = NULL;

    /* If link points to hardcoded filename, return as is */
    if (corto_file_test(name)) {
        return corto_strdup(name);
    }

    /* If not found, check if provided name has an extension */
    char *ext = strrchr(name, '.');
    if (ext && !strchr(name, '/')) {
        /* Hardcoded filename was provided but not found */
        return NULL;
    }

    /* Try finding a library called lib*.so or lib*.dylib (OSX only) */
    char *full_path = strdup(name);
    char *lib_name = strrchr(full_path, '/');
    if (lib_name) {
        lib_name[0] = '\0';
        lib_name ++;
    } else {
        lib_name = full_path;
        full_path = NULL;
    }

    /* Try .so */
    if (full_path) {
        char *so = corto_asprintf("%s/lib%s.so", full_path, lib_name);
        if (corto_file_test(so)) {
            result = so;
        }
    } else {
        char *so = corto_asprintf("lib%s.so", lib_name);
        if (corto_file_test(so)) {
            result = so;
        }
    }

    /* Try .dylib */
    if (!result && !strcmp(CORTO_OS_STRING, "darwin")) {
        if (full_path) {
            char *dylib = corto_asprintf("%s/lib%s.dylib", full_path, lib_name);
            if (corto_file_test(dylib)) {
                result = dylib;
            }
        } else {
            char *dylib = corto_asprintf("lib%s.dylib", lib_name);
            if (corto_file_test(dylib)) {
                result = dylib;
            }
        }
    }

    /* Try .a */
    if (!result) {
        if (full_path) {
            char *a = corto_asprintf("%s/lib%s.a", full_path, lib_name);
            if (corto_file_test(a)) {
                result = a;
            }
        } else {
            char *a = corto_asprintf("lib%s.a", lib_name);
            if (corto_file_test(a)) {
                result = a;
            }
        }
    }

    if (full_path) {
        free(full_path);
    } else if (lib_name) {
        free(lib_name);
    }

    return result;
}

void bake_project_link(
    bake_project *p,
    const char *use)
{
    corto_iter it = corto_ll_iter(p->link);
    while (corto_iter_hasNext(&it)) {
        char *project_use = corto_iter_next(&it);
        if (!strcmp(project_use, use)) {
            /* Duplicate */
            return;
        }
    }

    corto_ll_append(p->link, corto_strdup(use));
}

int16_t bake_project_resolveLinks(
    bake_project *p)
{
    corto_ll resolved = corto_ll_new();
    corto_iter it = corto_ll_iter(p->link);
    while (corto_iter_hasNext(&it)) {
        char *project_use = corto_iter_next(&it);
        char *parsed = bake_project_replace(p, NULL, project_use);
        if (!parsed) {
            goto error;
        }
        char *lib = bake_project_find_link_library(parsed);
        if (!lib) {
            corto_throw("cannot find library '%s' in 'link' attribute", parsed);
            goto error;
        }
        corto_ll_append(resolved, lib);
    }

    bake_clean_string_array(p->link);
    p->link = resolved;

    return 0;
error:
    bake_clean_string_array(resolved);
    return -1;
}
