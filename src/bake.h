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

/* Public includes */
#include "include/bake.h"

/* Private includes */
#include "crawler.h"
#include "project.h"

/* -- Configuration functions -- */

int16_t bake_config_load(
    bake_config *cfg_out,
    const char *cfg_id,
    const char *env_id,
    bool build_to_home);


/* -- Build functions -- */

int bake_do_build(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *p);

int bake_do_clean(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *p);

int bake_do_rebuild(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *p);

int bake_do_install(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *p);

int bake_do_uninstall(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *p);

int bake_do_foreach(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *p);

/* -- Remote install functions -- */

/* Clone remote repository */
bake_project* bake_clone(
    bake_config *config,
    const char *url);

/* Update remote repository */
bake_project* bake_update(
    bake_config *config,
    const char *url);

/* -- Install functions -- */

/** Copy project file to target environment */
int16_t bake_install_metadata(
    bake_config *config,
    bake_project *project);

/** Copy static files to target environment (pre build) */
int16_t bake_install_prebuild(
    bake_config *config,
    bake_project *project);

/** Copy artefact to target environment (post build) */
int16_t bake_install_postbuild(
    bake_config *config,
    bake_project *project);

/** Remove files from target environment for project except metadata */
int16_t bake_install_clear(
    bake_config *config,
    bake_project *project,
    bool uninstall);

/** Remove files from target environment for project */
int16_t bake_install_uninstall(
    bake_config *config,
    bake_project *project);


/* -- Driver functions -- */

extern bake_driver_api bake_driver_api_impl;

typedef struct bake_driver_impl {
    bake_driver_cb init;
    bake_artefact_cb artefact;
    bake_link_to_lib_cb link_to_lib;
    bake_driver_cb setup;
    bake_driver_cb generate;
    bake_driver_cb clean;
} bake_driver_impl;

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
};

bake_driver* bake_driver_get(
    const char *id);

/* Obtain artefact name from driver */
char* bake_driver__artefact(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Convert library name from link from driver */
char* bake_driver__link_to_lib(
    bake_driver *driver,
    bake_config *config,
    bake_project *project,
    const char *link);

/* Clean project */
int16_t bake_driver__clean(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Setup new project */
int16_t bake_driver__setup(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Generate driver code */
int16_t bake_driver__generate(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* -- Filelist -- */

typedef struct bake_file {
    char *path;
    char *name;
    char *file_path;
    uint64_t timestamp;
} bake_file;

typedef struct bake_filelist {
    char *path;
    char *pattern;
    ut_ll files;
    int16_t (*set)(const char *pattern);
} bake_filelist;

bake_filelist* bake_filelist_new(
    const char *path, const char *pattern);

void bake_filelist_free(
    bake_filelist* fl);

int16_t bake_filelist_set(
    bake_filelist *fl,
    const char *pattern);

ut_iter bake_filelist_iter(
    bake_filelist *fl);

bake_file* bake_filelist_add_file(
    bake_filelist *fl,
    const char *filename);

int16_t bake_filelist_add_pattern(
    bake_filelist *fl,
    const char *offset,
    const char *pattern);

int16_t bake_filelist_merge(
    bake_filelist *fl,
    bake_filelist *src);

uint64_t bake_filelist_count(
    bake_filelist *fl);

/* -- Rule API -- */

typedef struct bake_node {
    bake_rule_kind kind;
    const char *name;
    ut_ll deps;
    bake_condition_cb cond;
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

bake_node* bake_node_find(
    bake_driver *driver,
    const char *name);

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

int16_t bake_node_eval(
    bake_driver *driver,
    bake_node *n,
    bake_project *p,
    bake_config *c,
    bake_filelist *inherits,
    bake_filelist *outputs);


/* Attribute API */

ut_ll bake_attributes_parse(
    bake_config *config,
    bake_project *project,
    const char *project_id, /* differs from project if parsing dependee cfg */
    JSON_Object *object,
    ut_ll existing);

char* bake_attribute_replace(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    const char *input);

void bake_clean_string_array(
    ut_ll list);

/* -- JSON utilities -- */

int16_t bake_json_set_boolean(
    bool *ptr,
    const char *member,
    JSON_Value *v);

int16_t bake_json_set_string(
    char **ptr,
    const char *member,
    JSON_Value *v);

int16_t bake_json_set_array(
    ut_ll *ptr,
    const char *member,
    JSON_Value *v);

/* -- Setup -- */

int16_t bake_setup(
    bool local);
