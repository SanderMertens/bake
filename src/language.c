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

static corto_ll languages;

extern corto_tls BAKE_LANGUAGE_KEY;
extern corto_tls BAKE_FILELIST_KEY;
extern corto_tls BAKE_PROJECT_KEY;

typedef int (*buildmain_cb)(bake_language *l);

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
void bake_language_artefact_cb(
    bake_rule_artefact_cb artefact) 
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    l->artefact_cb = artefact;
}

static
void bake_language_exec_cb(
    const char *cmd)
{
    char *envcmd = corto_envparse("%s", cmd);

    int8_t ret = 0;
    int sig = 0;
    if ((sig = corto_proc_cmd(envcmd, &ret)) || ret) {
        if (!sig) {
            corto_seterr("command failed with returncode %d: %s", ret, envcmd);
        } else {
            corto_seterr("command failed with signal %d: %s", sig, envcmd);
        }

        bake_project *p = corto_tls_get(BAKE_PROJECT_KEY);
        p->error = true;
    }

    free(envcmd);
}

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
                corto_seterr("dependency '%s' not found for rule '%s'",
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
        if (pattern[0] != '$') {
            corto_seterr("target '%s' for rule '%s' does not refer named node",
                pattern, node->name);
            goto error;
        }

        bake_node *targetNode = bake_node_find(l, &pattern[1]);
        if (!targetNode) {
            corto_seterr("unresolved target '%s' for node '%s'",
                pattern, node->name);
            goto error;
        }

        if (!targetNode->deps) targetNode->deps = corto_ll_new();
        corto_ll_append(targetNode->deps, node);
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
        if (bake_node_addDependencies(l, n, source)) l->error = 1;
        if (bake_node_addToTarget(l, n, &target)) l->error = 1;
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
            goto error;
        }
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

    corto_log_push((char*)n->name);

    if (n->kind == BAKE_RULE_PATTERN) {
        if (((bake_pattern*)n)->pattern) {
            bool isSources = false;

            corto_trace("evaluating pattern");
            if (!stricmp(n->name, "SOURCES")) {
                targets = bake_filelist_new(NULL, NULL); /* Create empty list */
                isSources = true;

                /* If this is the special SOURCES rule, apply the pattern to
                 * every configured source directory */
                corto_iter it = corto_ll_iter(p->sources);
                corto_dirstack ds = NULL;
                while (corto_iter_hasNext(&it)) {
                    char *src = corto_iter_next(&it);
                    ds = corto_dirstack_push(ds, src);
                    if (bake_filelist_addPattern(targets, src, ((bake_pattern*)n)->pattern)) {
                        goto error;
                    }
                    corto_dirstack_pop(ds);
                }
            } else {
                /* If this is a regular pattern, match against project directory */
                targets = bake_filelist_new(NULL, ((bake_pattern*)n)->pattern);
            }

            if (!targets) {
                goto error;
            }
        } else {
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
            goto error;
        }

        corto_iter it = corto_ll_iter(n->deps);
        while (corto_iter_hasNext(&it)) {
            bake_node *e = corto_iter_next(&it);
            if (bake_node_eval(l, e, p, c, targets, inputs)) {
                corto_seterr("dependency '%s' failed", e->name);
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
                    goto error;
                }
                corto_iter it = bake_filelist_iter(inputs);
                int count = 0;
                while (corto_iter_hasNext(&it)) {
                    bake_file *src = corto_iter_next(&it);
                    bake_file *dst = NULL;
                    const char *map = r->target.is.map(l, p, src->name, NULL);
                    if (!map) {
                        corto_seterr("failed to map file '%s'", src->name);
                        goto error;
                    }
                    if (!(dst = bake_filelist_add(targets, map))) {
                        goto error;
                    }

                    count ++;
                    if (src->timestamp > dst->timestamp) {
                        corto_info("%s[%s%3d%%%s] %s", 
                            CORTO_NORMAL,
                            CORTO_GREY,
                            100 * count / bake_filelist_count(inputs),
                            CORTO_NORMAL,
                            src->name);

                        /* Make sure target directory exists */
                        if (bake_assertPathForFile(dst->name)) {
                            goto error;
                        }

                        char *srcPath = src->name;
                        if (src->offset) {
                            srcPath = corto_asprintf("%s/%s", src->offset, src->name);
                        }

                        r->action(l, p, c, srcPath, dst->name, NULL);

                        if (srcPath != src->name) {
                            free(srcPath);
                        }

                        if (p->error) {
                            corto_seterr("'%s' failed to build", src->name);
                            goto error;
                        }
                    } else {
                        corto_ok("[%s%3d%%%s] %s%s%s", 
                            CORTO_GREY,
                            100 * count / bake_filelist_count(inputs),
                            CORTO_NORMAL,
                            CORTO_GREEN,
                            src->name,
                            CORTO_NORMAL);
                    }
                }

            /* When rule specifies a pattern, generate targets from pattern */
            } else if (r->target.kind == BAKE_RULE_TARGET_PATTERN) {
                if (!r->target.is.pattern || r->target.is.pattern[0] == '$') {
                    targets = inherits;
                } else {
                    targets = bake_filelist_new(NULL, r->target.is.pattern);
                }
                if (!targets) {
                    goto error;
                }                    

                /* Do n-to-n comparison between sources and targets. If the
                 * target list is empty, it is possible that files still have to
                 * be generated, in which case the rule must be executed. */
                bool shouldBuild = false;

                if (!bake_filelist_count(targets)) {
                    shouldBuild = true;
                } else {
                    corto_iter src_iter = bake_filelist_iter(inputs);
                    while (!shouldBuild && corto_iter_hasNext(&src_iter)) {
                        bake_file *src = corto_iter_next(&src_iter);

                        corto_iter dst_iter = bake_filelist_iter(targets);
                        while (!shouldBuild && corto_iter_hasNext(&dst_iter)) {
                            bake_file *dst = corto_iter_next(&dst_iter);
                            if (!src->timestamp) {
                                shouldBuild = true;
                            } else if (src->timestamp > dst->timestamp) {
                                shouldBuild = true;
                            }
                        }
                    }
                }

                char *dst = NULL;
                if (bake_filelist_count(targets) == 1) {
                    bake_file *f = corto_ll_get(targets->files, 0);
                    dst = f->name;
                }

                if (shouldBuild) {
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
                    r->action(l, p, c, source_list_str, dst, NULL);
                    if (p->error) {
                        if (dst) {
                            corto_seterr("'%s' failed to build", dst);
                        } else {
                            corto_seterr("rule '%s' failed to build", n->name);
                        }
                        free(source_list_str);
                        goto error;
                    } else {
                        corto_info("%s", dst);
                    }

                    free(source_list_str);
                } else {
                    corto_info("%s%s%s", CORTO_GREEN, dst, CORTO_NORMAL);
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

bake_filelist* bake_language_build(
    bake_language *l,
    bake_project *p,
    bake_config *c)
{
    corto_log_push("build");
    corto_trace("begin");

    bake_node *root = bake_node_find(l, "ARTEFACT");
    if (!root) {
        corto_critical("root ARTEFACT node not found in language object");
    }

    /* Create filelist for artefact files */
    char *binaryPath = bake_project_binaryPath(p);
    bake_filelist *artefact_fl = bake_filelist_new(
        binaryPath,
        NULL
    );

    /* Populate filelist */
    corto_tls_set(BAKE_FILELIST_KEY, artefact_fl);
    l->artefact_cb(l, artefact_fl, p);

    if (!bake_filelist_count(artefact_fl)) {
        corto_seterr("no artefacts specified for project '%s' by language", p->id);
        goto error;
    }

    corto_tls_set(BAKE_PROJECT_KEY, p     );

    /* Evaluate root node */
    if (bake_node_eval(l, root, p, c, artefact_fl, NULL)) {
        corto_seterr("failed to build 'ARTEFACT'");
        goto error;
    }

    corto_trace("end");
    corto_log_pop();
    return artefact_fl;
error:
    corto_log_pop();
    return NULL;
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
        l->target_pattern = bake_language_target_pattern_cb;
        l->target_map = bake_language_target_map_cb;
        l->artefact = bake_language_artefact_cb;
        l->exec = bake_language_exec_cb;

        l->nodes = corto_ll_new();
        l->error = 0;

        corto_dl dl = NULL;
        buildmain_cb _main = corto_load_sym(package, &dl, "bakemain");

        if (!_main) {
            corto_seterr("failed load '%s': %s", 
                package,
                corto_lasterr());
            goto error;
        }
        l->dl = dl;

        /* Set language object in tls so callbacks can retrieve the language
         * object without having to explicitly specify it. */
        corto_tls_set(BAKE_LANGUAGE_KEY, l);

        /* Create built-in nodes */
        bake_language_pattern(l, "DEFINITION", NULL); /* Code-gen root */

        /* Run 'bakemain' which will load the rules for the language */
        if (_main(l)) {
            corto_seterr("bakemain for '%s' failed: %s", 
                language, 
                corto_lasterr());
            free(l);
            goto error;
        }

        if (l->error) {
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
