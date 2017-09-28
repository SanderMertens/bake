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
bake_rule_target bake_language_target_1_cb(
    const char *target) 
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_ONE;
    result.is.one = target;
    return result;
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
bake_rule_target bake_language_target_n_cb(
    bake_rule_map_cb mapping) 
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_N;
    result.is.n = mapping;
    return result;
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
    if (target->kind == BAKE_RULE_TARGET_ONE) {
        corto_assert(
            target->is.one != NULL, "invalid rule for rule '%s'", node->name);
        pattern = target->is.one;
    } else if (target->kind == BAKE_RULE_TARGET_PATTERN) {
        corto_assert(
            target->is.pattern != NULL, "invalid rule for rule '%s'", node->name);
        pattern = target->is.pattern;
    }

    /* If target specifies n targets, target is dynamic and there is no node
     * representing the target. */
    if (pattern) {
        if (pattern[0] != '$') {
            corto_seterr("target '%s' for rule '%s' does not refer named node",
                target->is.one, node->name);
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
    if (bake_node_find(l, name)) {
        l->error = 1;
        corto_error("pattern '%s' redeclared with value '%s'", name, pattern);
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
    if (bake_node_find(l, name)) {
        l->error = 1;
        corto_error("rule '%s' redeclared with source = '%s'", name, source);
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
int16_t bake_node_walkDependencies(
    bake_language *l,
    bake_node *n,
    bake_project *p)
{
    corto_trace("evaluating rule '%s'", n->name);
    corto_log_push((char*)n->name);
    corto_iter it = corto_ll_iter(n->deps);
    while (corto_iter_hasNext(&it)) {
        bake_node *e = corto_iter_next(&it);
        if (e->deps) {
            if (bake_node_walkDependencies(l, e, p)) {
                goto error;
            }
        }
    }
    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
}

int16_t bake_language_build(
    bake_language *l,
    bake_project *p)
{
    corto_log_push("build");
    corto_trace("begin");
    bake_node *root = bake_node_find(l, "ARTEFACT");
    if (!root) {
        corto_critical("root ARTEFACT node not found in language object");
    }

    if (root->deps) {
        if (bake_node_walkDependencies(l, root, p)) {
            goto error;
        }
    } else {
        /* no dependencies for artefact? nothing needs to be built. */
    }

    corto_trace("end");
    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
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
        l->target_1 = bake_language_target_1_cb;
        l->target_pattern = bake_language_target_pattern_cb;
        l->target_n = bake_language_target_n_cb;

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
        bake_language_pattern(l, "ARTEFACT", NULL); /* Build root */
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
