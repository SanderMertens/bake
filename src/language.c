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

static corto_ll languages;

extern corto_tls BAKE_LANGUAGE_KEY;
extern corto_tls BAKE_FILELIST_KEY;
extern corto_tls BAKE_PROJECT_KEY;

typedef int (*buildmain_cb)(bake_language *l);

static
bake_node* bake_node_find(
    bake_language *l,
    const char *name)
{
    corto_iter it = corto_ll_iter(l->nodes);
    bake_node *result = NULL;

    while (corto_iter_hasNext(&it)) {
        bake_node *e = corto_iter_next(&it);
        if (!strcmp(e->name, name)) {
            result = e;
            break;
        }
    }

    return result;
}

static
void bake_language_pattern_cb(
    const char *name,
    const char *pattern)
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    bake_language_pattern(l, name, pattern);
}

static
void bake_language_rule_cb(
    const char *name,
    const char *source,
    bake_rule_target target,
    bake_rule_action_cb action)
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    bake_language_rule(l, name, source, target, action);
}

static
void bake_language_dependency_rule_cb(
    const char *name,
    const char *deps,
    bake_rule_target dep_mapping,
    bake_rule_action_cb action)
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    bake_language_dependency_rule(l, name, deps, dep_mapping, action);
}

static
void bake_language_condition_cb(
    const char *name,
    bake_rule_condition_cb cond)
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    bake_node *n = bake_node_find(l, name);
    if (!n) {
        corto_throw("node '%s' not found for condition", name);
        l->error = true;
    } else {
        n->cond = cond;
    }
}

static
bake_rule_target bake_language_target_pattern_cb(
    const char *pattern)
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_PATTERN;
    result.is.pattern = pattern;
    return result;
}

static
bake_rule_target bake_language_target_map_cb(
    bake_rule_map_cb mapping)
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_MAP;
    result.is.map = mapping;
    return result;
}

static
void bake_language_init_cb(
    bake_rule_init_cb init)
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    l->init_cb = init;
}

static
void bake_language_artefact_cb(
    bake_rule_artefact_cb artefact)
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    l->artefact_cb = artefact;
}

static
void bake_language_clean_cb(
    bake_rule_clean_cb clean)
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    l->clean_cb = clean;
}

static
void bake_language_exec_cb(
    const char *cmd)
{
    char *envcmd = corto_envparse("%s", cmd);
    if (!envcmd) {
        corto_throw("invalid command '%s'", cmd);
        bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
        p->error = true;
    } else {
        int8_t ret = 0;
        int sig = 0;
        if ((sig = corto_proc_cmd(envcmd, &ret)) || ret) {
            if (!sig) {
                corto_throw("command returned %d", ret);
                corto_throw_detail("%s", envcmd);
            } else {
                corto_throw("command exited with signal %d", sig);
                corto_throw_detail("%s", envcmd);
            }

            bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
            p->error = true;
        }
        free(envcmd);
    }
}

static
void* bake_node_add(
    bake_language *l,
    void *n) /* void* to prevent excessive upcasting */
{
    corto_ll_append(l->nodes, n);
    return n;
}

static
int16_t bake_node_addDependencies(
    bake_language *l,
    bake_node *node,
    const char *pattern)
{
    char *str = corto_strdup(pattern);
    const char *ptr = strtok(str, ",");
    while (ptr) {
        if (ptr[0] == '$') {
            /* Create dependency to named node */
            bake_node *dep = bake_node_find(l, &ptr[1]);
            if (!dep) {
                corto_throw("dependency '%s' not found for rule '%s'",
                    ptr, node->name);
                goto error;
            } else {
                if (!node->deps) node->deps = corto_ll_new();
                corto_ll_append(node->deps, dep);
            }
        } else {
            /* Create dependency to anonymous pattern */
            bake_pattern *pattern = bake_pattern_new(NULL, ptr);
            if (!node->deps) node->deps = corto_ll_new();
            corto_ll_append(node->deps, pattern);
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
    bake_language *l,
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
        char *dup = corto_strdup(pattern);
        char *tok = strtok(dup, ",");
        while (tok) {
            if (tok[0] != '$') {
                corto_throw("target '%s' for rule '%s' does not refer named node",
                    pattern, node->name);
                goto error;
            }

            bake_node *targetNode = bake_node_find(l, &tok[1]);
            if (!targetNode) {
                corto_throw("unresolved target '%s' for node '%s'",
                    tok, node->name);
                goto error;
            }

            if (!targetNode->deps) targetNode->deps = corto_ll_new();
            corto_ll_append(targetNode->deps, node);
            tok = strtok(NULL, ",");
        }
        free(dup);
    }

    return 0;
error:
    return -1;
}

void bake_language_pattern(
    bake_language *l,
    const char *name,
    const char *pattern)
{
    bake_node *n;
    if ((n = bake_node_find(l, name))) {
        if (n->kind != BAKE_RULE_PATTERN) {
            l->error = 1;
            corto_error("'%s' redeclared as pattern", name);
        } else {
            ((bake_pattern*)n)->pattern = pattern;
        }

    } else {
        bake_node_add(l, bake_pattern_new(name, pattern));
    }
}

void bake_language_rule(
    bake_language *l,
    const char *name,
    const char *source,
    bake_rule_target target,
    bake_rule_action_cb action)
{
    bake_node *n;
    if (!source && target.kind == BAKE_RULE_TARGET_MAP) {
        l->error = 1;
        corto_error("rule '%s' has mapped target but no source to map from", name);
    } else if ((n = bake_node_find(l, name))) {
        if (n->kind != BAKE_RULE_RULE) {
            l->error = 1;
            corto_error("'%s' redeclared as rule", name);
        } else {
            ((bake_rule*)n)->source = source;
            ((bake_rule*)n)->target = target;
            ((bake_rule*)n)->action = action;
        }
    } else {
        bake_node *n = bake_node_add(l, bake_rule_new(name, source, target, action));
        if (bake_node_addDependencies(l, n, source)) {
            corto_throw(NULL);
            l->error = 1;
        }
        if (bake_node_addToTarget(l, n, &target)) {
            corto_throw(NULL);
            l->error = 1;
        }
    }
}

void bake_language_dependency_rule(
    bake_language *l,
    const char *name,
    const char *deps,
    bake_rule_target dep_mapping,
    bake_rule_action_cb action)
{
    if (bake_node_find(l, name)) {
        l->error = 1;
        corto_error("rule '%s' redeclared with dependencies = '%s'", name, deps);
    } else {
        bake_node_add(l, bake_dependency_rule_new(name, deps, dep_mapping, action));
    }
}

static
int16_t bake_assertPathForFile(
    char *path)
{
    char buffer[512];
    strcpy(buffer, path);
    char *ptr = strrchr(buffer, '/');
    if (ptr) {
        ptr[0] = '\0';
    }

    if (!corto_file_test(buffer)) {
        if (corto_mkdir(buffer)) {
            corto_throw(NULL);
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

static
bake_filelist* bake_node_eval_pattern(
    bake_node *n,
    bake_project *p)
{
    bool isSources = false;
    bake_filelist *targets = NULL;

    corto_trace("evaluating pattern");
    if (n->name && !stricmp(n->name, "SOURCES")) {
        targets = bake_filelist_new(NULL, NULL); /* Create empty list */
        isSources = true;

        /* If this is the special SOURCES rule, apply the pattern to
         * every configured source directory */
        corto_iter it = corto_ll_iter(p->sources);
        corto_dirstack ds = NULL;
        while (corto_iter_hasNext(&it)) {
            char *src = corto_iter_next(&it);
            if (!(ds = corto_dirstack_push(ds, src))) {
                corto_throw(NULL);
                goto error;
            }

            if (bake_filelist_addPattern(targets, src, ((bake_pattern*)n)->pattern)) {
                corto_throw(NULL);
                goto error;
            }
            corto_dirstack_pop(ds);
        }
    } else if (n->name && !stricmp(n->name, "MODEL") && p->model) {
        targets = bake_filelist_new(NULL, NULL); /* Create empty list */
        if (!bake_filelist_add(targets, p->model)) {
            corto_throw(NULL);
            goto error;
        }
    } else if (((bake_pattern*)n)->pattern) {
        /* If this is a regular pattern, match against project directory */
        targets = bake_filelist_new(NULL, ((bake_pattern*)n)->pattern);
    }

    if (!targets) {
        goto error;
    }

    return targets;
error:
    return NULL;
}

static
int16_t bake_node_run_rule_map(
    bake_language *l,
    bake_project *p,
    bake_config *c,
    bake_rule *r,
    bake_filelist *inputs,
    bake_filelist *targets)
{
    corto_iter it = bake_filelist_iter(inputs);
    int count = 0;
    while (corto_iter_hasNext(&it)) {
        bake_file *src = corto_iter_next(&it);
        bake_file *dst = NULL;
        const char *map = r->target.is.map(l, p, src->name, NULL);
        if (!map) {
            corto_throw("failed to map file '%s'", src->name);
            goto error;
        }
        if (!(dst = bake_filelist_add(targets, map))) {
            corto_throw(NULL);
            goto error;
        }

        count ++;
        if (src->timestamp > dst->timestamp) {
            corto_log_overwrite(CORTO_OK, "#[green][#[white]%3d%%#[green]]#[white] %s",
                100 * count / bake_filelist_count(inputs),
                src->name);

            /* Make sure target directory exists */
            if (bake_assertPathForFile(dst->name)) {
                corto_throw(NULL);
                goto error;
            }

            /* Invoke action */
            char *srcPath = src->name;
            if (src->offset) {
                srcPath = corto_asprintf("%s/%s", src->offset, src->name);
            }
            r->action(l, p, c, srcPath, dst->name, NULL);
            if (srcPath != src->name) {
                free(srcPath);
            }

            /* Check if error flag was set */
            if (p->error) {
                corto_throw("command for task '%s' failed", src->name);
                goto error;
            } else {
                p->freshly_baked = true;
                p->changed = true;
            }

            /* Update target with latest timestamp */
            dst->timestamp = corto_lastmodified(dst->name);
        } else {
            corto_trace("#[grey][%3d%%] %s",
                100 * count / bake_filelist_count(inputs),
                src->name);
        }
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_node_run_rule_pattern(
    bake_language *l,
    bake_project *p,
    bake_config *c,
    bake_rule *r,
    bake_filelist *inputs,
    bake_filelist *targets,
    bool shouldBuild)
{
    /* Do n-to-n comparison between sources and targets. If the
     * target list is empty, it is possible that files still have to
     * be generated, in which case the rule must be executed. */
    if (!shouldBuild) {
        if (!targets || !bake_filelist_count(targets)) {
            shouldBuild = true;
            corto_trace("no targets found for rule '%s', rebuilding",
                ((bake_node*)r)->name);
        } else {
            corto_iter src_iter = bake_filelist_iter(inputs);
            while (!shouldBuild && corto_iter_hasNext(&src_iter)) {
                bake_file *src = corto_iter_next(&src_iter);

                corto_iter dst_iter = bake_filelist_iter(targets);
                while (!shouldBuild && corto_iter_hasNext(&dst_iter)) {
                    bake_file *dst = corto_iter_next(&dst_iter);
                    if (!src->timestamp) {
                        shouldBuild = true;
                        corto_trace("'%s' does not exist for '%s', rebuilding",
                            dst->name,
                            ((bake_node*)r)->name);
                    } else if (src->timestamp > dst->timestamp) {
                        shouldBuild = true;
                        corto_trace("'%s' is newer than '%s', rebuilding",
                            src->name,
                            dst->name);
                    }
                }
            }
        }
    }

    char *dst = NULL;
    if (bake_filelist_count(targets) == 1) {
        bake_file *f = corto_ll_get(targets->files, 0);
        dst = f->name;
    }

    if (shouldBuild && inputs && bake_filelist_count(inputs)) {
        corto_buffer source_list = CORTO_BUFFER_INIT;
        corto_iter src_iter = bake_filelist_iter(inputs);
        int count = 0;
        while (corto_iter_hasNext(&src_iter)) {
            bake_file *src = corto_iter_next(&src_iter);
            if (count) {
                corto_buffer_appendstr(&source_list, " ");
            }
            corto_buffer_appendstr(&source_list, src->name);
            count ++;
        }

        char *source_list_str = corto_buffer_str(&source_list);

        if (dst) {
            corto_ok("#[bold]%s#[normal]", dst);
        } else {
            corto_ok("from #[bold]%s#[normal]", source_list_str);
        }

        r->action(l, p, c, source_list_str, dst, NULL);
        if (p->error) {
            if (dst) {
                corto_throw("command for task '%s' failed", dst);
            } else {
                corto_throw("rule failed");
            }
            free(source_list_str);
            corto_throw(NULL);
            goto error;
        } else {
            p->freshly_baked = true;
            p->changed = true;
        }

        free(source_list_str);
    } else if (dst) {
        corto_trace("#[grey]%s", dst);
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_node_eval(
    bake_language *l,
    bake_node *n,
    bake_project *p,
    bake_config *c,
    bake_filelist *inherits,
    bake_filelist *outputs)
{
    bake_filelist *targets = NULL, *inputs = NULL;

    if (n->cond && !n->cond(p)) {
        return 0;
    }

    corto_log_push((char*)n->name);

    if (n->kind == BAKE_RULE_PATTERN) {
        targets = bake_node_eval_pattern(n, p);
        if (!targets) {
            targets = inherits;
        }
    } else {
        corto_trace("evaluating rule");
        targets = inherits;
    }

    /* Collect input files for node */
    if (n->deps) {
        bake_filelist *inputs = bake_filelist_new(NULL, NULL);
        if (!inputs) {
            corto_throw(NULL);
            goto error;
        }

        /* Evaluate dependencies of node & collect its inputs */
        corto_iter it = corto_ll_iter(n->deps);
        while (corto_iter_hasNext(&it)) {
            bake_node *e = corto_iter_next(&it);
            if (bake_node_eval(l, e, p, c, targets, inputs)) {
                corto_throw("dependency '%s' failed", e->name);
                goto error;
            }
        }

        /* Generate target files */
        if (n->kind == BAKE_RULE_RULE) {
            bake_rule *r = (bake_rule*)n;

            /* When rule specifies a map, generate targets from inputs */
            if (r->target.kind == BAKE_RULE_TARGET_MAP) {
                targets = bake_filelist_new(NULL, NULL);
                if (!targets) {
                    corto_throw(NULL);
                    goto error;
                }
                if (bake_node_run_rule_map(l, p, c, r, inputs, targets)) {
                    corto_throw(NULL);
                    goto error;
                }

            /* When rule specifies a pattern, generate targets from pattern */
            } else if (r->target.kind == BAKE_RULE_TARGET_PATTERN) {
                bool shouldBuild = false;

                if (!r->target.is.pattern || (r->target.is.pattern[0] == '$' && inherits)) {
                    targets = inherits;
                } else {
                    char *pattern = corto_strdup(r->target.is.pattern);
                    targets = bake_filelist_new(NULL, NULL);

                    char *tok = strtok(pattern, ",");
                    while (tok) {
                        bake_node *targetNode = bake_node_find(l, &tok[1]);
                        if (!targetNode->cond || targetNode->cond(p)) {
                            bake_filelist *list = bake_filelist_new(
                                NULL, ((bake_pattern*)targetNode)->pattern);
                            if (!list || !bake_filelist_count(list)) {
                                corto_trace("no targets matched by '%s', need to rebuild '%s'",
                                    tok,
                                    n->name);
                                shouldBuild = true;
                            } else {
                                bake_filelist_addList(targets, list);
                                bake_filelist_free(list);
                            }
                        }
                        tok = strtok(NULL, ",");
                    }
                    free(pattern);
                }

                if (!targets && !bake_filelist_count(inputs)) {
                    corto_throw("no targets for rule");
                    goto error;
                }

                if (!targets) {
                    targets = bake_filelist_new(NULL, NULL);
                }

                if (bake_node_run_rule_pattern(l, p, c, r, inputs, targets, shouldBuild)) {
                    corto_throw(NULL);
                    goto error;
                }
            }
        }
    }

    /* Add targets to list of outputs (inputs for parent node) */
    if (outputs && targets) {
        bake_filelist_addList(outputs, targets);
    }

    corto_trace("done");
    corto_log_pop();

    return 0;
error:
    corto_log_pop();
    return -1;
}

int16_t bake_language_generate(
    bake_language *l,
    bake_project *p,
    bake_config *c)
{
    if (p->managed) {
        corto_log_push("generate");
        corto_trace("begin");

        bake_node *root = bake_node_find(l, "GENERATED-SOURCES");
        if (!root) {
            corto_critical("root GENERATED-SOURCES node not found in language object");
        }

        corto_tls_set(BAKE_PROJECT_KEY, p);

        if (bake_node_eval(l, root, p, c, NULL, NULL)) {
            corto_log_pop();
            goto error;
        }

        corto_trace("end");
        corto_log_pop();
    }

    return 0;
error:
    return -1;
}

int16_t bake_language_build(
    bake_language *l,
    bake_project *p,
    bake_config *c)
{
    char *artefact = NULL;
    char *artefact_path = NULL;

    corto_log_push("build");
    corto_trace("begin");

    /* If code generation yielded a folder with the name of the
     * project language, this is a new project that contains the
     * generated language binding api. */
    if (p->use_generated_api) {
        int sig;
        int8_t ret;
        if (corto_file_test("c") == 1) {
            if ((sig = corto_proc_cmd("bake build c", &ret)) || ret) {
                corto_throw(NULL);
                goto error;
            }
        }
        if (corto_file_test("cpp") == 1) {
            if ((sig = corto_proc_cmd("bake build cpp", &ret)) || ret) {
                corto_throw(NULL);
                goto error;
            }
        }
    }

    /* Add dependencies to link list */
    corto_iter it = corto_ll_iter(p->use);
    while (corto_iter_hasNext(&it)) {
        char *dep = corto_iter_next(&it);

        const char *libpath = corto_locate(dep, NULL, CORTO_LOCATE_PACKAGE);
        if (!libpath) {
            corto_throw(
                "failed to locate library path for dependency '%s'", dep);
            goto error;
        }

        const char *lib = corto_locate(dep, NULL, CORTO_LOCATE_LIB);
        if (lib) {
            corto_ll_append(p->link, corto_strdup(lib));
        } else {
            /* A dependency may not have a library that can be linked, but could
             * only contain build instructions */
            corto_catch();
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
    }

    /* If project is managed, add corto library to link */
    if (p->managed) {
        const char *cortolib = corto_locate("corto", NULL, CORTO_LOCATE_LIB);
        if (!cortolib) {
            goto error;
        }
        corto_ll_append(p->link, corto_strdup(cortolib));
    }

    bake_node *root = bake_node_find(l, "ARTEFACT");
    if (!root) {
        corto_critical("root ARTEFACT node not found in language object");
    }

    /* Obtain artefact */
    artefact = l->artefact_cb(l, p);
    if (!artefact) {
        corto_throw("no artefacts specified for project '%s' by language", p->id);
        goto error;
    }

    artefact_path = corto_asprintf("bin/%s-%s", CORTO_PLATFORM_STRING, c->id);

    if (corto_mkdir(artefact_path)) {
        corto_throw(NULL);
        goto error;
    }

    corto_tls_set(BAKE_PROJECT_KEY, p);

    /* Evaluate root node */
    char *binaryPath = bake_project_binaryPath(p);
    bake_filelist *artefact_fl = bake_filelist_new(
        binaryPath,
        NULL
    );
    bake_filelist_add(artefact_fl, strarg("%s/%s", artefact_path, artefact));
    if (bake_node_eval(l, root, p, c, artefact_fl, NULL)) {
        corto_throw("failed to build 'ARTEFACT'");
        goto error;
    }
    bake_filelist_free(artefact_fl);

    free(artefact_path);
    free(artefact);
    free(binaryPath);

    corto_log_pop();
    return 0;
error:
    if (artefact) free(artefact);
    if (artefact_path) free(artefact_path);
    corto_log_pop();
    return -1;
}

int16_t bake_language_init(
    bake_language *l,
    bake_project *p)
{
    if (l->init_cb) {
        corto_tls_set(BAKE_PROJECT_KEY, p);
        l->init_cb(p);
    }
    return 0;
}

int16_t bake_language_clean(
    bake_language *l,
    bake_project *p)
{
    char *artefact = NULL;
    corto_log_push("clean");
    corto_trace("begin");

    corto_tls_set(BAKE_PROJECT_KEY, p);

    /* Clear .bake_cache directory which contains object files / generated files */
    if (corto_rm(".bake_cache")) {
        goto error;
    }

    /* Clear bin directory which contains the artefact */
    if (corto_rm("bin")) {
        goto error;
    }

    /* Clear .corto directory (for legacy projects) */
    if (corto_rm(".corto")) {
        goto error;
    }

    /* If project is managed, remove projects that contain generated
     * language-specific code. */
    if (p->managed && p->language && corto_file_test(p->language) == 1) {
        /* Make sure removed directories are corto projects */
        if (corto_file_test("c/project.json")) {
            corto_rm("c");
        }
        if (corto_file_test("cpp/project.json")) {
            corto_rm("cpp");
        }
    }

    /* If language binding registered callback to specify additional files
     * to clean, call callback & walk over files to clean */
    if (l->clean) {
        l->clean_cb(l, p);

        /* Clean files marked by the language binding */
        corto_iter it = corto_ll_iter(p->files_to_clean);
        while (corto_iter_hasNext(&it)) {
            char *file = corto_iter_next(&it);
            corto_rm(file);
        }
    }

    p->changed = true;

    corto_trace("done");
    corto_log_pop();

    return 0;
error:
    if (artefact) free(artefact);
    return -1;
}

char* bake_language_artefact(
    bake_language *l,
    bake_project*p)
{
    corto_assert(l != NULL, "no language specified for bake_language_artefact");
    corto_assert(p != NULL, "no project specified for bake_language_artefact");
    return l->artefact_cb(l, p);
}

bake_language* bake_language_get(
    const char *language)
{
    bool found = false;
    bake_language *l = NULL;
    char *package = corto_asprintf("driver/bake/%s", language);

    if (!languages) {
        languages = corto_ll_new();
    }

    /* Check if language is already loaded */
    corto_iter it = corto_ll_iter(languages);
    while (corto_iter_hasNext(&it) && !l) {
        bake_language *e = corto_iter_next(&it);
        if (!strcmp(e->package, package)) {
            l = e;
        }
    }

    if (!l) {
        l = corto_calloc(sizeof(bake_language));

        /* Set callbacks for populating rules */
        l->pattern = bake_language_pattern_cb;
        l->rule = bake_language_rule_cb;
        l->dependency_rule = bake_language_dependency_rule_cb;
        l->condition = bake_language_condition_cb;
        l->target_pattern = bake_language_target_pattern_cb;
        l->target_map = bake_language_target_map_cb;
        l->init = bake_language_init_cb;
        l->artefact = bake_language_artefact_cb;
        l->clean = bake_language_clean_cb;
        l->exec = bake_language_exec_cb;

        l->nodes = corto_ll_new();
        l->error = 0;

        corto_dl dl = NULL;
        buildmain_cb _main = corto_load_sym(package, &dl, "bakemain");

        if (!_main) {
            corto_throw("failed load '%s'",
                package);
            goto error;
        }
        l->dl = dl;

        /* Set language object in tls so callbacks can retrieve the language
         * object without having to explicitly specify it. */
        corto_tls_set(BAKE_LANGUAGE_KEY, l);

        /* Create built-in nodes */
        bake_language_pattern(l, "MODEL", NULL); /* Code-gen source */

        /* Run 'bakemain' which will load the rules for the language */
        if (_main(l)) {
            corto_throw("bakemain for '%s' failed",
                language);
            free(l);
            goto error;
        }

        if (l->error) {
            corto_throw(NULL);
            goto error;
        }

        l->name = corto_strdup(language);
        l->package = corto_strdup(package);

        corto_ll_append(languages, l);
    }

    corto_ll_append(languages, l);

    if (package) free(package);
    return l;
error:
    if (package) free(package);
    return NULL;
}
