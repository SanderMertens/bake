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

typedef int (*buildmain_cb)(bake_driver_api *driver);

static ut_ll drivers;
extern ut_tls BAKE_DRIVER_KEY;
extern ut_tls BAKE_FILELIST_KEY;
extern ut_tls BAKE_PROJECT_KEY;

static
void* bake_node_add(
    bake_driver *driver,
    void *n) /* void* to prevent excessive upcasting */
{
    ut_ll_append(driver->nodes, n);
    return n;
}

static
int16_t bake_node_addDependencies(
    bake_driver *driver,
    bake_node *node,
    const char *pattern)
{
    char *str = ut_strdup(pattern);
    const char *ptr = strtok(str, ",");
    while (ptr) {
        if (ptr[0] == '$') {
            /* Create dependency to named node */
            bake_node *dep = bake_node_find(driver, &ptr[1]);
            if (!dep) {
                ut_throw("dependency '%s' not found for rule '%s'",
                    ptr, node->name);
                goto error;
            } else {
                if (!node->deps) node->deps = ut_ll_new();
                ut_ll_append(node->deps, dep);
            }
        } else {
            /* Create dependency to anonymous pattern */
            bake_pattern *pattern = bake_pattern_new(NULL, ptr);
            if (!node->deps) node->deps = ut_ll_new();
            ut_ll_append(node->deps, pattern);
        }
        ptr = strtok(NULL, ",");
    }
    free(str);
    return 0;
error:
    free(str);
    return -1;
}

static
int16_t bake_node_addToTarget(
    bake_driver *driver,
    bake_node *node,
    bake_rule_target *target)
{
    const char *pattern = NULL;
    if (target->kind == BAKE_RULE_TARGET_PATTERN) {
        pattern = target->is.pattern;
    }

    /* If target specifies n targets, target is dynamic and there is no node
     * representing the target. */
    if (pattern) {
        char *dup = ut_strdup(pattern);
        char *tok = strtok(dup, ",");
        while (tok) {
            if (tok[0] != '$') {
                ut_throw("target '%s' for rule '%s' does not refer named node",
                    pattern, node->name);
                goto error;
            }

            bake_node *targetNode = bake_node_find(driver, &tok[1]);
            if (!targetNode) {
                ut_throw("unresolved target '%s' for node '%s'",
                    tok, node->name);
                goto error;
            }

            if (!targetNode->deps) targetNode->deps = ut_ll_new();
            ut_ll_append(targetNode->deps, node);
            tok = strtok(NULL, ",");
        }
        free(dup);
    }

    return 0;
error:
    return -1;
}

static
void bake_driver_pattern(
    bake_driver *driver,
    const char *name,
    const char *pattern)
{
    bake_node *n;
    if ((n = bake_node_find(driver, name))) {
        if (n->kind != BAKE_RULE_PATTERN) {
            driver->error = 1;
            ut_error("'%s' redeclared as pattern", name);
        } else {
            ((bake_pattern*)n)->pattern = pattern;
        }

    } else {
        bake_node_add(driver, bake_pattern_new(name, pattern));
    }
}

static
void bake_driver_rule(
    bake_driver *driver,
    const char *name,
    const char *source,
    bake_rule_target target,
    bake_rule_action_cb action)
{
    bake_node *n;
    if (!source && target.kind == BAKE_RULE_TARGET_MAP) {
        driver->error = 1;
        ut_error("rule '%s' has mapped target but no source to map from", name);
    } else if ((n = bake_node_find(driver, name))) {
        if (n->kind != BAKE_RULE_RULE) {
            driver->error = 1;
            ut_error("'%s' redeclared as rule", name);
        } else {
            ((bake_rule*)n)->source = source;
            ((bake_rule*)n)->target = target;
            ((bake_rule*)n)->action = action;
        }
    } else {
        bake_node *n = bake_node_add(driver, bake_rule_new(name, source, target, action));
        if (bake_node_addDependencies(driver, n, source)) {
            ut_throw(NULL);
            driver->error = 1;
        }
        if (bake_node_addToTarget(driver, n, &target)) {
            ut_throw(NULL);
            driver->error = 1;
        }
    }
}

static
void bake_driver_dependency_rule(
    bake_driver *driver,
    const char *name,
    const char *deps,
    bake_rule_target dep_mapping,
    bake_rule_action_cb action)
{
    if (bake_node_find(driver, name)) {
        driver->error = 1;
        ut_error("rule '%s' redeclared with dependencies = '%s'", name, deps);
    } else {
        bake_node_add(driver,
            bake_dependency_rule_new(name, deps, dep_mapping, action));
    }
}

static
void bake_driver_pattern_cb(
    const char *name,
    const char *pattern)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    bake_driver_pattern(driver, name, pattern);
}

static
void bake_driver_rule_cb(
    const char *name,
    const char *source,
    bake_rule_target target,
    bake_rule_action_cb action)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    bake_driver_rule(driver, name, source, target, action);
}

static
void bake_driver_dependency_rule_cb(
    const char *name,
    const char *deps,
    bake_rule_target dep_mapping,
    bake_rule_action_cb action)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    bake_driver_dependency_rule(driver, name, deps, dep_mapping, action);
}

static
void bake_driver_condition_cb(
    const char *name,
    bake_condition_cb cond)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    bake_node *n = bake_node_find(driver, name);
    if (!n) {
        ut_throw("node '%s' not found for condition", name);
        driver->error = true;
    } else {
        n->cond = cond;
    }
}

static
bake_rule_target bake_driver_target_pattern_cb(
    const char *pattern)
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_PATTERN;
    result.is.pattern = pattern;
    return result;
}

static
bake_rule_target bake_driver_target_map_cb(
    bake_rule_map_cb mapping)
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_MAP;
    result.is.map = mapping;
    return result;
}

static
void bake_driver_init_cb(
    bake_driver_cb init)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    driver->impl.init = init;
}

static
void bake_driver_setup_cb(
    bake_driver_cb setup)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    driver->impl.setup = setup;
}

static
void bake_driver_artefact_cb(
    bake_artefact_cb artefact)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    driver->impl.artefact = artefact;
}

static
void bake_driver_link_to_lib_cb(
    bake_link_to_lib_cb link_to_lib)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    driver->impl.link_to_lib = link_to_lib;
}

static
void bake_driver_clean_cb(
    bake_driver_cb clean)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    driver->impl.clean = clean;
}

static
bake_attribute* bake_driver_get_attr_cb(
    const char *name)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    bake_project *project = ut_tls_get(BAKE_PROJECT_KEY);
    return bake_project_get_attr(project, driver->id, name);
}

static
bool bake_driver_get_bool_attr_cb(
    const char *name)
{
    bake_driver *driver = ut_tls_get(BAKE_DRIVER_KEY);
    bake_project *project = ut_tls_get(BAKE_PROJECT_KEY);
    bake_attribute *attr = bake_project_get_attr(project, driver->id, name);
    if (!attr) {
        return false;
    }

    if (attr->kind == BAKE_BOOLEAN) {
        return attr->is.boolean;
    } else {
        project->error = true;
        ut_throw("attribute '%s' is not of type boolean", name);
    }
}

static
void bake_driver_exec_cb(
    const char *cmd)
{
    char *envcmd = ut_envparse("%s", cmd);
    if (!envcmd) {
        ut_throw("invalid command '%s'", cmd);
        bake_project *p = ut_tls_get(BAKE_PROJECT_KEY);
        p->error = true;
    } else {
        int8_t ret = 0;
        int sig = 0;
        if ((sig = ut_proc_cmd(envcmd, &ret)) || ret) {
            if (!sig) {
                ut_throw("command returned %d", ret);
                ut_throw_detail("%s", envcmd);
            } else {
                ut_throw("command exited with signal %d", sig);
                ut_throw_detail("%s", envcmd);
            }

            bake_project *p = ut_tls_get(BAKE_PROJECT_KEY);
            p->error = true;
        }
        free(envcmd);
    }
}

/* Initialize driver API */
bake_driver_api bake_driver_api_impl = {
    .pattern = bake_driver_pattern_cb,
    .rule = bake_driver_rule_cb,
    .dependency_rule = bake_driver_dependency_rule_cb,
    .condition = bake_driver_condition_cb,
    .target_pattern = bake_driver_target_pattern_cb,
    .target_map = bake_driver_target_map_cb,
    .init = bake_driver_init_cb,
    .setup = bake_driver_setup_cb,
    .artefact = bake_driver_artefact_cb,
    .link_to_lib = bake_driver_link_to_lib_cb,
    .clean = bake_driver_clean_cb,
    .exec = bake_driver_exec_cb,
    .get_attr = bake_driver_get_attr_cb,
    .get_attr_bool = bake_driver_get_bool_attr_cb
};

char* bake_driver__artefact(
    bake_driver *driver,
    bake_config *config,
    bake_project *project)
{
    if (driver->impl.artefact) {
        ut_tls_set(BAKE_DRIVER_KEY, driver);
        ut_tls_set(BAKE_PROJECT_KEY, project);
        return driver->impl.artefact(
            &bake_driver_api_impl, config, project);
    }

    return NULL;
}

char* bake_driver__link_to_lib(
    bake_driver *driver,
    bake_config *config,
    bake_project *project,
    const char *link)
{
    if (driver->impl.link_to_lib) {
        ut_tls_set(BAKE_DRIVER_KEY, driver);
        ut_tls_set(BAKE_PROJECT_KEY, project);
        return driver->impl.link_to_lib(
            &bake_driver_api_impl, config, project, link);
    }

    return NULL;
}

int16_t bake_driver__clean(
    bake_driver *driver,
    bake_config *config,
    bake_project *project)
{
    if (driver->impl.clean) {
        ut_tls_set(BAKE_DRIVER_KEY, driver);
        ut_tls_set(BAKE_PROJECT_KEY, project);
        driver->impl.clean(
            &bake_driver_api_impl, config, project);

        if (project->error) {
            return -1;
        }
    }

    return 0;
}

int16_t bake_driver__setup(
    bake_driver *driver,
    bake_config *config,
    bake_project *project)
{
    if (driver->impl.setup) {
        ut_tls_set(BAKE_DRIVER_KEY, driver);
        ut_tls_set(BAKE_PROJECT_KEY, project);
        driver->impl.setup(&bake_driver_api_impl, config, project);

        if (project->error) {
            return -1;
        }

        return 0;
    }

    ut_throw("driver doesn't support project setup");

    return -1;
}

bake_driver* bake_driver_get(
    const char *id)
{
    char *package_id = ut_asprintf("bake.%s", id);
    bake_driver *driver = NULL;

    if (!drivers) {
        drivers = ut_ll_new();
    }

    /* Check if driver is already loaded */
    ut_iter it = ut_ll_iter(drivers);
    while (ut_iter_hasNext(&it) && !driver) {
        bake_driver *e = ut_iter_next(&it);
        if (!strcmp(e->package_id, package_id)) {
            driver = e;
        }
    }

    if (!driver) {
        ut_dl dl = NULL;

        /* Load package library and bakemain symbol */
        buildmain_cb cb = ut_load_sym(package_id, &dl, "bakemain");
        if (!dl) {
            ut_throw("could not load driver '%s'", package_id);
            goto error;
        }
        if (!cb) {
            ut_throw("cannot find bakemain in driver '%s'", package_id);
            goto error;
        }

        /* Create a new driver */
        driver = malloc(sizeof(bake_driver));
        driver->dl = dl;
        driver->id = ut_strdup(id);
        driver->package_id = package_id;
        driver->nodes = ut_ll_new();

        /* Set driver object in tls so callbacks can retrieve the driver
         * object without having to explicitly specify it. */
        ut_tls_set(BAKE_DRIVER_KEY, driver);

        cb(&bake_driver_api_impl);

        ut_trace("driver '%s' loaded (package id = '%s')", id, package_id);
    }

    return driver;
error:
    return NULL;
}
