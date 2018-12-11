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

#ifndef BAKE_DRIVER_API_H_
#define BAKE_DRIVER_API_H_

typedef struct bake_driver_api bake_driver_api;

/** Generic callback */
typedef
void (*bake_driver_cb)(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project);

/** Artefact callback */
typedef
char* (*bake_artefact_cb)(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project);

/** link_to_lib callback */
typedef
char* (*bake_link_to_lib_cb)(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *link);

/** Condition callback */
typedef
bool (*bake_condition_cb)(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project);

/** Action rule callback */
typedef
void (*bake_rule_action_cb)(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *src,
    char *target);

/** Map rule callback */
typedef
char* (*bake_rule_map_cb)(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *input);

/** Project setup callback */
typedef
int16_t (*bake_setup_cb)(
    bake_driver_api *driver,
    bake_config *config,
    const char *id,
    bake_project_type kind);


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

struct bake_driver_api {
    /* Create a pattern */
    void (*pattern)(
        const char *name,
        const char *pattern);

    /* Create a rule */
    void (*rule)(
        const char *name,
        const char *source,
        bake_rule_target target,
        bake_rule_action_cb action);

    /* Create a dependency rule */
    void (*dependency_rule)(
        const char *name,
        const char *deps,
        bake_rule_target dep_mapping,
        bake_rule_action_cb action);

    /* Create a pattern target (n-to-1 rule) */
    bake_rule_target (*target_pattern)(
        const char *pattern);

    /* Create a map target (n-to-n rule) */
    bake_rule_target (*target_map)(
        bake_rule_map_cb mapping);

    /* Create a condition for a rule */
    void (*condition)(
        const char *name,
        bake_condition_cb cond);

    /* Initialize a project before a build */
    void (*init)(
        bake_driver_cb action);

    /* Return the name of the artefact for a project */
    void (*artefact)(
        bake_artefact_cb action);

    /* Locate library file based on a link path */
    void (*link_to_lib)(
        bake_link_to_lib_cb action);

    /* Create a new project */
    void (*setup)(
        bake_setup_cb action);

    /* Clean a project */
    void (*clean)(
        bake_driver_cb action);

    /* Execute a command */
    void (*exec)(
        const char *cmd);

    /* Get a driver-specific attribute */
    bake_attribute* (*get_attr)(
        const char *name);

    /* Get a driver-specific attribute */
    bool (*get_attr_bool)(
        const char *name);
};

#endif
