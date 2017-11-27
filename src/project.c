/* Copyright (c) 2010-2017 the corto developers
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
    JSON_Value *v);

static
bake_project_attr* bake_project_parseArray(
    JSON_Array *a)
{
    uint32_t i, count = json_array_get_count(a);
    bake_project_attr *result = corto_calloc(sizeof(bake_project_attr));
    result->kind = BAKE_ATTR_ARRAY;
    result->is.array = corto_ll_new();

    for (i = 0; i < count; i ++) {
        JSON_Value *v = json_array_get_value(a, i);
        bake_project_attr *attr = bake_project_parseValue(v);
        if (attr) {
            corto_ll_append(result->is.array, attr);
        }
    }

    return result;
}

static
bake_project_attr* bake_project_parseString(
    const char *str)
{
    bake_project_attr *result = corto_calloc(sizeof(bake_project_attr));
    result->kind = BAKE_ATTR_STRING;

    if (str) {
        result->is.string = corto_strdup(str);
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
    JSON_Value *v)
{
    bake_project_attr *attr = NULL;

    JSON_Value_Type t = json_value_get_type(v);
    switch(t) {
    case JSONArray:
        attr = bake_project_parseArray(json_value_get_array(v));
        break;
    case JSONString:
        attr = bake_project_parseString(json_value_get_string(v));
        break;
    case JSONNumber:
        attr = bake_project_parseNumber(json_value_get_number(v));
        break;
    case JSONBoolean:
        attr = bake_project_parseBoolean(json_value_get_boolean(v));
        break;
    case JSONObject: break; /* Ignore objects */
    default:
        break;
    }

    return attr;
}

static
int16_t bake_project_parseMembers(
    bake_project *p,
    JSON_Object *jo)
{
    uint32_t i, count = json_object_get_count(jo);

    p->attributes = corto_ll_new();

    for (i = 0; i < count; i ++) {
        bake_project_attr *attr = NULL;
        JSON_Value *v = json_object_get_value_at(jo, i);
        const char *name = json_object_get_name(jo, i);
        attr = bake_project_parseValue(v);
        if (attr) {
            attr->name = corto_strdup(name);
            corto_ll_append(p->attributes, attr);

            /* Parse generic project attributes */
            if (!strcmp(name, "language")) {
                if (json_value_get_type(v) == JSONString) {
                    if (p->language) free(p->language);
                    p->language = strdup(json_value_get_string(v));
                } else {
                    corto_throw("expected JSON string for 'language' attribute");
                    goto error;
                }
            }

            if (!strcmp(name, "version")) {
                if (json_value_get_type(v) == JSONString) {
                    if (p->version) free(p->version);
                    p->version = strdup(json_value_get_string(v));
                } else {
                    corto_throw("expected JSON string for 'version' attribute");
                    goto error;
                }
            }

            if (!strcmp(name, "managed")) {
                if (json_value_get_type(v) == JSONBoolean) {
                    p->managed = json_value_get_boolean(v);
                } else {
                    corto_throw("expected JSON boolean for 'managed' attribute");
                    goto error;
                }
            }

            if (!strcmp(name, "public")) {
                if (json_value_get_type(v) == JSONBoolean) {
                    p->public = json_value_get_boolean(v);
                } else {
                    corto_throw("expected JSON string for 'public' attribute");
                    goto error;
                }
            }

            if (!strcmp(name, "sources")) {
                if (json_value_get_type(v) == JSONArray) {
                    JSON_Array *a = json_value_get_array(v);
                    uint32_t i, count = json_array_get_count(a);
                    for (i = 0; i < count; i ++) {
                        JSON_Value *v = json_array_get_value(a, i);
                        const char *src = json_value_get_string(v);
                        bake_project_addSource(p, src);
                    }
                }
            }

            if (!strcmp(name, "includes")) {
                if (json_value_get_type(v) == JSONArray) {
                    JSON_Array *a = json_value_get_array(v);
                    uint32_t i, count = json_array_get_count(a);
                    for (i = 0; i < count; i ++) {
                        JSON_Value *v = json_array_get_value(a, i);
                        const char *src = json_value_get_string(v);
                        bake_project_addInclude(p, src);
                    }
                }
            }

            if (!strcmp(name, "use")) {
                if (json_value_get_type(v) == JSONArray) {
                    JSON_Array *a = json_value_get_array(v);
                    uint32_t i, count = json_array_get_count(a);
                    for (i = 0; i < count; i ++) {
                        JSON_Value *v = json_array_get_value(a, i);
                        const char *use = json_value_get_string(v);
                        bake_project_use(p, use);
                    }
                }
            }

            if (!strcmp(name, "use_generated_api")) {
                if (json_value_get_type(v) == JSONBoolean) {
                    p->use_generated_api = json_value_get_boolean(v);
                } else {
                    corto_throw("expected JSON boolean for 'managed' attribute");
                    goto error;
                }
            }
        }
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_project_parseConfig(
    bake_project *p)
{
    const char *file = "project.json";

    if (corto_file_test("project.json")) {
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

        const char *j_id_member = json_object_get_string(jo, "id");
        if (!j_id_member) {
            corto_throw("failed to parse '%s': missing 'id' member", file);
            goto error;
        }

        p->id = strdup(j_id_member);

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
        }

        JSON_Value *j_value_member = json_object_get_value(jo, "value");
        if (j_value_member) {
            JSON_Object *jvo = json_value_get_object(j_value_member);
            if (!jvo) {
                corto_throw("failed to parse '%s': value member must be an object", file);
                goto error;
            }

            if (bake_project_parseMembers(p, jvo)) {
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

char *bake_project_getattr_tostr(bake_project_attr *result) {
    switch(result->kind) {
    case BAKE_ATTR_STRING:
        return corto_strdup(result->is.string);
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

char *bake_project_getattr_string_cb(const char *name) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_assert(p != NULL, "project::getattr called without project context");

    bake_project_attr *result = bake_project_getattr(p, name);

    if (result) {
        return bake_project_getattr_tostr(result);
    } else {
        return NULL;
    }
}

bake_project_attr* bake_project_getattr_cb(const char *name) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_assert(p != NULL, "project::getattr called without project context");

    return bake_project_getattr(p, name);
}

void bake_project_clean_cb(const char *file) {
    bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
    corto_ll_append(p->files_to_clean, corto_strdup(file));
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

bake_project* bake_project_new(
    const char *path)
{
    bake_project* result = corto_calloc(sizeof (bake_project));
    result->sources = corto_ll_new();
    result->includes = corto_ll_new();
    result->use = corto_ll_new();
    result->use_build = corto_ll_new();
    result->link = corto_ll_new();
    result->files_to_clean = corto_ll_new();

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
    corto_ll_append(result->includes, "include");
    corto_ll_append(result->sources, "src");

    /* Set interface callbacks */
    result->get_attr = bake_project_getattr_cb;
    result->get_attr_string = bake_project_getattr_string_cb;
    result->clean = bake_project_clean_cb;

    /* Parse project.json if available */
    if (bake_project_parseConfig(result)) {
        goto error;
    }

    /* Add packages required for building */
    if (result->managed) {
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

        /* Add corto as dependency to managed packages */
        corto_ll_append(
            result->use,
            corto_strdup("corto"));

        /* Add generator packages for language binding */
        /*corto_ll_append(
            result->use_build,
            corto_asprintf("driver/gen/%s",
                result->language));*/

    }

    if (result->use_generated_api) {
        if (result->managed) {
            corto_ll_append(result->use, strdup("corto/c"));
        }
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
    if (p->sources) corto_ll_free(p->sources);
    if (p->includes) corto_ll_free(p->includes);
    if (p->files_to_clean) corto_ll_free(p->files_to_clean);
    free(p);
}

char* bake_project_binaryPath(
    bake_project *p)
{
    char *kind, *subdir;
    if (p->kind == BAKE_APPLICATION) {
        kind = "bin";
        subdir = "cortobin";
    } else if (p->kind == BAKE_PACKAGE) {
        kind = "lib";
        subdir = "corto";
    } else if (p->kind == BAKE_TOOL) {
        return corto_envparse(
            "$CORTO_TARGET/bin");
    } else {
        corto_throw("unsupported project kind '%s'", p->kind);
        return NULL;
    }

    return corto_envparse(
        "$CORTO_TARGET/%s/%s/$CORTO_VERSION/%s",
        kind,
        subdir,
        p->id);
}

char* bake_project_includePath(
    bake_project *p)
{
    return corto_envparse(
        "$CORTO_TARGET/include/corto/$CORTO_VERSION/%s",
        p->id);
}

char* bake_project_etcPath(
    bake_project *p)
{
    return corto_envparse(
        "$CORTO_TARGET/etc/corto/$CORTO_VERSION/%s",
        p->id);
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
