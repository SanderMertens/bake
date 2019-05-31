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

/** @file
 * @section driver_api Driver interface
 * @brief Interface that can be implemented by bake drivers.
 **/

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


/* Bake target is a convenience type wrapped by functions that lets users
 * specify different kinds of targets as argument type. */

/* A PATTERN is a list of files matched against a pattern (string expression
 * that contains wildcards). A MAP is a function that transforms input
 * files to output files (like foo.c => foo.o). A FILE returns a single file,
 * regardless of whether this file exists. */
typedef enum bake_rule_targetKind {
    BAKE_RULE_TARGET_MAP,
    BAKE_RULE_TARGET_PATTERN,
    BAKE_RULE_TARGET_FILE
} bake_rule_targetKind;

/* A rule target specifies the expected list of files as result of a rule. When
 * the rule target is a MAP, this file list will be determined by invoking the
 * map callback on the input file list.
 * If the target pattern is a PATTERN, the output list will be populated by the
 * pattern in the target. */
typedef struct bake_rule_target {
    bake_rule_targetKind kind; /* Is the target a MAP or a PATTERN */
    union {
        bake_rule_map_cb map; /* MAP callback */
        const char *pattern;  /* PATTERN expression */
    } is;
} bake_rule_target;

/* A driver can create rules and patterns. A pattern is a string expression that
 * contains wildcards, and matches a list of files. A rule is a construct that
 * has an input file list (a pattern) and an output file list (a rule target).
 * When an output of a rule is required by the build, bake will evaluate the
 * corresponding inputs. If the input files are newer than the output files, the
 * rule will be invoked.
 * When the target is a PATTERN, the rule will be invoked once for the target.
 * When the target is a MAP, the rule will be invoked for every input-output
 * file (typical example: a .c file and .o file). */
typedef enum bake_rule_kind {
    BAKE_RULE_PATTERN,
    BAKE_RULE_RULE,
    BAKE_RULE_FILE
} bake_rule_kind;

/* The driver API is a struct that is passed to the bakemain function, which is
 * invoked when the driver is loaded. This struct is prepopulated with functions
 * the driver can call to create patterns and rules.
 * Before the bakemain function of the driver is invoked, bake will set the
 * driver object (of type bake_driver) in TLS memory. This enables the driver to
 * invoke the callbacks without explicitly passing the driver, like this:
 *    driver->pattern("SOURCES", "*.c");
 * The 'pattern' callback will then retrieve the driver object from TLS, and
 * create the pattern with the driver object. */
struct bake_driver_api {
    /* Import definitions from another driver */
    void (*import)(
        const char *driver);

    /* Lookup another driver by name */
    bake_driver* (*lookup_driver)(
        const char *driver);
    
    /* Get pointer to current driver */
    bake_driver* (*current_driver)(void);

    /* Switch API to (temporarily) control another driver */
    bake_driver* (*set_driver)(
        bake_driver *driver);

    /* Create a pattern */
    void (*pattern)(
        const char *name,
        const char *pattern);

    /* Create a pattern */
    void (*file)(
        const char *name,
        const char *file);

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

    /* Create a file target (n-to-1 rule) */
    bake_rule_target (*target_file)(
        const char *file);        

    /* Create a map target (n-to-n rule) */
    bake_rule_target (*target_map)(
        bake_rule_map_cb mapping);

    /* Create a condition for a rule */
    void (*condition)(
        const char *name,
        bake_condition_cb cond);

    /* Callback to initialize a driver before building the project */
    void (*init)(
        bake_driver_cb action);

    /* Callback that returns name of artefact */
    void (*artefact)(
        bake_artefact_cb action);

    /* Callback that locates library file based on logical package id */
    void (*link_to_lib)(
        bake_link_to_lib_cb action);

    /* Callback for initializing a new project in a directory */
    void (*setup)(
        bake_driver_cb action);

    /* Callback for generating code */
    void (*generate)(
        bake_driver_cb action);

    /* Callback called before build */
    void (*prebuild)(
        bake_driver_cb action);

    /* Callback called at build stage */
    void (*build)(
        bake_driver_cb action);        

    /* Callback called after build */
    void (*postbuild)(
        bake_driver_cb action);

    /* Callback called before test */
    void (*test)(
        bake_driver_cb action);

    /* Callback called for doing coverage analysis */
    void (*coverage)(
        bake_driver_cb action);

    /* Callback for specifying files to clean */
    void (*clean)(
        bake_driver_cb action);

    /* Mark a file to be removed when project is cleaned */
    void (*remove)(
        const char *file);

    /* Execute a command */
    void (*exec)(
        const char *cmd);

    /* Add dependency */
    void (*use)(
        const char *id);

    /* Does package exist */
    bool (*exists)(
        const char *id);

    /* Lookup package */
    bake_project* (*lookup)(
        const char *id);

    /* Add ignore path for project */
    void (*ignore_path)(
        const char *path);

    /* Get a driver-specific attribute */
    bake_attr* (*get_attr)(
        const char *name);

    /* Get a driver-specific boolean attribute */
    bool (*get_attr_bool)(
        const char *name);

    /* Get a driver-specific boolean attribute */
    char* (*get_attr_string)(
        const char *name);

    /* Get a driver-specific array attribute */
    ut_ll (*get_attr_array)(
        const char *name);        

    /* Get a driver-specific attribute */
    void (*set_attr_string)(
        const char *name,
        const char *value);

    /* Get a driver-specific attribute */
    void (*set_attr_bool)(
        const char *name,
        bool value);

    /* Add string element to array */
    void (*set_attr_array)(
        const char *name,
        const char *value);

    /* Get direct access to parson data */
    JSON_Object* (*get_json)(void);
};

#endif
