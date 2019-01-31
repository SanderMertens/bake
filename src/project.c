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

extern ut_tls BAKE_DRIVER_KEY;
extern ut_tls BAKE_FILELIST_KEY;
extern ut_tls BAKE_PROJECT_KEY;

/* Validate dependencies in use array */
static
int16_t bake_validate_dep_array(
    ut_ll array)
{
    ut_iter it = ut_ll_iter(array);
    while (ut_iter_hasNext(&it)) {
        char *dep = ut_iter_next(&it);

        char *ptr, ch;
        for (ptr = dep; (ch = *ptr); ptr ++) {
            if (ch == '/') {
                ut_warning("dependency '%s' contains deprecated '/' character",
                    dep);
                *ptr = '.';
            }
        }
    }

    return 0;
}

/* Parse JSON bake attributes in "value" member */
static
int16_t bake_project_parse_value(
    bake_config *config,
    bake_project *p,
    const char *project_id,
    JSON_Object *jo)
{
    uint32_t i, count = json_object_get_count(jo);
    bool error = false;

    for (i = 0; i < count; i ++) {
        JSON_Value *v = json_object_get_value_at(jo, i);
        char *member = (char*)json_object_get_name(jo, i);

        if (!strcmp(member, "public")) {
           ut_try (bake_json_set_boolean(&p->public, member, v), NULL);
        } else
        if (!strcmp(member, "author")) {
            ut_try (bake_json_set_string(&p->author, member, v), NULL);
        } else
        if (!strcmp(member, "organization")) {
            ut_try (bake_json_set_string(&p->organization, member, v), NULL);
        } else
        if (!strcmp(member, "description")) {
            ut_try (bake_json_set_string(&p->description, member, v), NULL);
        } else
        if (!strcmp(member, "version")) {
            ut_try (bake_json_set_string(&p->version, member, v), NULL);
        } else
        if (!strcmp(member, "repository")) {
            ut_try (bake_json_set_string(&p->repository, member, v), NULL);
        } else
        if (!strcmp(member, "license")) {
            ut_try (bake_json_set_string(&p->license, member, v), NULL);
        } else
        if (!strcmp(member, "language")) {
            ut_try (bake_json_set_string(&p->language, member, v), NULL);
        } else
        if (!strcmp(member, "use")) {
            ut_try (bake_json_set_array(&p->use, member, v), NULL);
            ut_try (bake_validate_dep_array(p->use), NULL);
        } else
        if (!strcmp(member, "use_private") || !strcmp(member, "use-private")) {
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
        if (!strcmp(member, "keep_binary") || !strcmp(member, "keep-binary")) {
            ut_try (bake_json_set_boolean(&p->keep_binary, member, v), NULL);
        } else {
            ut_warning("unknown member '%s' in project.json of '%s'", member, p->id);
        }
    }

    return 0;
error:
    return -1;
}

/* Set top-level attributes of project (id, type) */
static
int16_t bake_project_set(
    bake_project *p,
    const char *id,
    const char *type)
{
    p->id = ut_strdup(id);

    if (!strcmp(type, "application")) {
        p->type = BAKE_APPLICATION;
    } else if (!strcmp(type, "package")) {
        p->type = BAKE_PACKAGE;
    } else if (!strcmp(type, "tool")) {
        p->type = BAKE_TOOL;
    } else if (!strcmp(type, "template")) {
        p->type = BAKE_TEMPLATE;
    } else if (!strcmp(type, "executable")) {
        p->type = BAKE_APPLICATION;
        ut_warning("'executable' is deprecated, use 'application' instead for '%s'", p->id);
    } else if (!strcmp(type, "library")) {
        p->type = BAKE_PACKAGE;
        ut_warning("'library' is deprecated, use 'package' instead for '%s'", p->id);
    } else {
        ut_throw("project type '%s' is not valid for '%s'", type, p->id);
        goto error;
    }

    return 0;
error:
    return -1;
}

/* Parse project.json  */
static
int16_t bake_project_parse(
    bake_config *config,
    bake_project *project)
{
    char *file = ut_asprintf("%s/project.json", project->path);

    if (ut_file_test(file) == 1) {
        JSON_Value *j = json_parse_file_with_comments(file);
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
        if (j_value) {
            ut_try (bake_project_parse_value(config, project, project->id, j_value), NULL);
        }

        project->json = jo;
    } else {
        ut_throw("could not find file '%s'", file);
        goto error;
    }

    return 0;
error:
    return -1;
}

/* Find driver for project */
static
bake_project_driver* bake_project_get_driver(
    bake_project *project,
    const char *driver_id)
{
    ut_iter it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        if (!strcmp(driver->driver->id, driver_id)) {
            return driver;
        }
    }

    return NULL;
}

/* Load driver with JSON configuration */
static
int bake_project_load_driver(
    bake_project *project,
    const char *driver_id,
    JSON_Object *config)
{
    bake_project_driver *project_driver =
        bake_project_get_driver(project, driver_id);

    if (!project_driver) {
        bake_driver *driver = bake_driver_get(driver_id);
        if (!driver) {
            goto error;
        }

        project_driver = malloc(sizeof(bake_project_driver));
        project_driver->id = driver->id;
        project_driver->driver = driver;
        project_driver->json = NULL;
        project_driver->attributes = NULL;
        ut_ll_append(project->drivers, project_driver);
    }

    if (config) {
        project_driver->json = config;
    }

    return 0;
error:
    return -1;
}

/* Parse dependee configuration JSON object */
static
int16_t bake_project_load_dependee_object(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    JSON_Object *jo)
{
    uint32_t i, count = json_object_get_count(jo);

    for (i = 0; i < count; i ++) {
        const char *expr = json_object_get_name(jo, i);

        if (!strcmp(expr, "id") || !strcmp(expr, "type")) {
            /* Ignore */
            continue;
        }

        char *member = (char*)expr;
        member = bake_attr_replace(config, project, project_id, expr);
        if (!member) {
            goto error;
        }

        if (!strcmp(member, "1")) {
            JSON_Value *value = json_object_get_value_at(jo, i);
            if (json_value_get_type(value) == JSONObject) {
                JSON_Object *obj = json_value_get_object(value);
                ut_try (!bake_project_load_dependee_object(
                    config, project, project_id, obj), NULL);
            } else {
                ut_throw("JSON object expected for expression '%s'", expr);
                goto error;
            }
        } else if (!strcmp(member, "0")) {
            /* Ignore */
        } else {
            JSON_Value *value = json_object_get_value_at(jo, i);
            JSON_Object *obj = json_value_get_object(value);

            if (!strcmp(member, "value")) {
                ut_try (
                  bake_project_parse_value(config, project, project_id, obj),
                  NULL);
            } else {
                bake_project_driver *driver = bake_project_get_driver(
                    project, member);
                if (!driver) {
                    ut_try(
                      bake_project_load_driver(project, member, obj), NULL);
                } else {
                    driver->attributes = bake_attrs_parse(
                        config, project, project_id, obj, driver->attributes);
                    if (!driver->attributes) {
                        ut_throw(
                            "failed to parse dependee config for driver '%s'",
                            member);
                        goto error;
                    }
                }
            }
        }

        free(member);
    }

    return 0;
error:
    return -1;
}

/* Load dependee configuration for dependency */
static
int16_t bake_project_load_dependee_config(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    const char *file)
{
    JSON_Value *j = json_parse_file_with_comments(file);
    if (!j) {
        ut_throw("failed to parse '%s'", file);
        goto error;
    }

    JSON_Object *jo = json_value_get_object(j);
    if (!jo) {
        ut_throw("failed to parse '%s' (expected object)", file);
        goto error;
    }

    ut_try( bake_project_load_dependee_object(config, project, project_id, jo),
        NULL);

    return 0;
error:
    return -1;
}

/* Add dependee configuration to project configuration (if exists) */
static
int16_t bake_project_add_dependee_config(
    bake_config *config,
    bake_project *project,
    const char *dependency)
{
    ut_locate_reset(dependency);

    const char *libpath = ut_locate(dependency, NULL, UT_LOCATE_PROJECT);
    if (libpath) {
        /* Check if dependency has a dependee file with build instructions */
        char *file = ut_asprintf("%s/dependee.json", libpath);
        if (ut_file_test(file)) {
            ut_try (
              bake_project_load_dependee_config(config, project, dependency, file),
              NULL);
        }

        free(file);
    } else {
        /* If dependency cannot be found at this time, it is either a missing
         * dependency (in which case an error will be thrown) or it is a project
         * that is yet to be generated. In the latter case, ignore it for the
         * dependee configuration as project configuration needs to be stable at
         * this stage for bake's dependency resolver to work correctly */
    }

    return 0;
error:
    return -1;
}

/* Initialize language attribute and language driver */
static
int16_t bake_project_init_language(
    bake_config *config,
    bake_project *project)
{
    if (!project->language) {
        project->language = ut_strdup("c");
    }

    if (!project->version) {
        project->version = ut_strdup("0.0.0");
    }

    if (project->language && !strcmp(project->language, "c++")) {
        free(project->language);
        project->language = ut_strdup("cpp");
    }

    if (project->language && !strcmp(project->language, "none")) {
        free(project->language);
        project->language = NULL;
    }

    if (project->language) {
        char *driver_id = ut_asprintf("lang.%s", project->language);
        ut_try( bake_project_load_driver(
            project,
            driver_id,
            NULL),
              "failed to load driver for language '%s'", project->language);
        free(driver_id);

        bake_project_driver *driver = ut_ll_get(project->drivers, 0);
        project->language_driver = driver;

        /* Initialize configuration for language driver */
        project->language_driver->attributes = ut_ll_new();
        ut_try(
          bake_driver__init(project->language_driver->driver, config, project),
          NULL);
    }

    return 0;
error:
    return -1;
}

/* Initialize artefact name */
void bake_project_init_artefact(
    bake_config *config,
    bake_project *project)
{
    if (project->artefact) {
        project->artefact_path = ut_asprintf(
            "%s/bin/%s-%s", project->path, UT_PLATFORM_STRING,
            config->configuration);

        project->artefact_file = ut_asprintf(
            "%s/%s", project->artefact_path, project->artefact);
    }
}


/* -- Public API -- */

/* Initialize automatically derived project attributes */
int16_t bake_project_init(
    bake_config *config,
    bake_project *project)
{
    if (!isalpha(project->id[0])) {
        ut_error("project id '%s' is invalid (should start with a letter)",
            project->id);
        goto error;
    }

    project->id_underscore = ut_strdup(project->id);
    project->id_dash = ut_strdup(project->id);

    char *ptr;
    char ch;
    for (ptr = project->id; (ch = *ptr); ptr ++) {
        if (ch == '/') {
            ut_warning("project id '%s' contains deprecated '/' character",
                project->id);
            *ptr = '.';
            ch = '.';
        }

        if (ch == '.') {
            project->id_underscore[ptr - project->id] = '_';
            project->id_dash[ptr - project->id] = '-';

        } else if (!isalpha(ch) && !isdigit(ch) && ch != '_') {
            ut_error("project id '%s' contains invalid character ('%c')",
                project->id, ch);
            goto error;
        }
    }

    project->id_short = strrchr(project->id, '.');
    if (!project->id_short) {
        project->id_short = project->id;
    } else {
        project->id_short ++;
    }

    if (!project->sources) {
        project->sources = ut_ll_new();
    }
    if (!project->includes) {
        project->includes = ut_ll_new();
    }
    if (!project->drivers) {
        project->drivers = ut_ll_new();
    }
    if (!project->use) {
        project->use = ut_ll_new();
    }
    if (!project->use_private) {
        project->use_private = ut_ll_new();
    }
    if (!project->use_build) {
        project->use_build = ut_ll_new();
    }
    if (!project->link) {
        project->link = ut_ll_new();
    }
    if (!project->files_to_clean) {
        project->files_to_clean = ut_ll_new();
    }

    /* If 'src' and 'includes' weren't set, use defaults */
    if (!ut_ll_count(project->sources)) {
        ut_ll_append(project->sources, ut_strdup("src"));
    }
    if (!ut_ll_count(project->includes)) {
        ut_ll_append(project->includes, ut_strdup("include"));
    }

    ut_try (bake_project_init_language(config, project), NULL);

    /* Project artefact could have been provided manually on command line */
    if (project->artefact) {
        bake_project_init_artefact(config, project);
    }

    project->bin_path = ut_asprintf(
        "%s/bin", project->path);

    project->cache_path = ut_asprintf(
        "%s/.bake_cache", project->path);

    if (project->path[0] == '/') {
        project->fullpath = ut_strdup(project->path);
    } else if (strcmp(project->path, ".")) {
        project->fullpath = ut_asprintf("%s/%s", ut_cwd(), project->path);
    } else {
        project->fullpath = ut_strdup(ut_cwd());
    }

    return 0;
error:
    return -1;
}

/* Load all drivers for project */
static
int bake_project_load_drivers(
    bake_project *project)
{
    uint32_t i, count = json_object_get_count(project->json);
    bool error = false;

    for (i = 0; i < count; i ++) {
        const char *member = json_object_get_name(project->json, i);

        if (!strcmp(member, "id") || !strcmp(member, "type") ||
            !strcmp(member, "value"))
            continue;

        JSON_Value *value = json_object_get_value_at(project->json, i);
        JSON_Object *obj = json_value_get_object(value);

        if (strcmp(member, "dependee")) {
            ut_try( bake_project_load_driver(project, member, obj), NULL);
        } else {
            project->dependee_json = json_serialize_to_string(value);
        }
    }

    return 0;
error:
    return -1;
}

/* Create new project from path */
bake_project* bake_project_new(
    const char *path,
    bake_config *config)
{
    bake_project *result = ut_calloc(sizeof (bake_project));
    if (!path && !config) {
        return result;
    }

    result->path = path ? strdup(path) : NULL;
    result->public = true;

    /* Parse project.json if available */
    if (path) {
        ut_try (
            bake_project_parse(config, result),
            "failed to parse '%s/project.json'",
            path);

        ut_try( bake_project_init(config, result), NULL);
    }

    /* Load drivers */
    ut_try (bake_project_load_drivers(result), NULL);

    return result;
error:
    return NULL;
}

/* Free project resources */
void bake_project_free(
    bake_project *project)
{
    bake_attr_free_string_array(project->use);
    bake_attr_free_string_array(project->use_private);
    bake_attr_free_string_array(project->use_build);
    bake_attr_free_string_array(project->sources);
    bake_attr_free_string_array(project->includes);
    bake_attr_free_string_array(project->link);

    ut_iter it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        bake_attr_free_attr_array(driver->attributes);
    }

    free(project->id);
    free(project->id_underscore);
    free(project->id_dash);
}

/* Get attribute of project */
bake_attr* bake_project_get_attr(
    bake_project *project,
    const char *driver_id,
    const char *attr)
{
    bake_project_driver* driver = bake_project_get_driver(project, driver_id);
    if (driver && driver->attributes) {
        return bake_attr_get(driver->attributes, attr);
    } else {
        return NULL;
    }
}

/* Set attribute for project-specific driver configuration */
static
bake_attr* bake_project_set_attr(
    bake_config *config,
    bake_project *project,
    const char *driver_id,
    const char *attr,
    JSON_Value *value)
{
    bake_project_driver* driver = bake_project_get_driver(project, driver_id);

    if (driver) {
        ut_try( bake_attr_add(
          config, project, project->id, driver->attributes, attr, value), NULL);

        return bake_attr_get(driver->attributes, attr);
    } else {
        project->error = true;
        ut_error("failed to set attribute for unknown driver '%s'", driver_id);
    }

error:
    return NULL;
}

/* Set array attribute for project-specific driver configuration */
bake_attr* bake_project_set_attr_array(
    bake_config *config,
    bake_project *project,
    const char *driver_id,
    const char *name,
    const char *value)
{
    bake_attr *attr = bake_project_get_attr(project, driver_id, name);

    if (!attr) {
        attr = ut_calloc(sizeof(bake_attr));
        attr->name = ut_strdup(name);
        attr->kind = BAKE_ARRAY;
        attr->is.array = ut_ll_new();
    }

    if (attr->kind != BAKE_ARRAY) {
        ut_throw("attribute '%s' is not of type array", name);
        return NULL;
    }

    /* Check if value isn't already added */
    ut_iter it = ut_ll_iter(attr->is.array);
    while (ut_iter_hasNext(&it)) {
        const char *elem = ut_iter_next(&it);
        if (!strcmp(elem, value)) {
            return attr;
        }
    }

    ut_ll_append(attr->is.array, ut_strdup(value));

    return attr;
}

/* Set string attribute for project-specific driver configuration */
bake_attr* bake_project_set_attr_string(
    bake_config *config,
    bake_project *project,
    const char *driver_id,
    const char *attr,
    const char *value)
{
    return bake_project_set_attr(
        config, project, driver_id, attr, json_value_init_string(value));
}

/* Set bool attribute for project-specific driver configuration */
bake_attr* bake_project_set_attr_bool(
    bake_config *config,
    bake_project *project,
    const char *driver_id,
    const char *attr,
    bool value)
{
    return bake_project_set_attr(
        config, project, driver_id, attr, json_value_init_boolean(value));
}

/* Parse driver configuration (build cached list of attributes) */
int bake_project_parse_driver_config(
    bake_config *config,
    bake_project *project)
{
    ut_iter it = ut_ll_iter(project->drivers);

    /* Parse attributes for drivers in project configuration */
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        if (!driver->json) {
            continue;
        }

        driver->attributes = bake_attrs_parse(
            config, project, project->id, driver->json, NULL);
        if (!driver->attributes) {
            goto error;
        }
    }

    /* Now that all information is parsed, we can load the artefact name */
    if (project->language_driver && !project->artefact) {
        project->artefact = bake_driver__artefact(
            project->language_driver->driver, config, project);

        bake_project_init_artefact(config, project);
    }

    return 0;
error:
    return -1;
}

/* Initialize drivers for project */
int bake_project_init_drivers(
    bake_config *config,
    bake_project *project)
{
    ut_iter it = ut_ll_iter(project->drivers);

    /* Run driver initialization function */
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        ut_try(
            bake_driver__init(driver->driver, config, project),
            NULL);
    }

    return 0;
error:
    return -1;
}

/* Walk over all dependees, add dependee configuration */
int16_t bake_project_parse_dependee_config(
    bake_config *config,
    bake_project *project)
{
    /* Add dependencies to link list */
    if (project->use) {
        ut_iter it = ut_ll_iter(project->use);
        while (ut_iter_hasNext(&it)) {
            char *dep = ut_iter_next(&it);
            if (bake_project_add_dependee_config(config, project, dep)) {
                goto error;
            }
        }
    }

    /* Add private dependencies to link list */
    if (project->use_private) {
        ut_iter it = ut_ll_iter(project->use_private);
        while (ut_iter_hasNext(&it)) {
            char *dep = ut_iter_next(&it);
            if (bake_project_add_dependee_config(config, project, dep)) {
                goto error;
            }
        }
    }

    return 0;
error:
    return -1;
}

/* Check project dependency */
static
int16_t bake_check_dependency(
    bake_config *config,
    bake_project *p,
    const char *dependency,
    uint32_t artefact_modified,
    bool private)
{
    ut_locate_reset(dependency);
    const char *path = ut_locate(dependency, NULL, UT_LOCATE_PROJECT);
    bool dep_has_lib = false;

    if (path) {
        bake_project *dep = bake_project_new(path, config);
        if (dep) {
            if (dep->type != BAKE_PACKAGE) {
                ut_throw("invalid dependency '%s', not a package", dependency);
                goto error;
            }
            if (dep->language) {
                dep_has_lib = true;
            }
            bake_project_free(dep);
        } else {
            ut_throw("failed to create project for path '%s'", path);
            goto error;
        }
    }

    if (!dep_has_lib) {
        return 0;
    }

    const char *lib = ut_locate(dependency, NULL, UT_LOCATE_BIN);
    if (!lib) {
        ut_throw("binary for dependency '%s' not found", dependency);
        goto error;
    }

    time_t dep_modified = ut_lastmodified(lib);

    if (!artefact_modified || dep_modified <= artefact_modified) {
        const char *fmt = private
            ? "#[grey]use %s => %s (modified=%d private)"
            : "#[grey]use %s => %s (modified=%d)"
            ;
        ut_ok(fmt, dependency, lib, dep_modified);
    } else {
        p->artefact_outdated = true;
        const char *fmt = private
            ? "#[grey]use %s => %s (modified=%d, changed, private)"
            : "#[grey]use %s => %s (modified=%d, changed)"
            ;
        ut_ok(fmt, dependency, lib, dep_modified);
    }

proceed:
    return 0;
error:
    return -1;
}

/* Check project dependencies */
int16_t bake_project_check_dependencies(
    bake_config *config,
    bake_project *project)
{
    time_t artefact_modified = 0;

    if (!project->language) {
        return 0;
    }

    char *artefact_full = project->artefact_file;
    if  (ut_file_test(artefact_full)) {
        artefact_modified = ut_lastmodified(artefact_full);
    }

    if (project->use) {
        ut_iter it = ut_ll_iter(project->use);
        while (ut_iter_hasNext(&it)) {
            char *package = ut_iter_next(&it);
            if (bake_check_dependency(
                config, project, package, artefact_modified, false))
            {
                goto error;
            }
        }
    }

    if (project->use_private) {
        ut_iter it = ut_ll_iter(project->use_private);
        while (ut_iter_hasNext(&it)) {
            char *package = ut_iter_next(&it);
            if (bake_check_dependency(
                config, project, package, artefact_modified, true))
            {
                goto error;
            }
        }
    }

    if (project->artefact_outdated) {
        if (ut_rm(project->artefact_file)) {
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

/* Generate build step */
int16_t bake_project_generate(
    bake_config *config,
    bake_project *project)
{
    /* Invoke generate action for all drivers */
    ut_iter it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        ut_try( bake_driver__generate(driver->driver, config, project), NULL);
    }

    /* Evaluate GENERATED-SOURCES nodes if there are any */
    it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        bake_node *node = bake_node_find(driver->driver, "GENERATED-SOURCES");
        if (!node) {
            continue;
        }

        bake_driver *old_driver = ut_tls_get(BAKE_DRIVER_KEY);
        ut_tls_set(BAKE_DRIVER_KEY, driver->driver);
        bake_project *old_project = ut_tls_get(BAKE_PROJECT_KEY);
        ut_tls_set(BAKE_PROJECT_KEY, project);

        if (bake_node_eval(driver->driver, node, project, config, NULL, NULL)) {
            ut_throw("failed to build rule 'GENERATED-SOURCES'");
            goto error;
        }

        ut_tls_set(BAKE_DRIVER_KEY, old_driver);
        ut_tls_set(BAKE_PROJECT_KEY, old_project);
    }

    return 0;
error:
    return -1;
}

/* Resolve actual library paths for dependees */
static
int16_t bake_project_resolve_links(
    bake_config *config,
    bake_project *project)
{
    ut_ll resolved = ut_ll_new();
    ut_iter it = ut_ll_iter(project->link);

    while (ut_iter_hasNext(&it)) {
        char *link = ut_iter_next(&it);
        char *parsed = bake_attr_replace(config, project, NULL, link);
        if (!parsed) {
            goto error;
        }

        char *lib = bake_driver__link_to_lib(
            project->language_driver->driver, config, project, parsed);
        if (!lib) {
            ut_throw("cannot find library '%s' in 'link' attribute", parsed);
            goto error;
        }

        ut_ll_append(resolved, lib);
    }

    bake_attr_free_string_array(project->link);
    project->link = resolved;

    return 0;
error:
    bake_attr_free_string_array(resolved);
    return -1;
}

/* Cleanup resources */
static
void bake_project_link_cleanup(
    ut_ll link)
{
    ut_iter it = ut_ll_iter(link);
    while (ut_iter_hasNext(&it)) {
        char *link = ut_iter_next(&it);
        free(link);
    }
    ut_ll_free(link);
}

/* Copy libraries in link to bake environment */
static
ut_ll bake_project_copy_libs(
    bake_project *p,
    const char *path)
{
    ut_ll link_list = ut_ll_new();

    ut_iter it = ut_ll_iter(p->link);
    while (ut_iter_hasNext(&it)) {
        char *link = ut_iter_next(&it);

        char *link_lib = strrchr(link, '/');
        if (!link_lib) {
            link_lib = link;
        } else {
            link_lib ++;
        }

        if (!strncmp(link_lib, "lib", 3)) {
            link_lib += 3;
        }

        char *target_link = ut_asprintf("%s/lib%s_%s",
            path, p->id_underscore, link_lib);

        /* Copy to path */
        if (ut_cp(link, target_link)) {
            ut_throw("failed to library in link '%s'", link);
            goto error;
        }

        free(target_link);

        /* Create library name for linking */
        char *link_name = ut_asprintf("%s_%s", p->id_underscore, link_lib);

        /* Strip extension */
        char *ext = strrchr(link_name, '.');
        if (ext) {
            ext[0] = '\0';
        }

        ut_ll_append(link_list, link_name);
    }

    return link_list;
error:
    bake_project_link_cleanup(link_list);
    return NULL;
}

/* Add dependency to project */
static
int16_t bake_project_add_dependency(
    bake_project *p,
    const char *dep)
{
   /* Reset locate cache. It is possible that this dependency was looked up
    * before when it did not exist yet, but since then has been created (which
    * would have to be through a code generation process). */

    ut_locate_reset(dep);

    const char *libpath = ut_locate(dep, NULL, UT_LOCATE_PROJECT);
    if (!libpath) {
        ut_throw(
            "failed to locate library path for dependency '%s'", dep);
        goto error;
    }

    const char *lib = ut_locate(dep, NULL, UT_LOCATE_LIB);
    if (lib) {
        char *dep_lib = ut_strdup(dep);
        char *ptr, ch;
        for (ptr = dep_lib; (ch = *ptr); ptr ++) {
            if (ch == '/' || ch == '.') {
                ptr[0] = '_';
            }
        }

        ut_ll_append(p->link, dep_lib);
    } else {
        /* A dependency may not have a library that can be linked, but could
         * only contain build instructions */
        ut_catch();
    }

    return 0;
error:
    return -1;
}

/* Add dependencies to project */
static
int16_t bake_project_add_dependencies(
    bake_project *p)
{
    /* Add dependencies to link list */
    if (p->use) {
        ut_iter it = ut_ll_iter(p->use);
        while (ut_iter_hasNext(&it)) {
            char *dep = ut_iter_next(&it);
            if (bake_project_add_dependency(p, dep)) {
                goto error;
            }
        }
    }

    /* Add private dependencies to link list */
    if (p->use_private) {
        ut_iter it = ut_ll_iter(p->use_private);
        while (ut_iter_hasNext(&it)) {
            char *dep = ut_iter_next(&it);
            if (bake_project_add_dependency(p, dep)) {
                goto error;
            }
        }
    }

    return 0;
error:
    return -1;
}

/* Build artefact step */
static
int16_t bake_project_build_artefact(
    bake_config *config,
    bake_project *project,
    const char *artefact,
    const char *artefact_path)
{
    bake_driver *driver = project->language_driver->driver;
    bake_filelist *output = bake_filelist_new(NULL, NULL);

    /* Collect generated source files from drivers */
    ut_iter it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        bake_node *node = bake_node_find(driver->driver, "GENERATED-SOURCES");
        if (node) {
            bake_node_eval(driver->driver, node, project, config, NULL, output);
        }
    }

    /* Set generated_files list, so SOURCES can find it when its evaluated */
    project->generated_sources = output;

    ut_try (ut_mkdir(artefact_path), NULL);

    bake_driver *old_driver = ut_tls_get(BAKE_DRIVER_KEY);
    ut_tls_set(BAKE_DRIVER_KEY, driver);
    bake_project *old_project = ut_tls_get(BAKE_PROJECT_KEY);
    ut_tls_set(BAKE_PROJECT_KEY, project);

    /* Lookup artefact node, which must be the top level node */
    bake_node *root = bake_node_find(driver, "ARTEFACT");
    if (!root) {
        ut_throw("rule ARTEFACT not found in driver '%s'", driver->id);
        goto error;
    }

    /* Evaluate artefact node */
    bake_filelist *artefact_fl = bake_filelist_new(NULL, NULL);
    bake_filelist_add_file(artefact_fl, NULL, project->artefact_file);
    if (bake_node_eval(driver, root, project, config, artefact_fl, NULL)) {
        ut_throw("failed to build rule 'ARTEFACT'");
        goto error;
    }
    bake_filelist_free(artefact_fl);

    ut_tls_set(BAKE_DRIVER_KEY, old_driver);
    ut_tls_set(BAKE_PROJECT_KEY, old_project);

    return 0;
error:
    return -1;
}

/* Build build step */
int16_t bake_project_build(
    bake_config *config,
    bake_project *project)
{
    if (!project->artefact) {
        return 0;
    }

    /* If keep_binary is true and artefact exists, don't build even if inputs
     * are newer. This feature lets users control manually when a project should
     * be rebuilt, and is useful for projects that take a long time. */
    if (project->keep_binary && ut_file_test(project->artefact_file) == 1) {
        return 0;
    }

    /* Resolve libraries in project link attribute */
    if (bake_project_resolve_links(config, project)) {
        goto error;
    }

    /* Copy libraries to BAKE_TARGET, return list with local library names */
    ut_ll old_link = project->link;
    project->link = bake_project_copy_libs(project, config->target_lib);
    if (!project->link) {
        ut_throw(NULL);
        goto error;
    }

    /* Add use dependencies to the link attribute */
    if (bake_project_add_dependencies(project)) {
        goto error;
    }

    /* Run the top-level ARTEFACT rule */
    if (bake_project_build_artefact(
        config,
        project,
        project->artefact,
        project->artefact_path))
    {
        bake_project_link_cleanup(project->link);
        project->link = old_link;
        goto error;
    }

    /* Cleanup temporary list */
    bake_project_link_cleanup(project->link);

    /* Restore old list */
    project->link = old_link;

    return 0;
error:
    return -1;
}

/* Prebuild build step */
int16_t bake_project_prebuild(
    bake_config *config,
    bake_project *project)
{
    if (project->drivers) {
        ut_iter it = ut_ll_iter(project->drivers);
        while (ut_iter_hasNext(&it)) {
            bake_project_driver *driver = ut_iter_next(&it);
            ut_try(
              bake_driver__prebuild(driver->driver, config, project), NULL);
        }
    }

    return 0;
error:
    return -1;
}

/* Postbuild build step */
int16_t bake_project_postbuild(
    bake_config *config,
    bake_project *project)
{
    if (project->drivers) {
        ut_iter it = ut_ll_iter(project->drivers);
        while (ut_iter_hasNext(&it)) {
            bake_project_driver *driver = ut_iter_next(&it);
            ut_try(
              bake_driver__postbuild(driver->driver, config, project), NULL);
        }
    }

    return 0;
error:
    return -1;
}

/* Clean project, for all platforms or only the current platform */
int16_t bake_project_clean_intern(
    bake_config *config,
    bake_project *project,
    bool all_platforms)
{
    if (project->drivers) {
        ut_iter it = ut_ll_iter(project->drivers);
        while (ut_iter_hasNext(&it)) {
            bake_project_driver *driver = ut_iter_next(&it);
            ut_try( bake_driver__clean(driver->driver, config, project), NULL);
        }
    }

    ut_try( ut_rm(project->cache_path), NULL);

    if (!project->keep_binary) {
        if (all_platforms) {
            if (project->bin_path) {
                ut_try( ut_rm(project->bin_path), NULL);
            }
        } else {
            if (project->artefact_path) {
                ut_try( ut_rm(project->artefact_path), NULL);
            }
        }
    }

    if (project->files_to_clean) {
        ut_iter it = ut_ll_iter(project->files_to_clean);
        while (ut_iter_hasNext(&it)) {
            char *file = ut_iter_next(&it);
            ut_try(ut_rm(strarg("%s/%s", project->path, file)), NULL);
        }
    }

    project->changed = true;

    return 0;
error:
    return -1;
}

/* Clean build step */
int16_t bake_project_clean(
    bake_config *config,
    bake_project *project)
{
    return bake_project_clean_intern(config, project, true);
}

/* Only clean current platform */
int16_t bake_project_clean_current_platform(
    bake_config *config,
    bake_project *project)
{
    return bake_project_clean_intern(config, project, false);
}

/* Evaluate path against should_ignore paths of driver */
int16_t bake_project_should_ignore(
    bake_project *project,
    const char *file)
{
    if (project->drivers) {
        ut_iter it = ut_ll_iter(project->drivers);
        while (ut_iter_hasNext(&it)) {
            bake_project_driver *driver = ut_iter_next(&it);

            if (driver->driver->ignore_paths) {
                ut_iter path_it = ut_ll_iter(driver->driver->ignore_paths);
                while (ut_iter_hasNext(&path_it)) {
                    const char *path = ut_iter_next(&path_it);
                    if (!strcmp(path, file)) {
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

/* Generate random project description for when creating a new project */
static
char* bake_random_description(void) {
    char buffer[256];

    char *function[] = {
        "Car rentals",
        "Ride sharing",
        "Room sharing",
        "Vegan meal delivery",
        "Instant noodles",
        "Premium coffee beans",
        "Air conditioners",
        "Sunscreen",
        "Furniture",
        "A microwave",
        "Conditioner",
        "Grocery stores",
        "Photo sharing",
        "A social network",
        "Coding tutorials",
        "Bodybuilding",
        "Laser hair removal",
        "A movie theater subscription service",
        "Plant delivery",
        "Shaving supplies",
        "On demand country music",
        "A dating site",
        "Wine tasting",
        "Solar energy",
        "A towel",
        "A middle out compression algorithm",
        "Putting birds on ",
        "Winter is coming",
        "Space travel",
        "Lightsabers",
        "Hi-speed internet",
        "A web framework",
        "A fitness tracker"};

    char *subject[] = {
        "drones",
        "self driving cars",
        "air travel",
        "kids",
        "millenials",
        "coders",
        "project managers",
        "entrepreneurs",
        "influencers",
        "movie stars",
        "boy bands",
        "designers",
        "web developers",
        "venture capitalists",
        "sales reps",
        "married couples",
        "wookies",
        "dogs",
        "cats"};

    strcpy(buffer, function[rand() % (sizeof(function) / sizeof(char*))]);
    if (buffer[strlen(buffer) - 1] != ' ') {
        strcat(buffer, " for ");
    }
    strcat(buffer, subject[rand() % (sizeof(subject) / sizeof(char*))]);

    return strdup(buffer);
}

/* Convert project type into string */
const char* bake_project_type_str(
    bake_project_type type)
{
    switch (type) {
    case BAKE_APPLICATION: return "application";
    case BAKE_PACKAGE: return "package";
    case BAKE_TOOL: return "tool";
    case BAKE_TEMPLATE: return "template";
    }
    return "???";
}

/* Create project.json file for new project */
static
int16_t bake_project_create_project_json(
    bake_project *project)
{
    char *description = bake_random_description();

    /* Create project.json */
    FILE *f = fopen("project.json", "w");
    if (!f) {
        ut_throw("failed to open 'project.json");
        goto error;
    }

    fprintf(f,
        "{\n"
        "    \"id\": \"%s\",\n"
        "    \"type\": \"%s\",\n"
        "    \"value\": {\n"
        "        \"author\": \"John Doe\",\n"
        "        \"description\": \"%s\",\n"
        "        \"version\": \"0.0.0\",\n"
        "        \"repository\": null,\n"
        "        \"license\": null",
            project->id,
            bake_project_type_str(project->type),
            description);

    if (strcmp(project->language, "c")) {
        fprintf(f,
        ",\n        \"language\": \"%s\"", project->language);
    }

    fprintf(f, "\n    }\n}\n");

    fclose(f);

    return 0;
error:
    return -1;
}

/* Create .gitignore file for new project */
static
int16_t bake_project_create_gitignore(
    bake_project *project)
{
    FILE *f = fopen(".gitignore", "w");
    if (!f) {
        ut_throw("failed to open '.gitignore'");
        goto error;
    }

    fprintf(f, ".bake_cache");
    fprintf(f, ".DS_Store");
    fprintf(f, ".vscode");
    fprintf(f, "bin");

    fclose(f);

    return 0;
error:
    return -1;
}

/* Setup a new project */
int16_t bake_project_setup(
    bake_config *config,
    bake_project *project)
{
    bake_driver *driver = project->language_driver->driver;

    ut_try(!
        ut_proc_runRedirect("git", (const char*[]){"git", "init", NULL}, stdin, stdout, stderr),
        "failed to initialize git repository");

    ut_try( bake_project_create_project_json(project), NULL);

    ut_try( bake_project_create_gitignore(project), NULL);

    if (driver) {
        ut_try( bake_driver__setup(driver, config, project), NULL);
    } else {
        ut_throw(
            "cannot init: project '%s' does not configure a language",
            project->id);
        goto error;
    }

    ut_log("Created new %s with id '%s'\n",
        project->type == BAKE_APPLICATION
            ? "application"
            : "package",
        project->id);

    return 0;
error:
    return -1;
}

/* Copy file from template to new project, do template substitutions */
static
int16_t bake_project_file_from_template(
    bake_config *config,
    bake_project *project,
    const char *src_file,
    const char *dst_file)
{
    char *src_content = ut_file_load(src_file);
    if (!src_content) {
        ut_throw("failed to load file '%s'", src_file);
        goto error;
    }

    FILE *dst = fopen(dst_file, "w");
    if (!dst) {
        ut_throw("failed to open file '%s' for writing", dst_file);
        goto error;
    }

    char *dst_content = bake_attr_replace(
            config, project, project->id, src_content);

    fprintf(dst, "%s", dst_content);
    fclose(dst);

    free(dst_content);
    free(src_content);

    return 0;
error:
    return -1;
}

/* Determine whether file is a file that needs substitution */
static const char *template_file[] = {
    "Makefile"
};

static const char *template_ext[] = {
    "", "c", "cpp", "h", "hpp", "html", "js", "css", "json", "md", "sh", "bat",
    "lua", "python", "java", "cs", "make"
};

static
bool bake_is_template_file(
    const char *file)
{
    int i;
    for (i = 0; i < sizeof(template_file) / sizeof(char*); i ++) {
        if (!strcmp(template_file[i], file)) {
            return true;
        }
    }

    char *ext = strrchr(file, '.');

    if (!ext) {
        ext = "";
    } else {
        ext ++;
    }

    for (i = 0; i < sizeof(template_ext) / sizeof(char*); i ++) {
        if (!strcmp(template_ext[i], ext)) {
            return true;
        }
    }

    return false;
}

static
char *bake_project_template_filename(
    bake_config *config,
    bake_project *project,
    char *file)
{
    char *real = strrchr(file, '/');
    if (!real) {
        real = file;
    } else {
        real ++;
    }

    if (real[0] == '_' && real[1] == '_') {
        char *ext = strrchr(real, '.');
        char *path = NULL;

        if (!ext) {
            ext = "";
        }

        if (real != file) {
            path = ut_strdup(file);
            char *last_elem = strrchr(path, '/');
            last_elem[1] = '\0';
        } else {
            path = "";
        }

        real += 2; /* Strip __ */

        char *tmp = ut_asprintf("%s${%s%s", path, real, ext);
        char *ptr, ch;
        for (ptr = tmp; (ch = *ptr); ptr ++) {
            if (ch == '_') {
                ptr[0] = ' ';
            }
            if (ch == '.') {
                ptr[0] = '}';
                strcpy(ptr + 1, ext);
                break;
            }
        }
        
        real = bake_attr_replace(config, project, project->id, tmp);
        free(tmp);

    } else {
        real = file;
    }

    return real;
}

/* Setup a new project from a template */
int16_t bake_project_setup_w_template(
    bake_config *config,
    bake_project *project,
    const char *template_id)
{
    const char *template_path = ut_locate(
            template_id, NULL, UT_LOCATE_TEMPLATE);
    if (!template_path) {
        ut_throw("template '%s' not found", template_id);
        goto error;
    }

    ut_iter it;
    ut_try( ut_dir_iter(template_path, "//", &it), NULL);

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        if (file[0] == '.') {
            continue;
        }

        if (!strcmp(file, "project.json")) {
            continue;
        }

        bool is_template = bake_is_template_file(file);
        char *src_path = ut_asprintf("%s/%s", template_path, file);

        char *real = file;
        
        if (is_template) {
            real = bake_project_template_filename(config, project, file);
        }

        char *dst_path = ut_asprintf("%s/%s", project->path, real);

        if (!ut_isdir(src_path)) {
            if (is_template) {
                ut_try( bake_project_file_from_template(
                    config, project, src_path, dst_path),
                    NULL);
            } else {
                ut_try( ut_cp(src_path, dst_path), NULL);
            }
        } else {
            ut_try( ut_mkdir(dst_path), NULL);
        }

        free(dst_path);
        free(src_path);
    }

    /* Load JSON from template, tailor to project */
    char *template_json = ut_asprintf("%s/project.json", template_path);
    JSON_Value *value = json_parse_file_with_comments(template_json);
    if (!value) {
        ut_throw("failed to load project.json of template '%s", template_id);
        goto error;
    }

    /* Tailor JSON to use project specific settings */
    JSON_Object *root = json_value_get_object(value);
    if (!root) {
        ut_throw("expected JSON in project.json of template '%s'", template_id);
        goto error;
    }

    /* Overwrite the name and type of the JSON */
    json_object_set_string(root, "id", project->id);
    json_object_set_string(root, "type", bake_project_type_str(project->type));

    /* Write to project.json of project */
    char *project_json = ut_asprintf("%s/project.json", project->path);
    json_serialize_to_file_pretty(value, project_json);
    free(project_json);

    ut_try( bake_project_create_gitignore(project), NULL);

    return 0;
error:
    return -1;
}
