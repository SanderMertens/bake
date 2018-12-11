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

#ifndef BAKE_DRIVER_H_
#define BAKE_DRIVER_H_

typedef int (*bake_driver_cb)(
    bake_config *config,
    bake_project *project,
    ut_ll attributes);

typedef struct bake_driver_impl {
    bake_driver_cb init;
    bake_driver_cb artefact;
    bake_driver_cb setup;
    bake_driver_cb clean;
} bake_driver_impl;

typedef struct bake_driver_api {
    /* API for defining dependency rules */
    void (*pattern)(
        const char *name,
        const char *pattern);

    void (*rule)(
        const char *name,
        const char *source,
        bake_rule_target target,
        bake_rule_action_cb action);

    void (*dependency_rule)(
        const char *name,
        const char *deps,
        bake_rule_target dep_mapping,
        bake_rule_action_cb action);

    bake_rule_target (*target_pattern)(
        const char *pattern);

    bake_rule_target (*target_map)(
        bake_rule_map_cb mapping);

    void (*condition)(
        const char *name,
        bake_rule_condition_cb cond);

    /* API for initializing / creating / cleaning projects */
    void (*init)(
        bake_driver_cb action);

    void (*artefact)(
        bake_driver_cb action);

    void (*setup)(
        bake_driver_cb action);

    void (*clean)(
        bake_driver_cb action);

    /* API for executing a command */
    void (*exec)(
        const char *cmd);
} bake_driver_api;

struct bake_driver {
    char *id;
    char *package_id;
    ut_dl dl;

    /* Dependency rules */
    ut_ll nodes;

    /* True if error occured */
    int error;

    /* Callbacks set by the driver */
    bake_driver_impl impl;

    /* API provided by bake to the driver */
    bake_driver_api api;
};

#endif
