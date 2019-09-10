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

typedef struct bake_repository_ref {
    const char *id;
    const char *repository;
    const char *branch;
    const char *commit;
    const char *tag;
    const char *bundle_source;
    const char *bundle;
} bake_repository_ref;

static ut_rb rrefs;

static
int repository_ref_cmp(
    void *ctx,
    const void* key1,
    const void* key2)
{
    return strcmp(key1, key2);
}

int16_t bake_use(
    bake_config *config, 
    const char *expr)
{
    char *pkg = ut_strdup(expr);
    char *bundle = strrchr(pkg, ':');
    if (bundle) {
        *bundle = '\0';
        bundle ++;
    }

    const char *path = ut_locate(pkg, NULL, UT_LOCATE_PROJECT);
    if (!path) {
        ut_error("cannot find project '%s'", pkg);
        goto error;
    }

    bool changed = false;
    ut_try( bake_config_use_bundle(config, pkg, bundle, &changed), NULL);

    if (changed) {
        bake_message(UT_LOG, "info", 
            "bundle '%s:%s' added to configuration",
            pkg, bundle);
    } else {
        bake_message(UT_LOG, "info", 
            "bundle '%s:%s' already added to configuration",
            pkg, bundle);        
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
    const char *id,
    const char *repository,
    const char *branch,
    const char *commit,
    const char *tag,
    const char *bundle_source,
    const char *bundle)
{
    bake_repository_ref *rref = NULL;

    if (!rrefs) {
        rrefs = ut_rb_new(repository_ref_cmp, NULL);
    } else {
        rref = ut_rb_find(rrefs, id);
    }

    if (!rref) {
        rref = malloc(sizeof(bake_repository_ref));
        rref->id = ut_strdup(id);
        rref->repository = ut_strdup(repository);
        rref->branch = ut_strdup(branch);
        rref->commit = ut_strdup(commit);
        rref->tag = ut_strdup(tag);
        rref->bundle_source = ut_strdup(bundle_source);
        rref->bundle = ut_strdup(bundle);
        ut_rb_set(rrefs, id, rref);
    } else {
        ut_try( check_conflict(
            id, 
            "repository", 
            rref->repository, 
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
    }

    return 0;
error:
    ut_error("conflicting bundles: %s:%s, %s:%s", 
        rref->bundle_source, rref->bundle, bundle_source, bundle);
    return -1;
}