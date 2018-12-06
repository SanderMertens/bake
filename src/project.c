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

int16_t bake_project_load_value(
    bake_project *p,
    JSON_Object *jo)
{
    uint32_t i, count = json_object_get_count(jo);

    for (i = 0; i < count; i ++) {
        JSON_Value *v = json_object_get_value_at(jo, i);
        const char *member = json_object_get_name(jo, i);

        if (!strcmp(member, "public")) {
           ut_try (bake_json_set_boolean(&p->public, member, v), NULL);
        } else
        if (!strcmp(member, "version")) {
            ut_try (bake_json_set_string(&p->version, member, v), NULL);
        } else
        if (!strcmp(member, "repository")) {
            ut_try (bake_json_set_string(&p->repository, member, v), NULL);
        } else
        if (!strcmp(member, "use")) {
            ut_try (bake_json_set_array(&p->use, member, v), NULL);
        } else
        if (!strcmp(member, "use_private")) {
            ut_try (bake_json_set_array(&p->use_private, member, v), NULL);
        } else
        if (!strcmp(member, "link")) {
            ut_try (bake_json_set_array(&p->link, member, v), NULL);
        } else
        if (!strcmp(member, "sources")) {
            ut_try (bake_json_set_array(&p->sources, member, v), NULL);
        } else
        if (!strcmp(member, "includes")) {
            ut_try (bake_json_set_array(&p->includes, member, v), NULL);
        } else
        if (!strcmp(member, "keep_binary")) {
            ut_try (bake_json_set_boolean(&p->keep_binary, member, v), NULL);
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_project_set(
    bake_project *p,
    const char *id,
    const char *type)
{
    p->id = strdup(id);
    char *ptr, ch;
    for (ptr = p->id; (ch = *ptr); ptr ++) {
        if (ch == '.') {
            *ptr = '/';
        } else
        if (!isalpha(ch) && !isdigit(ch) &&
            ch != '_' && ch != '/' && ch != '-') {
            ut_throw("project id '%s' contains invalid characters", id);
            goto error;
        }
    }

    if (!strcmp(type, "application")) {
        p->type = BAKE_APPLICATION;
    } else if (!strcmp(type, "package")) {
        p->type = BAKE_PACKAGE;
    } else if (!strcmp(type, "tool")) {
        p->type = BAKE_TOOL;
    } else if (!strcmp(type, "executable")) {
        p->type = BAKE_APPLICATION;
        ut_warning("'executable' is deprecated, use 'application' instead");
    } else if (!strcmp(type, "library")) {
        p->type = BAKE_PACKAGE;
        ut_warning("'library' is deprecated, use 'package' instead");
    } else {
        ut_throw("project type '%s' is not valid", type);
        goto error;
    }

    return 0;
error:
    return -1;
}

int16_t bake_project_load(
    bake_project *project)
{
    char *file = ut_asprintf("%s/project.json", project->path);

    if (ut_file_test(file) == 1) {
        JSON_Value *j = json_parse_file(file);
        if (!j) {
            ut_throw("failed to parse '%s'", file);
            goto error;
        }

        JSON_Object *jo = json_value_get_object(j);
        if (!jo) {
            ut_throw("failed to parse '%s' (expected object)", file);
            goto error;
        }

        const char *j_id = json_object_get_string(jo, "id");
        if (!j_id) {
            ut_throw("failed to parse '%s': missing 'id'", file);
            goto error;
        }

        const char *j_type = json_object_get_string(jo, "type");
        if (!j_type) {
            j_type = "package";
        }

        ut_try (bake_project_set(project, j_id, j_type), NULL);

        JSON_Object *j_value = json_object_get_object(jo, "value");
        if (!j_value) {
            ut_try (bake_project_load_value(project, j_value), NULL);
        }
    } else {
        ut_throw("could not find file '%s'", file);
        goto error;
    }

    return 0;
error:
    return -1;
}

bake_project* bake_project_new(
    const char *path,
    bake_config *cfg)
{
    bake_project *result = ut_calloc(sizeof (bake_project));
    result->path = path ? strdup(path) : NULL;
    result->sources = ut_ll_new();
    result->includes = ut_ll_new();
    result->use = ut_ll_new();
    result->use_private = ut_ll_new();
    result->use_build = ut_ll_new();
    result->link = ut_ll_new();
    result->files_to_clean = ut_ll_new();

    /* Parse project.json if available */
    ut_try (
        bake_project_load(result), "failed to parse '%s/project.json'", path);

    /* If 'src' and 'includes' weren't set, use defaults */
    if (!ut_ll_count(result->sources)) {
        ut_ll_append(result->sources, "src");
    }
    if (!ut_ll_count(result->includes)) {
        ut_ll_append(result->includes, "include");
    }

    return result;
error:
    return NULL;
}

void bake_project_free(
    bake_project *p)
{

}

int16_t bake_project_init(
    bake_project *p)
{
    return 0;
}
