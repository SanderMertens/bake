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

static
int16_t bake_project_parse_value(
    bake_project *p,
    JSON_Object *jo)
{
    uint32_t i, count = json_object_get_count(jo);
    bool error = false;

    for (i = 0; i < count; i ++) {
        JSON_Value *v = json_object_get_value_at(jo, i);
        const char *member = json_object_get_name(jo, i);

        if (!strcmp(member, "public")) {
           ut_try (bake_json_set_boolean(&p->public, member, v), NULL);
        } else
        if (!strcmp(member, "author")) {
            ut_try (bake_json_set_string(&p->author, member, v), NULL);
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
        if (!strcmp(member, "language")) {
            ut_try (bake_json_set_string(&p->language, member, v), NULL);
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
        if (!strcmp(member, "keep_artefact")) {
            ut_try (bake_json_set_boolean(&p->keep_artefact, member, v), NULL);
        } else {
            ut_throw("unknown member '%s' in project.json", member);
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

static
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

static
int16_t bake_project_parse(
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
        if (j_value) {
            ut_try (bake_project_parse_value(project, j_value), NULL);
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

static
int16_t bake_project_load_dependee_config(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    const char *file)
{
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

    uint32_t i, count = json_object_get_count(jo);

    for (i = 0; i < count; i ++) {
        const char *member = json_object_get_name(jo, i);

        if (!strcmp(member, "id") || !strcmp(member, "type")) {
            ut_throw("dependee config cannot override '%s'", member);
            goto error;
        }

        JSON_Value *value = json_object_get_value_at(jo, i);
        JSON_Object *obj = json_value_get_object(value);

        if (strcmp(member, "value")) {
            bake_project_driver *driver = bake_project_get_driver(project, member);
            if (!driver) {
                ut_try( bake_project_load_driver(project, member, obj), NULL);
            } else {
                if (!bake_attributes_parse(
                    config, project, project->id, obj, driver->attributes))
                {
                    ut_throw("failed to parse dependee config for driver %s", member);
                    goto error;
                }
            }
        } else {
            ut_try (bake_project_parse_value(project, obj), NULL);
        }
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_project_add_dependee_config(
    bake_config *config,
    bake_project *project,
    const char *dependency)
{
    const char *libpath = ut_locate(dependency, NULL, UT_LOCATE_PACKAGE);
    if (!libpath) {
        ut_throw("failed to locate path for dependency '%s'", dependency);
        goto error;
    }

    /* Check if dependency has a dependee file with build instructions */
    char *file = ut_asprintf("%s/dependee.json", libpath);
    if (ut_file_test(file)) {
        ut_try (
          bake_project_load_dependee_config(config, project, dependency, file),
          NULL);
    }

    free(file);

    return 0;
error:
    return -1;
}

/* -- Public API -- */

int16_t bake_project_init(
    bake_config *config,
    bake_project *project)
{
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
        ut_ll_append(project->sources, "src");
    }
    if (!ut_ll_count(project->includes)) {
        ut_ll_append(project->includes, "include");
    }

    if (!project->language) {
        project->language = ut_strdup("c");
    }

    if (!strcmp(project->language, "none")) {
        free(project->language);
        project->language = NULL;
    }

    if (project->language) {
        ut_try( bake_project_load_driver(
            project,
            strarg("lang.%s", project->language),
            NULL),
              "failed to load driver for language '%s'", project->language);

        bake_project_driver *driver = ut_ll_get(project->drivers, 0);
        project->language_driver = driver;

        if (!project->artefact) {
            project->artefact = bake_driver__artefact(
                driver->driver, config, project);
        }
    }

    project->artefact_path = ut_asprintf(
        "%s/bin/%s-%s", project->path, UT_PLATFORM_STRING,
        config->configuration);

    if (project->artefact) {
        project->artefact_file = ut_asprintf(
            "%s/%s", project->artefact_path, project->artefact);

        ut_trace("building artefact '%s' for project '%s'",
            project->artefact, project->id);
    }

    project->bin_path = ut_asprintf(
        "%s/bin", project->path);

    project->cache_path = ut_asprintf(
        "%s/.bake_cache", project->path);

    return 0;
error:
    return -1;
}

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
            bake_project_parse(result),
            "failed to parse '%s/project.json'",
            path);

        ut_try( bake_project_init(config, result), NULL);
    }

    return result;
error:
    return NULL;
}

void bake_project_free(
    bake_project *project)
{

}

bake_attribute* bake_project_get_attr(
    bake_project *project,
    const char *driver_id,
    const char *attr)
{
    bake_project_driver* driver = bake_project_get_driver(project, driver_id);
    if (driver && driver->attributes) {
        return bake_attribute_get(driver->attributes, attr);
    } else {
        return NULL;
    }
}

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

int bake_project_parse_driver_config(
    bake_config *config,
    bake_project *project)
{
    ut_iter it = ut_ll_iter(project->drivers);

    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        driver->attributes = bake_attributes_parse(
            config, project, project->id, driver->json, NULL);
    }

    return 0;
error:
    return -1;
}

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
int16_t bake_check_dependency(
    bake_project *p,
    const char *dependency,
    uint32_t artefact_modified,
    bool private)
{
    const char *lib = ut_locate(dependency, NULL, UT_LOCATE_PACKAGE);
    if (!lib) {
        ut_info("use '%s' => #[red]missing#[normal]", dependency, lib);
        ut_throw("missing dependency '%s'", dependency);
        goto error;
    }

    time_t dep_modified = ut_lastmodified(lib);

    if (!artefact_modified || dep_modified <= artefact_modified) {
        const char *fmt = private
            ? "#[grey] use %s => %s (modified=%d private)"
            : "#[grey] use %s => %s (modified=%d)"
            ;
        ut_ok(fmt, dependency, lib);
    } else {
        p->artefact_outdated = true;
        const char *fmt = private
            ? "#[grey] use %s => %s (modified=%d, changed, private)"
            : "#[grey] use %s => %s (modified=%d, changed)"
            ;
        ut_ok(fmt, dependency, lib, dep_modified);
    }

proceed:
    return 0;
error:
    return -1;
}

int16_t bake_project_check_dependencies(
    bake_config *config,
    bake_project *project)
{
    time_t artefact_modified = 0;

    char *artefact_full = project->artefact_file;
    if  (ut_file_test(artefact_full)) {
        artefact_modified = ut_lastmodified(artefact_full);
    }

    if (project->use) {
        ut_iter it = ut_ll_iter(project->use);
        while (ut_iter_hasNext(&it)) {
            char *package = ut_iter_next(&it);
            if (bake_check_dependency(
                project, package, artefact_modified, false))
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
                project, package, artefact_modified, true))
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

int16_t bake_project_generate(
    bake_config *config,
    bake_project *project)
{
    /* Invoke generate action for all drivers */
    ut_iter it = ut_ll_iter(project->drivers);
    while (ut_iter_hasNext(&it)) {
        bake_project_driver *driver = ut_iter_next(&it);
        bake_driver__generate(driver->driver, config, project);
    }

    return 0;
}

int16_t bake_project_build_generated(
    bake_config *config,
    bake_project *project)
{
    return 0;
}

static
int16_t bake_project_resolve_links(
    bake_config *config,
    bake_project *project)
{
    ut_ll resolved = ut_ll_new();
    ut_iter it = ut_ll_iter(project->link);

    while (ut_iter_hasNext(&it)) {
        char *link = ut_iter_next(&it);
        char *parsed = bake_attribute_replace(config, project, NULL, link);
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

    bake_clean_string_array(project->link);
    project->link = resolved;

    return 0;
error:
    bake_clean_string_array(resolved);
    return -1;
}

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
            path, p->id, link_lib);
        char *ptr = target_link + strlen(path) + 1, ch;
        for (; (ch = *ptr); ptr ++) {
            if (ch == '/') {
                *ptr = '_';
            }
        }

        /* Copy to path */
        if (ut_cp(link, target_link)) {
            ut_throw("failed to library in link '%s'", link);
            goto error;
        }

        free(target_link);

        /* Create library name for linking */
        char *link_name = ut_asprintf("%s_%s", p->id, link_lib);
        for (ptr = link_name; (ch = *ptr); ptr ++) {
            if (ch == '/') {
                *ptr = '_';
            }
        }

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

static
int16_t bake_project_add_dependency(
    bake_project *p,
    const char *dep)
{
    const char *libpath = ut_locate(dep, NULL, UT_LOCATE_PACKAGE);
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

static
int16_t bake_project_build_artefact(
    bake_config *config,
    bake_project *project,
    const char *artefact,
    const char *artefact_path,
    const char *rule_name)
{
    bake_driver *driver = project->language_driver->driver;
    bake_node *root = bake_node_find(driver, rule_name);
    if (!root) {
        ut_throw("rule '%s' not found in driver '%s'", rule_name, driver->id);
        goto error;
    }

    ut_try (ut_mkdir(artefact_path), NULL);

    ut_tls_set(BAKE_PROJECT_KEY, project);

    /* Evaluate root node */
    char *binaryPath = config->target_lib;
    bake_filelist *artefact_fl = bake_filelist_new(NULL, NULL);

    bake_filelist_add_file(artefact_fl, project->artefact_file);
    if (bake_node_eval(driver, root, project, config, artefact_fl, NULL)) {
        ut_throw("failed to build rule '%s'", rule_name);
        goto error;
    }
    bake_filelist_free(artefact_fl);

    return 0;
error:
    return -1;
}

int16_t bake_project_build(
    bake_config *config,
    bake_project *project)
{
    if (!project->artefact) {
        return 0;
    }

    char *artefact = project->artefact;

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

    char *artefact_path = project->artefact_path;

    /* Run the top-level ARTEFACT rule */
    if (bake_project_build_artefact(
        config,
        project,
        artefact,
        artefact_path,
        "ARTEFACT"))
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

int16_t bake_project_clean_intern(
    bake_config *config,
    bake_project *project,
    bool all_platforms)
{
    bake_driver *driver = project->language_driver->driver;

    ut_try( bake_driver__clean(driver, config, project), NULL);

    ut_try( ut_rm(project->cache_path), NULL);

    if (!project->keep_artefact) {
        if (all_platforms) {
            ut_try( ut_rm(project->bin_path), NULL);
        } else {
            ut_try( ut_rm(project->artefact_path), NULL);
        }
    }

    ut_iter it = ut_ll_iter(project->files_to_clean);
    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        ut_try(ut_rm(strarg("%s/%s", project->path, file)), NULL);
    }

    project->changed = true;

    return 0;
error:
    return -1;
}

int16_t bake_project_clean(
    bake_config *config,
    bake_project *project)
{
    return bake_project_clean_intern(config, project, true);
}

int16_t bake_project_clean_current_platform(
    bake_config *config,
    bake_project *project)
{
    return bake_project_clean_intern(config, project, true);
}

int16_t bake_project_setup(
    bake_config *config,
    bake_project *project)
{
    bake_driver *driver = project->language_driver->driver;

    if (driver) {
        ut_try( bake_driver__setup(driver, config, project), NULL);
    } else {
        ut_throw(
            "cannot init: project '%s' does not configure a language",
            project->id);
        goto error;
    }

    return 0;
error:
    return -1;
}
