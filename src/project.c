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
extern ut_tls BAKE_FILELIST_KEY;
extern ut_tls BAKE_PROJECT_KEY;

static
int rb_strcmp(
    void *ctx,
    const void* key1,
    const void* key2)
{
    return strcmp(key1, key2);
}

/* Parse ref in 'refs' section */
static
int16_t bake_project_parse_ref(
    bake_project *p,
    bake_project_bundle *bundle,
    const char *id,
    JSON_Object *o)
{
    if (!bundle->refs) {
        bundle->refs = ut_rb_new(rb_strcmp, NULL);
    } else {
        if (ut_rb_find(bundle->refs, id)) {
            ut_throw("ref '%s' redefined", id);
            goto error;
        }
    }

    bake_ref *ref = ut_calloc(sizeof(bake_project_bundle));

    /* Find corresponding repository */
    bake_project_repository *repo = ut_rb_find(p->repositories, id);
    if (!repo) {
        ut_throw("referenced project '%s' that is not in 'repository' list",
            id);
        goto error;
    }

    ref->repository = repo;

    uint32_t i, count = json_object_get_count(o);
    for (i = 0; i < count; i ++) {
        char *member = (char*)json_object_get_name(o, i);
        JSON_Value *v = json_object_get_value_at(o, i);

        if (!strcmp(member, "tag")) {
            ut_try( bake_json_set_string(&ref->tag, member, v), NULL);

        } else if (!strcmp(member, "commit")) {
            ut_try( bake_json_set_string(&ref->commit, member, v), NULL);

        } else if (!strcmp(member, "branch")) {
            ut_try( bake_json_set_string(&ref->branch, member, v), NULL);
        
        } else {
            ut_warning("ignoring unkown property '%s'", member);
        }
    }

    if (!ref->tag && !ref->commit) {
        if (bundle->defaults.tag) {
            ref->tag = ut_strdup(bundle->defaults.tag);
        }
    }

    if (!ref->branch) {
        if (bundle->defaults.branch) {
            ref->branch = ut_strdup(bundle->defaults.branch);
        }
    }

    ut_rb_set(bundle->refs, id, ref);

    return 0;
error:
    return -1;
}

/* Parse bundle in 'refs' section */
static
int16_t bake_project_parse_bundle(
    bake_project *p,
    const char *id,
    JSON_Object *o)
{
    bake_project_bundle *bundle = ut_calloc(sizeof(bake_project_bundle));

    uint32_t i, count = json_object_get_count(o);
    for (i = 0; i < count; i ++) {
        char *member = (char*)json_object_get_name(o, i);
        JSON_Value *v = json_object_get_value_at(o, i);

        if (!strcmp(member, "default-tag")) {
            ut_try( bake_json_set_string(&bundle->defaults.tag, member, v), NULL);

        } else if (!strcmp(member, "default-branch")) {
            ut_try( bake_json_set_string(&bundle->defaults.branch, member, v), NULL);
        
        } else {
            JSON_Object *o = json_object(v);
            if (!o) {
                ut_throw("invalid value for ref '%s', expected object",
                    member);
                goto error;
            }

            ut_try( bake_project_parse_ref(p, bundle, member, o), NULL);
        }
    }

    if (!p->bundles) {
        p->bundles = ut_rb_new(rb_strcmp, NULL);
    }

    ut_rb_set(p->bundles, id, bundle);        

    return 0;
error:
    return -1;
}

/* Parse 'refs' in 'bundle' section */
static
int16_t bake_project_parse_refs(
    bake_project *p,
    JSON_Object *o)
{
    uint32_t i, count = json_object_get_count(o);
    for (i = 0; i < count; i ++) {
        char *member = (char*)json_object_get_name(o, i);
        JSON_Value *v = json_object_get_value_at(o, i);
        JSON_Object *o = json_object(v);
        if (!o) {
            ut_throw("invalid value for '%s', expected object");
            goto error;
        }

        ut_try( bake_project_parse_bundle(p, member, o), NULL);
    }

    return 0;
error:
    return -1;
}

/* Parse repository in 'repositories' section */
static
int16_t bake_project_parse_repository(
    bake_project *p,
    const char *project,
    const char *url)
{
    if (!project) {
        ut_throw("invalid project identifier in bundle repositories");
        goto error;
    }

    if (!url) {
        ut_throw("invalid repository for bundle project '%s'", project);
        goto error;
    }

    bake_project_repository *repo = ut_calloc(sizeof(bake_repository));
    repo->id = ut_strdup(project);

    /* If URL is not complete, append either the default host, or if
        * none is specified, prefix with https://github.com */
    bool has_protocol, has_url;
    ut_try( bake_url_is_well_formed(url, &has_protocol, &has_url), NULL);
    if (has_protocol && has_url) {
        repo->url = ut_strdup(url);
    } else {
        if (p->default_host) {
            repo->url = ut_asprintf("%s/%s", p->default_host, url);
        } else {
            repo->url = ut_asprintf("https://github.com/%s", url);
        }
    }

    if (!p->repositories) {
        p->repositories = ut_rb_new(rb_strcmp, NULL);
    } else {
        if (ut_rb_find(p->repositories, repo->id)) {
            ut_throw("repository '%s' redefined", repo->id);
            goto error;
        }
    }

    ut_rb_set(p->repositories, repo->id, repo);

    return 0;
error:
    return -1;
}

/* Parse 'repositories' section in bundle */
static
int16_t bake_project_parse_repositories(
    bake_project *p,
    JSON_Object *o)
{
    uint32_t i, count = json_object_get_count(o);
    for (i = 0; i < count; i ++) {
        JSON_Value *v = json_object_get_value_at(o, i);
        const char *project = (char*)json_object_get_name(o, i);
        const char *repo = json_string(v);
        if (!repo) {
            ut_throw(
                "invalid value for project '%s': expected repository string",
                project);
            goto error;
        }

        ut_try( bake_project_parse_repository(p, project, repo), NULL);
    }

    return 0;
error:
    return -1;
}

/* Parse 'use' section in bundle */
static
int16_t bake_project_parse_use_bundles(
    bake_project *p,
    JSON_Array *a)
{
    uint32_t i, count = json_array_get_count(a);
    for (i = 0; i < count; i ++) {
        const char *bundle = json_array_get_string(a, i);
        if (!bundle) {
            ut_throw(
                "invalid value for element in 'use' array: expected string");
            goto error;
        }

        if (!p->use_bundle) {
            p->use_bundle = ut_ll_new();
        }

        ut_ll_append(p->use_bundle, ut_strdup(bundle));
    }

    return 0;
error:
    return -1;
}

/* Parse 'bundle' section in project file */
static
int16_t bake_project_parse_bundles(
    bake_project *p,
    JSON_Object *jo)
{
    /* Parse default-host first, before parsing repositories */
    JSON_Value *default_host_value = json_object_get_value(jo, "default-host");
    if (default_host_value) {
        ut_try( bake_json_set_string(
            &p->default_host, "default-host", default_host_value), NULL);
    }

    /* Parse repositories before parsing repository references, so they can be
     * added to the configuration file in any order. */
    JSON_Value *repositories_value = json_object_get_value(jo, "repositories");
    if (repositories_value) {
        JSON_Object *o = json_object(repositories_value);
        if (!o) {
            ut_throw("invalid value for 'repositories', expected object");
            goto error;                
        }

        ut_try( bake_project_parse_repositories(p, o), NULL);
    }

    /* Parse refs. These contain specific commit/tag refs to repositories. If
     * no refs are provided, the provided bundle id (e.g. "v1.0") will be used
     * as tag instead */
    JSON_Value *refs_value = json_object_get_value(jo, "refs");
    if (refs_value) {
        JSON_Object *o = json_object(refs_value);
        if (!o) {
            ut_throw("invalid value for 'refs', expected object");
            goto error;
        }

        ut_try( bake_project_parse_refs(p, o), NULL);
    }

    /* Check for invalid properties */
    uint32_t i, count = json_object_get_count(jo);
    for (i = 0; i < count; i ++) {
        char *member = (char*)json_object_get_name(jo, i);
        JSON_Value *v = json_object_get_value_at(jo, i);

        if (strcmp(member, "default-host") && 
            strcmp(member, "repositories") && 
            strcmp(member, "refs"))
        {
            ut_warning("ignoring unknown property '%s'", member);
        }
    }

    return 0;
error:
    return -1;
}

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
        if (!strcmp(member, "coverage")) {
           ut_try (bake_json_set_boolean(&p->coverage, member, v), NULL);
        } else   
        if (!strcmp(member, "amalgamate")) {
           ut_try (bake_json_set_boolean(&p->amalgamate, member, v), NULL);
        } else
        if (!strcmp(member, "amalgamate_path") || !strcmp(member, "amalgamate-path")) {
           ut_try (bake_json_set_string(&p->amalgamate_path, member, v), NULL);
        } else
        if (!strcmp(member, "standalone") || !strcmp(member, "standalone")) {
           ut_try (bake_json_set_boolean(&p->standalone, member, v), NULL);
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

            /* Register repository for project */
            ut_try( bake_add_repository(
                config, 
                p->id, 
                p->repository,
                NULL,
                NULL,
                NULL,
                p->id,
                NULL), NULL);
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
        if (!strcmp(member, "use_runtime") || !strcmp(member, "use-runtime")) {
            ut_try (bake_json_set_array(&p->use_runtime, member, v), NULL);
        } else        
        if (!strcmp(member, "use_bundle") || !strcmp(member, "use-bundle")) {
            ut_try (bake_json_set_array(&p->use_bundle, member, v), NULL);
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
        } else
        if (!strcmp(member, "clangd")) {
            ut_try (bake_json_set_boolean(&p->clangd, member, v), NULL);
        } else {
            ut_warning("unknown member '%s' in project.json of '%s'", member, p->id);
        }
    }

    if (p->standalone) {
        /* Standalone projects can't be public because their include files might
         * refer to dependencies that are not copied to the bake environment. */
        p->public = false;
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
    char *file = ut_asprintf("%s"UT_OS_PS"project.json", project->path);

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

        project_driver = ut_calloc(sizeof(bake_project_driver));
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
            } else if (strcmp(member, "dependee")) {
                bake_project_driver *driver = bake_project_get_driver(
                    project, member);
                if (!driver) {
                    ut_try(
                      bake_project_load_driver(project, member, obj), NULL);
                    driver = bake_project_get_driver(project, member);
                }
                
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
        char *file = ut_asprintf("%s"UT_OS_PS"dependee.json", libpath);
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
            "%s"UT_OS_PS"bin"UT_OS_PS"%s-%s", project->path, config->build_target,
            config->configuration);

        project->artefact_file = ut_asprintf(
            "%s"UT_OS_PS"%s", project->artefact_path, project->artefact);
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

    if (ut_project_is_buildtool(project->id)) {
        project->bake_extension = true;
    }

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

    project->id_base = strrchr(project->id, '.');
    if (!project->id_base) {
        project->id_base = project->id;
    } else {
        project->id_base ++;
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
    if (!project->use_runtime) {
        project->use_runtime = ut_ll_new();
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

    /* If project imports sources from other projects, add deps folder */
    if (project->standalone) {
        ut_ll_append(project->sources, ut_strdup("deps"));
    }

    ut_try (bake_project_init_language(config, project), NULL);

    /* Project artefact could have been provided manually on command line */
    if (project->artefact) {
        bake_project_init_artefact(config, project);
    }

    if (project->amalgamate) {
        ut_try( bake_project_load_driver(
            project, "amalgamate", NULL), "failed to load amalgamate driver");        
    }

    project->bin_path = ut_asprintf(
        "%s"UT_OS_PS"bin", project->path);

    project->cache_path = ut_asprintf(
        "%s"UT_OS_PS".bake_cache", project->path);

    if (!ut_path_is_relative(project->path)) {
        project->fullpath = ut_strdup(project->path);
    } else if (strcmp(project->path, ".")) {
        project->fullpath = ut_asprintf("%s"UT_OS_PS"%s", ut_cwd(), project->path);
    } else {
        project->fullpath = ut_strdup(ut_cwd());
    }

    project->recursive = false;

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

        if (strchr(member, '$')) {
            ut_throw(
                "cannot use template functions at root of project.json ('%s')",
                member);
            goto error;
        }

        JSON_Value *value = json_object_get_value_at(project->json, i);
        JSON_Object *obj = json_value_get_object(value);

        if (!strcmp(member, "dependee")) {
            project->dependee_json = json_serialize_to_string(value);
        } else if (!strcmp(member, "bundle")) {
            ut_try( bake_project_parse_bundles(project, obj), NULL);
        } else {
            ut_try( bake_project_load_driver(project, member, obj), NULL);
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
    result->coverage = true;

    /* Parse project.json if available */
    if (path) {
        ut_try (
            bake_project_parse(config, result),
            "failed to parse '%s"UT_OS_PS"project.json'",
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
    bake_attr_free_string_array(project->use_runtime);
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
    bake_attr *result = NULL;

    if (driver) {
        if (driver->attributes) {
            result = bake_attr_get(driver->attributes, attr);
        }
        if (!result && driver->base_attributes) {
            result = bake_attr_get(driver->base_attributes, attr);
        }
    }

    return result;
}

/* Get JSON object of driver configuration */
JSON_Object* bake_project_get_json(
    bake_project *project,
    const char *driver_id)
{
    bake_project_driver* driver = bake_project_get_driver(project, driver_id);
    return driver->json;
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

        bake_project_driver* driver = bake_project_get_driver(project, driver_id);
        if (!driver->attributes) {
            driver->attributes = ut_ll_new();
        }

        ut_ll_append(driver->attributes, attr);
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

    bake_attr *attr_el = ut_calloc(sizeof(bake_attr));
    attr_el->kind = BAKE_STRING;
    attr_el->is.string = ut_strdup(value);
    ut_ll_append(attr->is.array, attr_el);

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
            config, project, project->id, driver->json, driver->attributes);
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

static
int16_t bake_merge_dependency_config_value(
    bake_project *p,
    bake_project *dep,
    JSON_Value *dst,
    JSON_Value *src)
{
    JSON_Object *dst_json = json_value_get_object(dst);
    JSON_Object *src_json = json_value_get_object(src);
    int i, count = json_object_get_count(src_json);

    for (i = 0; i < count; i ++) {
        const char *mbr = json_object_get_name(src_json, i);
        JSON_Value *dst_v = json_object_get_value(dst_json, mbr);
        JSON_Value *src_v = json_object_get_value_at(src_json, i);

        if (!dst_v) {
            dst_v = json_value_deep_copy(src_v);
            json_object_set_value(dst_json, mbr, dst_v);
            continue;
        }

        /* Member types must be the same or we can't merge */
        if (json_value_get_type(dst_v) != json_value_get_type(src_v)) {
            ut_throw("conflicting type for member '%s' in dependency '%s'",
                mbr, dep->id);
            goto error;
        }

        /* If type is array, append elements from src to dst */
        if (json_value_get_type(src_v) == JSONArray) {
            JSON_Array *dst_a = json_value_get_array(dst_v);
            JSON_Array *src_a = json_value_get_array(src_v);
            int32_t src_el, dst_el, src_a_count = json_array_get_count(src_a),
                dst_a_count = json_array_get_count(dst_a);

            for (src_el = 0; src_el < src_a_count; src_el ++) {
                JSON_Value *src_el_v = json_array_get_value(src_a, src_el);

                /* Make sure we're not adding duplicate elements */
                for (dst_el = 0; dst_el < dst_a_count; dst_el ++) {
                    JSON_Value *dst_el_v = json_array_get_value(dst_a, dst_el);
                    if (json_value_equals(src_el_v, dst_el_v)) {
                        break;
                    }
                }

                /* Element was not found, add */
                if (dst_el == dst_a_count) {
                    json_array_append_value(dst_a, 
                        json_value_deep_copy(src_el_v));
                }
            }

        /* If type is object, merge objects */
        } else if (json_value_get_type(src_v) == JSONObject) {
            ut_try(
                bake_merge_dependency_config_value(
                    p, dep, dst_v, src_v), NULL);

        /* If other type, values must match or we can't merge */
        } else if (!json_value_equals(dst_v, src_v)) {
            ut_throw("conflicting value for member '%s' in dependency '%s'",
                mbr, dep->id);
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

/* Inherit configuration from embedded projects */
static
int16_t bake_import_dependency_config(
    bake_config *config, 
    bake_project *p, 
    bake_project *dep)
{
    JSON_Object *json = dep->json;
    if (json) {        
        int32_t i, count = json_object_get_count(json);
        for (i = 0; i < count; i ++) {
            const char *mbr = json_object_get_name(json, i);

            /* Skip fields with non-inheritable configuration */
            if (strcmp(mbr, "id") && strcmp(mbr, "type") && 
                strcmp(mbr, "value")) 
            {
                if (!p->embed_json) {
                    p->embed_json = json_value_init_object();
                }

                JSON_Value *dst = NULL;
                dst = json_object_get_value(json_object(p->embed_json), mbr);
                if (!dst) {
                    dst = json_value_init_object();
                    json_object_set_value(json_object(p->embed_json), mbr, dst);
                }

                JSON_Value *src = json_object_get_value_at(json, i);

                if (bake_merge_dependency_config_value(p, dep, dst, src)) {
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
int16_t copy_amalgamated_from_dep(
    bake_config *config,
    bake_project *p,
    bake_driver *amalg_driver,
    const char *dependency,
    ut_ll *amalg_copied)
{
    bake_project *dep = NULL;

    if (!amalg_copied[0]) {
        amalg_copied[0] = ut_ll_new();
    } else {
        ut_iter it = ut_ll_iter(amalg_copied[0]);
        while (ut_iter_hasNext(&it)) {
            char *dep_id = ut_iter_next(&it);
            if (!strcmp(dep_id, dependency)) {
                /* Project is already imported */
                return 0;
            }
        }
    }

    ut_ll_append(amalg_copied[0], strdup(dependency));

    const char *src_path = ut_locate(dependency, NULL, UT_LOCATE_DEVSRC);
    if (!src_path) {
        /* Possible that project is only config and no sources */
        ut_trace("cannot find sources for dependency '%s'", dependency);
    } else {
        ut_trace("source path for '%s' is '%s'", dependency, src_path);

        dep = bake_project_new(src_path, config);
        if (!dep) {
            ut_throw("failed to create project for path '%s'", src_path);
            goto error;
        }

        char *dst_path = ut_asprintf("%s"UT_OS_PS"deps", p->path);
        ut_trace("copy '%s' sources to '%s'", dependency, dst_path);
        dep->generate_path = dst_path;

        ut_log_push("copy-amalgamate");

        // when amalgamating dependencies, copy them directly into deps/
        dep->amalgamate_path = NULL;

        ut_try( bake_driver__generate(amalg_driver, config, dep), NULL);
        ut_log_pop();

        free(dst_path);
    }

    if (!dep) {
        const char *path = ut_locate(dependency, NULL, UT_LOCATE_PROJECT);
        if (!path) {
            ut_throw("cannot find dependency '%s' of project '%s'", 
                dependency, p->id);
            goto error;
        }

        dep = bake_project_new(path, config);
        if (!dep) {
            ut_throw("failed to create project from path '%s'", path);
            goto error;
        }
    }

    /* Import build configuration from dependency */
    bake_import_dependency_config(config, p, dep);

    /* Recursively copy dependencies */
    if (dep->use) {
        ut_iter it = ut_ll_iter(dep->use);
        while (ut_iter_hasNext(&it)) {
            char *dep_of_dep = ut_iter_next(&it);
            ut_try(
                copy_amalgamated_from_dep(
                    config, p, amalg_driver, dep_of_dep, amalg_copied), NULL);
        }
    }

    bake_project_free(dep);

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
    bool private,
    bake_driver *amalg_driver,
    ut_ll *amalg_copied)
{
    ut_locate_reset(dependency);

    /* Try to find dependency path & project settings */
    const char *path = ut_locate(dependency, NULL, UT_LOCATE_PROJECT);
    bake_project *dep = NULL;
    bool dep_has_lib = false;

    if (path) {
        dep = bake_project_new(path, config);
        if (!dep) {
            ut_throw("failed to create project from path '%s'", path);
            goto error;
        }
    } else {
        ut_trace("missing dependency '%s'", dependency);
        p->missing_dependencies ++;
        goto proceed;
    }

    if (dep->type != BAKE_PACKAGE) {
        ut_throw("invalid dependency '%s', not a package", dependency);
        goto error;
    }

    /* Copy files from etc folder of dependency */
    const char *src_path = ut_locate(dependency, NULL, UT_LOCATE_DEVSRC);
    if (src_path) {
        char *etc_path = ut_asprintf("%s"UT_OS_PS"etc", src_path);
        if (ut_file_test("%s", etc_path)) {
            char *dst_etc_path = ut_asprintf("%s"UT_OS_PS"etc", p->path);
            ut_try(ut_cp(etc_path, dst_etc_path), 
                "failed to copy files from '%s'", etc_path);
        }
    }

    /* If standalone, copy source of dependencies to project */    
    if (p->standalone) {
        ut_try(copy_amalgamated_from_dep(
            config, p, amalg_driver, dependency, amalg_copied), NULL);
        goto proceed;
    }

    /* If project doesn't have a language, there's nothing to link with */
    if (!dep->language) {
        goto proceed;
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
    if (dep) {
        bake_project_free(dep);
    }
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
    char *deps_path = NULL, *deps_dependee_file = NULL;
    int32_t total_dependencies = 0;
    ut_ll amalg_copied = NULL;

    if (!project->language) {
        return 0;
    }

    bake_driver *amalg_driver = NULL;
    if (project->standalone) {
        amalg_driver = bake_driver_get("amalgamate");
        if (!amalg_driver) {
            ut_throw("failed to locate amalgamation driver");
            goto error;
        }
    }

    char *artefact_full = project->artefact_file;
    if  (ut_file_test(artefact_full)) {
        artefact_modified = ut_lastmodified(artefact_full);
    }

    if (project->standalone) {
        deps_path = ut_asprintf("%s"UT_OS_PS"deps", project->path);
        deps_dependee_file = ut_asprintf(
            "%s"UT_OS_PS"/dependee.json", deps_path);
        ut_mkdir(deps_path);
    }

    if (project->use) {
        ut_iter it = ut_ll_iter(project->use);
        while (ut_iter_hasNext(&it)) {
            char *package = ut_iter_next(&it);
            if (bake_check_dependency(
                config, project, package, artefact_modified, false, 
                amalg_driver, &amalg_copied))
            {
                goto error;
            }
            total_dependencies ++;
        }
    }

    if (project->use_private) {
        ut_iter it = ut_ll_iter(project->use_private);
        while (ut_iter_hasNext(&it)) {
            char *package = ut_iter_next(&it);
            if (bake_check_dependency(
                config, project, package, artefact_modified, true, 
                amalg_driver, &amalg_copied))
            {
                goto error;
            }
            total_dependencies ++;
        }
    }

    if (project->artefact_outdated) {
        if (ut_rm(project->artefact_file)) {
            goto error;
        }
    }

    /* If not all dependencies could be found, test if we can build this
     * project in standalone mode (with embedded sources) */
    if (project->missing_dependencies) {
        bool standalone = false;

        if (project->standalone) {
            if (ut_file_test(deps_dependee_file) == 1) {
                standalone = true;
            }
        }

        if (!standalone) {
            /* If project has missing dependencies and it can't be built in
             * standalone mode, we can't build it */
            ut_throw(
                "missing dependencies! (rebuild with --trace for more details)");
            goto error;
        }
    }

    /* If all dependencies were found & we're embedding sources, (over)write the
     * combined configuration for the dependencies */
    if (!project->missing_dependencies && project->standalone) {
        if (project->embed_json) {
            json_serialize_to_file_pretty(project->embed_json, deps_dependee_file);
            json_value_free(project->embed_json);
            project->embed_json = NULL;
        } else {
            /* If there's no embedded config, write an empty object so that next
             * time we can detect that we have everything that's needed for a
             * standalone build. */
            json_serialize_to_file(
                json_value_init_object(), deps_dependee_file);
        }
    }

    /* At this point we should have a file with the configuration for the
     * embedded dependencies if we're using embedded sources, whether the
     * dependencies are locally available or not. */
    if (project->standalone) {
        /* Load as regular dependee config */
        bake_project_load_dependee_config(
            config, project, "deps", deps_dependee_file);
    }

    /* No more changes will be made to the project configuration at this point.
     * Iterate project drivers with base drivers to update references to base
     * configuration attributes. */
    ut_iter it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        const char *base = driver->driver->base;
        if (!base) {
            continue;
        }

        bake_project_driver *base_driver = bake_project_get_driver(
            project, base);
        if (base_driver) {
            driver->base_attributes = base_driver->attributes;
        }
    }

    it = ut_ll_iter(amalg_copied);
    while (ut_iter_hasNext(&it)) {
        free(ut_iter_next(&it));
    }
    ut_ll_free(amalg_copied);

    return 0;
error:
    return -1;
}

/* Generate build step */
int16_t bake_project_generate(
    bake_config *config,
    bake_project *project)
{
    /* Evaluate GENERATED-SOURCES nodes if there are any */
    ut_iter it = ut_ll_iter(project->drivers);
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

    /* Invoke generate action for all drivers */
    it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        ut_try( bake_driver__generate(driver->driver, config, project), NULL);
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

        char *link_lib = strrchr(link, UT_OS_PS[0]);
        if (!link_lib) {
            link_lib = link;
        } else {
            link_lib ++;
        }

#ifndef _WIN32
        if (!strncmp(link_lib, "lib", 3)) {
            link_lib += 3;
        }
#endif

        char *target_link = ut_asprintf("%s"UT_OS_PS"%s%s_%s",
            path, UT_LIB_PREFIX, p->id_underscore, link_lib);

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
            "missing dependency '%s'", dep, p->id);
        goto error;
    }

    const char *lib = ut_locate(dep, NULL, UT_LOCATE_LIB);
    if (lib) {
        char *dep_lib = ut_strdup(dep);
        char *ptr, ch;
        for (ptr = dep_lib; (ch = *ptr); ptr ++) {
            if (ch == UT_OS_PS[0] || ch == '.') {
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
    bool error = false;

    if (p->standalone) {
        /* If project is importing amalgamated sources from dependencies, don't
         * link with dependency libraries. */
        return 0;
    }

    /* Add dependencies to link list */
    if (p->use) {
        ut_iter it = ut_ll_iter(p->use);
        while (ut_iter_hasNext(&it)) {
            char *dep = ut_iter_next(&it);
            if (bake_project_add_dependency(p, dep)) {
                error = true;
            }
        }
    }

    /* Add private dependencies to link list */
    if (p->use_private) {
        ut_iter it = ut_ll_iter(p->use_private);
        while (ut_iter_hasNext(&it)) {
            char *dep = ut_iter_next(&it);
            if (bake_project_add_dependency(p, dep)) {
                error = true;
            }
        }
    }

    return error == false ? 0 : -1;
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

    /* Copy libraries to libpath, return list with local library names */
    ut_ll old_link = project->link;
    project->link = bake_project_copy_libs(project, UT_LIB_PATH);
    if (!project->link) {
        ut_throw(NULL);
        goto error;
    }

    /* Add use dependencies to the link attribute */
    if (bake_project_add_dependencies(project)) {
        goto error;
    }

    /* Run driver actions for build stage */
    ut_iter it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        ut_try( bake_driver__build(driver->driver, config, project), NULL);
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

    return (project->error == true) * -1;
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

/* Test build step */
int16_t bake_project_test(
    bake_config *config,
    bake_project *project)
{
    if (project->drivers) {
        ut_iter it = ut_ll_iter(project->drivers);
        while (ut_iter_hasNext(&it)) {
            bake_project_driver *driver = ut_iter_next(&it);
            ut_try(
              bake_driver__test(driver->driver, config, project), NULL);
        }
    }

    return 0;
error:
    return -1;
}


/* Postbuild build step */
int16_t bake_project_coverage(
    bake_config *config,
    bake_project *project)
{
    if (project->drivers) {
        ut_iter it = ut_ll_iter(project->drivers);
        while (ut_iter_hasNext(&it)) {
            bake_project_driver *driver = ut_iter_next(&it);
            ut_try(
              bake_driver__coverage(driver->driver, config, project), NULL);
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
            ut_try(ut_rm(strarg("%s"UT_OS_PS"%s", project->path, file)), NULL);
        }
    }

    if (project->standalone && all_platforms) {
        bake_project_check_dependencies(config, project);
        if (project->missing_dependencies == 0) {
            /* If we don't have all dependencies, don't throw away deps */
            char *deps_path = ut_asprintf("%s"UT_OS_PS"deps", project->path);
            ut_try(ut_rm(deps_path), NULL);
            free(deps_path);
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
    bake_project *project,
    bool is_test)
{
    char *author = project->author;
    if (!author) {
        if (rand() % 2 == 1) {
            author = "John Doe";
        } else {
            author = "Jane Doe";
        }
    }
    
    char *description = project->description;
    if (!description) {
        description = bake_random_description();
    }

    /* Create project.json */
    ut_code *f = ut_code_open("project.json");
    if (!f) {
        ut_throw("failed to open 'project.json");
        goto error;
    }

    ut_code_write(f, "{\n");
    ut_code_indent(f);
    ut_code_write(f, "\"id\": \"%s\",\n", project->id);
    ut_code_write(f, "\"type\": \"%s\",\n", bake_project_type_str(project->type));
    ut_code_write(f, "\"value\": {\n");
    ut_code_indent(f);
    ut_code_write(f, "\"author\": \"%s\",\n", author);
    ut_code_write(f, "\"description\": \"%s\"", description);

    if (strcmp(project->language, "c")) {
        ut_code_write(f, ",\n");
        ut_code_write(f, "\"language\": \"%s\"", project->language);
    }

    if (!project->public) {
        ut_code_write(f, ",\n");
        ut_code_write(f, "\"public\": false");
    }

    if (is_test) {
        ut_code_write(f, ",\n");
        ut_code_write(f, "\"coverage\": false");
    }

    if (project->use && ut_ll_count(project->use)) {
        int count = 0;

        ut_code_write(f, ",\n");
        ut_code_write(f, "\"use\": [");
        ut_code_indent(f);
        
        ut_iter it = ut_ll_iter(project->use);
        while (ut_iter_hasNext(&it)) {
            const char *pkg = ut_iter_next(&it);
            if (count) ut_code_write(f, ",");
            ut_code_write(f, "\n");
            ut_code_write(f, "\"%s\"", pkg);
            count ++;
        }

        ut_code_dedent(f);
        ut_code_write(f, "\n");
        ut_code_write(f, "]");
    }

    ut_code_write(f, "\n");
    ut_code_dedent(f);
    ut_code_write(f, "}");

    if (is_test) {
        ut_code_write(f, ",\n");
        ut_code_write(f, "\"test\": {\n");
        ut_code_indent(f);
        ut_code_write(f, "\"testsuites\": [{\n");
        ut_code_indent(f);
        ut_code_write(f, "\"id\": \"my_suite\",\n");
        ut_code_write(f, "\"testcases\": []\n");
        ut_code_dedent(f);
        ut_code_write(f, "}]\n");
        ut_code_dedent(f);
        ut_code_write(f, "}");
    }

    ut_code_write(f, "\n");
    ut_code_dedent(f);
    ut_code_write(f, "}\n");

    ut_code_close(f);

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

    fprintf(f, ".bake_cache\n");
    fprintf(f, ".DS_Store\n");
    fprintf(f, ".vscode\n");
    fprintf(f, "gcov\n");
    fprintf(f, "bin\n");

    fclose(f);

    return 0;
error:
    return -1;
}

/* Setup a new project */
int16_t bake_project_setup(
    bake_config *config,
    bake_project *project,
    bool is_test)
{
    bake_driver *driver = project->language_driver->driver;

    ut_try(!
        ut_proc_runRedirect("git", (const char*[]){"git", "init", NULL}, stdin, stdout, stderr),
        "failed to initialize git repository");

    ut_try( bake_project_create_project_json(project, is_test), NULL);

    ut_try( bake_project_create_gitignore(project), NULL);

    if (driver) {
        ut_try( bake_driver__setup(driver, config, project), NULL);
    } else {
        ut_throw(
            "cannot init: project '%s' does not configure a language",
            project->id);
        goto error;
    }

    ut_log("Created new %s %s with id '%s'\n",
        project->language, 
        project->type == BAKE_APPLICATION
            ? "application"
            : project->type == BAKE_TEMPLATE 
                ? "template"
                : "package"
            ,
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
    ut_strbuf buf = UT_STRBUF_INIT;

    char *ptr, ch;
    bool in_expr = false;

    for (ptr = file; (ch = *ptr); ptr ++) {
        if (ch == '_' && ptr[1] == '_') {
            if (!in_expr) {
                ut_strbuf_appendstr(&buf, "${");
                ptr ++;
                in_expr = true;
            } else {
                ut_strbuf_appendstr(&buf, "}");
                ptr ++;
                in_expr = false;
            }
        }

        else if (ch == '.' && in_expr) {
            ut_strbuf_appendstr(&buf, "}.");
            in_expr = false;
        }

        else if (ch == '_' && in_expr) {
            ut_strbuf_appendstr(&buf, " ");
        }

        else {
            ut_strbuf_appendstrn(&buf, &ch, 1);
        }
    }

    char *expr = ut_strbuf_get(&buf);
    if (!expr) {
        ut_throw("failed to parse file '%s'", file);
    }

    char *result = bake_attr_replace(config, project, project->id, expr);

    if (!result) {
        ut_throw("failed to parse file '%s' (%s)", file, expr);
    }

    free(expr);

    return result;
}

static
void bake_replace_id_underscore(
    bake_project *project,
    JSON_Value *json_val)
{
    size_t i = 0, count = 0;
    size_t sub_length = strlen("${id underscore}");
    size_t new_sub_length = strlen(project->id_underscore);
    char *replace = "${id underscore}";
    char *json_val_str = json_serialize_to_string(json_val);

    while (json_val_str[i]) {
        if (strstr(&json_val_str[i], replace) == &json_val_str[i]) {
            count++;
            i += sub_length - 1;
        } else {
            i++;
        }
    }

    if (count == 0) {
        return;
    }

    char *final_json_val = malloc(i + count * (new_sub_length - sub_length) + 1);

    i = 0;
    while (*json_val_str) {
        if (strstr(json_val_str, replace) == json_val_str) {
            strcpy(&final_json_val[i], project->id_underscore);
            i += new_sub_length;
            json_val_str += sub_length;
        } else {
            final_json_val[i++] = *json_val_str++;
        }
    }

    final_json_val[i] = '\0';
    json_value_free(json_val);
    json_val = json_parse_string_with_comments(final_json_val);
}

/* Setup a new project from a template */
int16_t bake_project_setup_w_template(
    bake_config *config,
    bake_project *project,
    const char *template_id)
{
    char *template_path = NULL;
    const char *template_root = ut_locate(
            template_id, NULL, UT_LOCATE_TEMPLATE);
    if (!template_root) {
        ut_throw("template '%s' not found", template_id);
        goto error;
    }

    template_path = ut_asprintf("%s"UT_OS_PS"%s", template_root, project->language);
    if (ut_file_test(template_path) != 1) {
        ut_throw("template '%s' not available for language '%s'",
            template_id, project->language);
        goto error;
    }

    ut_debug("start iterating over template path '%s'", template_path);

    ut_iter it;
    ut_try( ut_dir_iter(template_path, "//", &it), NULL);

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        if (file[0] == '.') {
            ut_debug("ignoring hidden file '%s'", file);
            continue;
        }

        if (!strcmp(file, "project.json")) {
            continue;
        }

        bool is_template = bake_is_template_file(file);
        char *src_path = ut_asprintf("%s"UT_OS_PS"%s", template_path, file);
        char *real = file;
        
        if (is_template) {
            real = bake_project_template_filename(config, project, file);
        }

        char *dst_path = ut_asprintf("%s"UT_OS_PS"%s", project->path, real);

        ut_debug("copy '%s' to '%s' (project path = '%s', old filename = '%s', template = %d)", 
            src_path, dst_path, project->path, file, is_template);

        if (!ut_isdir(src_path)) {
            if (is_template) {
                ut_debug("copy template file '%s' to '%s'", src_path, dst_path);
                ut_try( bake_project_file_from_template(
                    config, project, src_path, dst_path),
                    NULL);
            } else {
                ut_debug("copy regular file '%s' to '%s'", src_path, dst_path);
                ut_try( ut_cp(src_path, dst_path), NULL);
            }
        } else {
            ut_debug("create directory '%s'", dst_path);
            ut_try( ut_mkdir(dst_path), NULL);
        }

        free(dst_path);
        free(src_path);
    }

    /* Load JSON from template, tailor to project */
    char *template_json = ut_asprintf("%s"UT_OS_PS"project.json", template_path);
    JSON_Value *value = json_parse_file_with_comments(template_json);
    if (!value) {
        ut_throw("failed to load project.json of template '%s'", template_id);
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
    char *project_json = ut_asprintf("%s"UT_OS_PS"project.json", project->path);
    json_serialize_to_file_pretty(value, project_json);
    free(project_json);
    json_value_free(value);

    ut_try( bake_project_create_gitignore(project), NULL);

    /* Copy .vscode directory from template */
    char *vscode_dst = ut_asprintf("%s"UT_OS_PS".vscode", project->path);
    char *template_launch_json_path = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS".vscode"UT_OS_PS"launch.json", template_root, project->language);
    char *project_launch_json_path = ut_asprintf("%s"UT_OS_PS".vscode"UT_OS_PS"launch.json", project->path);
    JSON_Value *launch_json = json_parse_file_with_comments(template_launch_json_path);
    if (launch_json) {
        bool success = true;
        if (ut_mkdir(vscode_dst, NULL)) {
            ut_throw("failed make directory .vscode for '%s'", project->id);
            success = false;
        }

        bake_replace_id_underscore(project, launch_json);

        if (json_serialize_to_file_pretty(launch_json, project_launch_json_path)) {
            ut_throw("failed to load .vscode/launch.json of template '%s'", template_id);
            success = false;
        }
        
        if (!success) {
            free(template_launch_json_path);
            free(project_launch_json_path);
            free(vscode_dst);
            goto error;
        }
    }
    
    char *template_tasks_json_path = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS".vscode"UT_OS_PS"tasks.json", template_root, project->language);
    char *project_tasks_json_path = ut_asprintf("%s"UT_OS_PS".vscode"UT_OS_PS"tasks.json", project->path);
    JSON_Value *tasks_json = json_parse_file_with_comments(template_tasks_json_path);
    if (tasks_json) {
        bool success = true;
        if (ut_mkdir(vscode_dst, NULL)) {
            ut_throw("failed make directory .vscode for '%s'", project->id);
            success = false;
        }

        bake_replace_id_underscore(project, tasks_json);

        if (json_serialize_to_file_pretty(tasks_json, project_tasks_json_path)) {
            ut_throw("failed to load .vscode/tasks.json of template '%s'", template_id);
            success = false;
        }
        
        if (!success) {
            free(template_tasks_json_path);
            free(project_tasks_json_path);
            free(vscode_dst);
            goto error;
        }
    }

    free(template_launch_json_path);
    free(project_launch_json_path);
    free(template_tasks_json_path);
    free(project_tasks_json_path);
    free(vscode_dst);

    ut_log("Created new %s %s with id '%s' from template '%s'\n\n",
        project->language, 
        project->type == BAKE_APPLICATION
            ? "application"
            : "package",
        project->id,
        template_id,
        project->language);

    ut_log("To run the project, do:\n");
    ut_log("  bake run %s\n", project->id);

    free(template_path);
    return 0;
error:
    free(template_path);
    return -1;
}

uint16_t bake_project_load_bundle_repositories(
    bake_config *config,
    bake_project *project,
    bake_project_bundle *bundle,
    const char *bundle_id)
{
    ut_iter it = ut_rb_iter(project->repositories);
    while (ut_iter_hasNext(&it)) {
        bake_project_repository *repo = ut_iter_next(&it);

        const char *branch = NULL;
        const char *tag = NULL;
        const char *commit = NULL;

        /* Try to find repository in bundle */
        if (bundle) {
            bake_ref *ref = ut_rb_find(bundle->refs, repo->id);
            const char *ref_branch = ref ? ref->branch : NULL;
            const char *ref_commit = ref ? ref->commit : NULL;
            const char *ref_tag = ref ? ref->tag : NULL;

            /* If a ref is found for the repository, get values from ref and use
            * defaults where no values are provided */

            branch = ref_branch
                ? ref_branch
                : bundle->defaults.branch
                    ? bundle->defaults.branch
                    : NULL;

            commit = ref_commit;

            /* Can never set the tag and commit sha simultaneously. If commit is
            * set, don't attempt to assign default values to tag, even if the
            * tag is not set in the ref. 
            *
            * If no commit sha is provided and no values nor default values are
            * provided for the tag, use the bundle_id. This allows users to
            * easily switch between tags for all repositories without having to
            * explicitly configure it. */
            if (!commit) {
                tag = ref_tag
                    ? ref_tag
                    : bundle->defaults.tag
                        ? bundle->defaults.tag
                        : bundle_id;
            }

        /* If there is no bundle, set the tag to the bundle_id, unless the
        * bundle is the default bundle. The default bundle is intended to just
        * fetch the latest state of the master, so don't attempt to load a
        * 'default' tag. */
        } else if (bundle_id) {
            if (strcmp(bundle_id, "default")) {
                tag = bundle_id;
            }
        }

        ut_try( bake_add_repository(
            config,
            repo->id,
            repo->url,
            branch,
            commit,
            tag,
            project->id,
            bundle_id), NULL);
    }    

    return 0;
error:
    return -1;
}

uint16_t bake_project_load_bundle(
    bake_config *config,
    bake_project *project,
    const char *bundle_id)
{
    bool error = false;

    /* Lookup bundle in package */
    bake_project_bundle *bundle = NULL;
    if (project->bundles) {
        if (bundle_id) {
            bundle = ut_rb_find(project->bundles, bundle_id);
        } else {
            bundle = ut_rb_find(project->bundles, "default");
            if (bundle) {
                bundle_id = "default";
            }
        }
    }

    /* Regardless of whether a bundle is found, all repositories are always
     * added to the administration. */
    if (project->repositories) {
        ut_try( bake_project_load_bundle_repositories(
            config, project, bundle, bundle_id), NULL);
    }

    /* Load any bundles the project depends on. By first loading the 
     * repositories, the project gets a chance to register dependent modules as
     * repository, which allows bake to install a bundle if it is not yet
     * installed. */
    if (project->use_bundle) {
        ut_iter it = ut_ll_iter(project->use_bundle);
        while (ut_iter_hasNext(&it)) {
            const char *bundle = ut_iter_next(&it);
            if (bake_use(config, bundle, false, true)) {
                ut_catch();
                ut_error("failed to load bundle '%s' for project '%s'", 
                    bundle, project->id);
                error = true;
            }
        }
    }    

    return error == false ? 0 : -1;
error:
    return -1;
}
