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

/** @file
 * @section rule Types used for specifying bake rules.
 * @brief Rules generate a sequence of build actions for a given set of files.
 */

#ifndef BAKE_RULE_H_
#define BAKE_RULE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bake_language bake_language;

typedef void (*bake_rule_action_cb)(bake_language *l, bake_project *p, bake_config *c, char *src, char *target, void *ctx);
typedef char* (*bake_rule_map_cb)(bake_language *l, bake_project *p, const char *input, void *ctx);
typedef char* (*bake_rule_artefact_cb)(bake_language *l, bake_project *p);

/* Bake target is a convenience type wrapped by functions that lets users
 * specify different kinds of targets as argument type. */
typedef enum bake_rule_targetKind {
    BAKE_RULE_TARGET_MAP,
    BAKE_RULE_TARGET_PATTERN
} bake_rule_targetKind;

typedef enum bake_rule_kind {
    BAKE_RULE_PATTERN,
    BAKE_RULE_RULE
} bake_rule_kind;

typedef struct bake_rule_target {
    bake_rule_targetKind kind;
    union {
        bake_rule_map_cb map;
        const char *pattern;
    } is;
} bake_rule_target;

typedef struct bake_node {
    bake_rule_kind kind;
    const char *name;
    corto_ll deps;
} bake_node;

typedef struct bake_pattern {
    bake_node super;
    const char *pattern;
} bake_pattern;

typedef struct bake_rule {
    bake_node super;
    const char *source;
    bake_rule_target target;
    bake_rule_action_cb action;
} bake_rule;

typedef struct bake_dependency_rule {
    bake_node super;
    bake_rule_target target;
    const char *deps;
    bake_rule_action_cb action;
} bake_dependency_rule;

bake_pattern* bake_pattern_new(
    const char *name, 
    const char *pattern);

bake_rule* bake_rule_new(
    const char *name, 
    const char *source, 
    bake_rule_target target, 
    bake_rule_action_cb action);

bake_dependency_rule* bake_dependency_rule_new(
    const char *name, 
    const char *deps, 
    bake_rule_target dep_mapping, 
    bake_rule_action_cb action);

#ifdef __cplusplus
}
#endif

#endif
