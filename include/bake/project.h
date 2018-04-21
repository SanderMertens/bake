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

/** @file
 * @section project Project API
 * @brief API that exposes details about a project.
 */

#ifndef BAKE_PROJECT_H_
#define BAKE_PROJECT_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bake_project_kind {
    BAKE_APPLICATION,
    BAKE_PACKAGE,
    BAKE_TOOL
} bake_project_kind;

typedef enum bake_project_attrKind {
    BAKE_ATTR_BOOLEAN,
    BAKE_ATTR_STRING,
    BAKE_ATTR_NUMBER,
    BAKE_ATTR_ARRAY
} bake_project_attrKind;

typedef struct bake_project_attr {
    char *name;
    bake_project_attrKind kind;
    union {
        bool boolean;
        char *string;
        double number;
        corto_ll array;
    } is;
} bake_project_attr;

typedef struct bake_project {
    /* Static properties (managed by project) */
    char *id;
    bake_project_kind kind;
    corto_ll use;
    corto_ll use_private; /* packages that are not exposed to dependees */
    corto_ll use_build; /* packages required at compile time, but not runtime */
    corto_ll link; /* All objects package needs to link with (contains located dependencies) */
    bool public;
    bool managed;
    bool use_generated_api;
    char *path;
    corto_ll sources;
    corto_ll includes;
    char *language;
    char *args;
    char *version;
    bake_config *cfg;
    corto_ll attributes;

    /* Project model file */
    char *model;

    /* Runtime status (managed by language binding) */
    bool error;
    bool freshly_baked;
    bool changed;

    /* Should project be rebuilt (managed by bake action) */
    bool artefact_outdated;
    bool sources_outdated;

    /* Dependency administration (managed by crawler) */
    int unresolved_dependencies; /* number of dependencies still to be built */
    corto_ll dependents; /* projects that depend on this project */
    bool built;

    /* Files to be cleaned other than objects and artefact (populated by
     * language binding) */
    corto_ll files_to_clean;

    /* JSON that contains build instructions for dependee projects */
    char *dependee_json;

    /* Pointer to parsed JSON value and JSON value member from project config */
    void *json;
    void *value_json;

    /* Interface for bake plugin */
    bake_project_attr* (*get_attr)(const char *name);
    char* (*get_attr_string)(const char *name);
    void (*add_build_dependency)(const char *file);
    void (*clean)(const char *file);
} bake_project;

#ifdef __cplusplus
}
#endif

#endif
