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
 * @section builder Builder framework for bake.
 * @brief The builder provides an interface for building bake plugins.
 */

#ifndef BAKE_RULE_H_
#define BAKE_RULE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t (*bake_rule_action_cb)(bake_project *p, const char *input, void *ctx);
typedef char* (*bake_rule_map_cb)(bake_project *p, const char *input, void *ctx);

/* Bake target is a convenience type wrapped by functions that lets users
 * specify different kinds of targets as argument type. */
typedef enum bake_rule_targetKind {
    BAKE_RULE_TARGET_ONE,
    BAKE_RULE_TARGET_N,
    BAKE_RULE_TARGET_PATTERN
} bake_rule_targetKind;

typedef struct bake_rule_target {
    bake_rule_targetKind kind;
    union {
        bake_rule_map_cb n;
        const char *pattern;
        const char *one;
    } is;
} bake_rule_target;

typedef struct bake_node {
    char *name;
} bake_node;

typedef struct bake_pattern {
    bake_node super;
    char *pattern;
} bake_pattern;

typedef struct bake_rule {
    bake_node super;
    char *source;
    bake_rule_target *target;
    bake_rule_action_cb action;
} bake_rule;

typedef struct bake_dependency_rule {
    char *target;
    bake_rule_target dependencies;
    bake_rule_map_cb dep_mapping;
    bake_rule_action_cb action;
} bake_dependency_rule;


#ifdef __cplusplus
}
#endif

#endif
