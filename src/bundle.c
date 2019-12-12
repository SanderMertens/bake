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

static ut_rb rrefs;

static
int repository_cmp(
    void *ctx,
    const void* key1,
    const void* key2)
{
    return strcmp(key1, key2);
}

int16_t bake_use(
    bake_config *config, 
    const char *expr,
    bool update_config,
    bool load_bundle)
{
    char *pkg = ut_strdup(expr);
    char *bundle = strrchr(pkg, ':');
    if (bundle) {
        *bundle = '\0';
        bundle ++;
    }

    const char *path = ut_locate(pkg, NULL, UT_LOCATE_PROJECT);
    if (!path) {
        /* Check if bundle is registered as a repository */
        bake_repository *repo = bake_find_repository(config, pkg);
        if (repo) {
            /* If it is, try to install it */
            ut_try( bake_install(config, pkg), NULL);

            /* Reset locate cache, as project will have been cloned to env */
            ut_locate_reset(pkg);
            path = ut_locate(pkg, NULL, UT_LOCATE_PROJECT);
        }
    }

    if (!path) {
        ut_error("cannot find project '%s' of bundle '%s'", pkg, expr);
        goto error;
    }

    if (update_config) {
        ut_trace("add bundle '%s:%s' to configuration", pkg, bundle);

        bool changed = false;
        ut_try( bake_config_use_bundle(config, pkg, bundle, &changed), NULL);

        /* Set to "default" rather than NULL for more pleasant log to the console */
        if (!bundle) {
            bundle = "default";
        }

        if (changed) {
            bake_message(UT_LOG, "info", 
                "bundle '%s:%s' set in configuration",
                pkg, bundle);
        } else {
            bake_message(UT_LOG, "info", 
                "bundle '%s:%s' is already configured",
                pkg, bundle);
        }
    }

    if (load_bundle) {
        ut_trace("load bundle '%s:%s'", pkg, bundle);

        bake_project *project = bake_project_new(path, config);
        if (!project) {
            goto error;
        }

        ut_try( bake_project_load_bundle(config, project, bundle), NULL);
    }

    return 0;
error:
    return -1;
}

int16_t bake_unuse(
    bake_config *config,
    const char *project)
{
    char *pkg = ut_strdup(project);
    char *bundle = strrchr(pkg, ':');
    if (bundle) {
        *bundle = '\0';
        bundle ++;
        ut_throw("should not specify bundle with unuse, try 'bake unuse %s'",
            pkg);
        free(pkg);
        goto error;
    }

    const char *path = ut_locate(pkg, NULL, UT_LOCATE_PROJECT);
    if (!path) {
        ut_error("cannot find project '%s'", pkg);
        goto error;
    }

    bool changed = false;
    ut_try( bake_config_unuse_bundle(config, pkg, &changed), NULL);

    if (changed) {
        bake_message(UT_LOG, "info", 
            "project '%s' removed from bundle configuration",
            pkg);
    } else {
        bake_message(UT_LOG, "info", 
            "project '%s' was not found in bundle configuration",
            pkg);
    }

    return 0;
error:
    return -1;    
}

static
int16_t check_conflict(
    const char *id,
    const char *attribute,
    const char *value1,
    const char *value2)
{
    if (value1 && value2) {
        if (strcmp(value1, value2)) {
            ut_error("bundle conflict for '%s': %s mismatch ('%s' vs. '%s')",
                id, attribute, value1, value2);
            goto error;                
        }
    } else if (value1 != value2) {
        if (value1) {
            ut_error("bundle conflict for '%s': %s mismatch ('%s' vs. null)",
                id, attribute, value1);
            goto error;
        } else {
            ut_error("bundle conflict for '%s': %s mismatch ('%s' vs. null)",
                id, attribute, value2);  
            goto error;              
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_add_repository(
    bake_config *cfg,
    const char *id,
    const char *repository,
    const char *branch,
    const char *commit,
    const char *tag,
    const char *project,
    const char *bundle)
{
    bake_repository *rref = NULL;
    
    if (!cfg->repositories) {
        cfg->repositories = ut_rb_new(repository_cmp, NULL);
    } else {
        rref = ut_rb_find(cfg->repositories, id);
    }

    if (!rref) {
        rref = ut_calloc(sizeof(bake_repository));
        rref->id = ut_strdup(id);
        rref->url = ut_strdup(repository);  
        ut_rb_set(cfg->repositories, id, rref);      
    }

    /* If the previous value did not set a bundle, override previous values. A
     * repository without a bundle comes from the repository field in the value
     * section of a project file. */
    if (!rref->bundle && bundle) {
        /* The repository URL has to match */
        ut_try( check_conflict(
            id, 
            "repository", 
            rref->url, 
            repository), NULL);

        /* These fields are guaranteed to not be set */
        rref->branch = ut_strdup(branch);
        rref->commit = ut_strdup(commit);
        rref->tag = ut_strdup(tag);

        /* If the repository was set through the repository field, the project
         * id was already set. Override with the project of the bundle. */
        free(rref->project);
        rref->project = ut_strdup(project);
        rref->bundle = ut_strdup(bundle);

        ut_trace("found #[cyan]%s#[normal] -> #[green]%s:%s#[normal]", 
            rref->url, rref->branch ? rref->branch : "master",
            rref->tag
                ? rref->tag
                : rref->commit
                    ? rref->commit
                    : "latest");

    /* If the bundle is set, make sure the new values don't conflict with the
     * old ones. */
    } else if (rref->bundle && bundle) {
        ut_try( check_conflict(
            id, 
            "repository", 
            rref->url, 
            repository), NULL);

        ut_try( check_conflict(
            id, 
            "branch", 
            rref->branch, 
            branch), NULL);

        ut_try( check_conflict(
            id, 
            "commit", 
            rref->commit, 
            commit), NULL);

        ut_try( check_conflict(
            id, 
            "tag", 
            rref->tag, 
            tag), NULL);

    /* It can happen that after loading the bundle, the project.json file is
     * parsed again. In that case just check if the repository still matches. */
    } else if (!bundle) {
        ut_try( check_conflict(
            id, 
            "repository", 
            rref->url, 
            repository), NULL);
        
        /* Nothing else needs to be done, the bundle takes precedence over the
         * repository field in the value section. */
    }

    return 0;
error:
    ut_error("conflicting bundles: %s:%s, %s:%s", 
        rref->project, 
        rref->bundle ? rref->bundle : "[value.repository]", 
        project, 
        bundle ? bundle : "[value.repository]");
    return -1;
}

bake_repository* bake_find_repository(
    bake_config *cfg,
    const char *id)
{
    return ut_rb_find(cfg->repositories, id);
}