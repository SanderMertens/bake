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
 * @section internal Internal bake API
 **/

/* Public includes */
#include <bake.h>

/* Private includes */
#include "crawler.h"
#include "project.h"

/* Track which messages have been printed already to avoid spamming the console */
typedef struct bake_notify_state {
    bool always_clone;    
} bake_notify_state;

void bake_message(
    int kind,
    const char *bracket_txt,
    const char *fmt,
    ...);

/* -- Repository database -- */

int16_t bake_use(
    bake_config *config, 
    const char *expr,
    bool update_config,
    bool load_bundle);

int16_t bake_unuse(
    bake_config *config,
    const char *project);

int16_t bake_add_repository(
    bake_config *config,
    const char *id,
    const char *repository,
    const char *branch,
    const char *commit,
    const char *tag,
    const char *project,
    const char *bundle);

bake_repository* bake_find_repository(
    bake_config *cfg,
    const char *id);

/* -- Configuration functions -- */

/** Find bake config file(s), load specified configuration & environment */
int16_t bake_config_load(
    bake_config *cfg_out,
    const char *env_id,
    bool load_bundles);

void bake_config_log(
    bake_config *cfg);

/** Export variable to bake configuration */
int16_t bake_config_export(
    bake_config *cfg,
    const char *expr);

/** Remove variable from bake configuration */
int16_t bake_config_unset(
    bake_config *cfg,
    const char *expr);

/** Add bundle to configuration */
int16_t bake_config_use_bundle(
    bake_config *cfg,
    const char *bundle,
    const char *tag,
    bool *changed);

/* Remove bundle from configruation */
int16_t bake_config_unuse_bundle(
    bake_config *cfg,
    const char *project_id,
    bool *changed);

/* Reset bundles in configuration */
int16_t bake_config_reset_bundles(
    bake_config *cfg);

/* -- Build functions -- */

/** Get project initialize for discovery phase */
int bake_do_pre_discovery(
    bake_config *config,
    bake_project *project);

/* Complete project configuration after all projects have been discovered */
int bake_do_post_discovery(
    bake_config *config,
    bake_project *project);

/** Build project */
int bake_do_build(
    bake_config *config,
    bake_project *p);

/** Clean project */
int bake_do_clean(
    bake_config *config,
    bake_project *p);

/** Rebuild project */
int bake_do_rebuild(
    bake_config *config,
    bake_project *p);

/** Install project files to bake environment (config->target) */
int bake_do_install(
    bake_config *config,
    bake_project *p);


/* -- Run project -- */

int bake_run(
    bake_config *config,
    const char *app_id,
    const char *run_prefix,
    bool interactive,
    int argc,
    const char *argv[]);

/* -- Git functions -- */

/* Clone from remote repository to local directory */
int16_t bake_clone(
    bake_config *config,
    const char *url,
    bool to_env,
    bool always_clone,
    bool clone_dependencies,
    bake_notify_state *notify_state,
    bake_project **project_out);

/* Clone project to bake environment from bundle */
int16_t bake_install(
    bake_config *config,
    const char *project_id);

/* Update from remote repository */
int bake_update_action(
    bake_config *config,
    bake_project *project);

/* Publish new version for repository */
int16_t bake_publish(
    bake_config *config,
    bake_project *project,
    const char *publish_cmd);

/* Quick & dirty check to see if URL is well formed */
int16_t bake_url_is_well_formed(
    const char *url,
    bool *has_protocol_out,
    bool *has_url_out);

/* -- Template functions -- */

/* Install template */
int16_t bake_install_template(
    bake_config *cfg,
    bake_project *project);

/* -- Install functions -- */

/** Copy project.json & other meta files to config->target */
int16_t bake_install_metadata(
    bake_config *config,
    bake_project *project);

/** Copy static files to config->target (pre build) */
int16_t bake_install_prebuild(
    bake_config *config,
    bake_project *project);

/** Copy artefact to config->target (post build) */
int16_t bake_install_postbuild(
    bake_config *config,
    bake_project *project);

int16_t bake_project_coverage(
    bake_config *config,
    bake_project *project);

/** Remove files from config->target for project except metadata and binary */
int16_t bake_install_clear(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    bool uninstall);

/** Uninstall project from bake environment */
int16_t bake_install_uninstall(
    bake_config *config,
    const char *project_id);

/** Uninstall template from bake environment */
int16_t bake_install_uninstall_template(
    bake_config *config,
    const char *project_id);

/* -- Driver functions -- */

extern bake_driver_api bake_driver_api_impl;

/** Functions (optionally) implemented by driver */
typedef struct bake_driver_impl {
    bake_driver_cb init;            /* Initializes driver for project */
    bake_artefact_cb artefact;      /* Returns artefact name for project */
    bake_link_to_lib_cb link_to_lib; /* Convert logical name to library name */
    bake_driver_cb setup;           /* Initialize directory with new project */
    bake_driver_cb generate;    /* Generate code or run code generation rules */
    bake_driver_cb prebuild;        /* Stage before build stage */
    bake_driver_cb build;           /* Called at build stage, before rules */
    bake_driver_cb postbuild;       /* Stage after build */
    bake_driver_cb test;            /* Stage before test */
    bake_driver_cb coverage;        /* Coverage analysis */
    bake_driver_cb clean;           /* Specify files to clean */
} bake_driver_impl;

/** Driver type */
struct bake_driver {
    char *id;                     /* Bake driver id (lang.c) */
    char *package_id;             /* Full driver package id (bake.lang.c) */
    ut_dl dl;                     /* Shared object */

    ut_ll nodes;                  /* Dependency graph with rules & patterns */
    ut_ll ignore_paths;           /* Project paths to ignore in discovery */

    int error;                    /* True if error occured */

    bake_driver_impl impl;        /* See above */
};

/* Find or load driver */
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

/* Initialize project */
int16_t bake_driver__init(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Generate driver code */
int16_t bake_driver__generate(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Prebuild step */
int16_t bake_driver__prebuild(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Prebuild step */
int16_t bake_driver__build(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Postbuild step */
int16_t bake_driver__postbuild(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Postbuild step */
int16_t bake_driver__test(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* Coverage analysis */
int16_t bake_driver__coverage(
    bake_driver *driver,
    bake_config *config,
    bake_project *project);

/* -- Filelist -- */

/** File matched by a pattern, created from map or added explicitly to filelist */
typedef struct bake_file {
    char *path;             /* File path (/home/user) */
    char *name;             /* File name (foo.c) */
    char *file_path;        /* File + path (/home/user/foo.c) */
    uint64_t timestamp;     /* Last modified timestamp */
} bake_file;

/** A filelist is populated with files inside a path that match a pattern */
typedef struct bake_filelist {
    char *path;             /* Path in which filelist applies pattern */
    char *pattern;          /* Pattern used to match against files */
    ut_ll files;            /* List of matched files (bake_file) */
    int16_t (*set)(const char *pattern);
} bake_filelist;

/** Create new filelist */
bake_filelist* bake_filelist_new(
    const char *path, const char *pattern);

/** Free filelist */
void bake_filelist_free(
    bake_filelist* fl);

/** Set pattern in filelist */
int16_t bake_filelist_set(
    bake_filelist *fl,
    const char *pattern);

/** Iterate over files in filelist */
ut_iter bake_filelist_iter(
    bake_filelist *fl);

/** Manually add file to filelist */
bake_file* bake_filelist_add_file(
    bake_filelist *fl,
    const char *filepath,
    const char *file);

/** Set pattern and path */
int16_t bake_filelist_add_pattern(
    bake_filelist *fl,
    const char *path,
    const char *pattern);

/** Merge two filelists into destination */
int16_t bake_filelist_merge(
    bake_filelist *dst,
    bake_filelist *src);

/** Return number of files in filelist */
int bake_filelist_count(
    bake_filelist *fl);

/* -- Rule API -- */

/** Base type for rule or pattern */
typedef struct bake_node {
    bake_rule_kind kind;    /* Is node rule or pattern */
    const char *name;       /* Node name */
    ut_ll deps;             /* Node dependencies */
    bake_condition_cb cond; /* Node condition */
} bake_node;

/** Pattern node
 * A pattern matches a list of files, using a string expression with wildcards.
 */
typedef struct bake_pattern {
    bake_node super;        /* Node super type */
    const char *pattern;    /* Pattern expression */
} bake_pattern;

/** Rule node
 * A rule is invoked when a dependee requires its outputs. When invoked, the
 * rule will invoke the action for every input matching the required outputs.
 */
typedef struct bake_rule {
    bake_node super;        /* Node super type */
    const char *source;     /* Source pattern */
    bake_rule_target target;      /* Rule target (MAP or PATTERN) */
    bake_rule_action_cb action;   /* Action to execute for rule */
} bake_rule;

/** Dependency rule
 * A dependency rule can dynamically insert a list of dependent files. A typical
 * example for this is using header files (from generated .dep files) as
 * dependencies for object files, in addition to a source file. */
typedef struct bake_dependency_rule {
    bake_node super;
    bake_rule_target target;
    const char *deps;
    bake_rule_action_cb action;
} bake_dependency_rule;

/** Find a node by name */
bake_node* bake_node_find(
    bake_driver *driver,
    const char *name);

/** Create new pattern */
bake_pattern* bake_pattern_new(
    const char *name,
    const char *pattern);

/* Create a new file pattern */
bake_pattern* bake_file_pattern_new(
    const char *name,
    const char *pattern);

/** Create new rule */
bake_rule* bake_rule_new(
    const char *name,
    const char *source,
    bake_rule_target target,
    bake_rule_action_cb action);

/** Create new dependency rule */
bake_dependency_rule* bake_dependency_rule_new(
    const char *name,
    const char *deps,
    bake_rule_target dep_mapping,
    bake_rule_action_cb action);

/** Evaluate node */
int16_t bake_node_eval(
    bake_driver *driver,
    bake_node *n,
    bake_project *p,
    bake_config *c,
    bake_filelist *inherits,
    bake_filelist *outputs);


/* Attribute API */

/** Parse JSON object into list of attributes */
ut_ll bake_attrs_parse(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    JSON_Object *object,
    ut_ll existing);

/** Replace bake functions and environment variables in string */
char* bake_attr_replace(
    bake_config *config,
    bake_project *project,
    const char *package_id,
    const char *input);

/* Clean list of strings */
void bake_clean_string_array(
    ut_ll list);

/* -- JSON utilities -- */

/** Set boolean value from JSON value */
int16_t bake_json_set_boolean(
    bool *ptr,
    const char *member,
    JSON_Value *v);

/** Set string value from JSON value */
int16_t bake_json_set_string(
    char **ptr,
    const char *member,
    JSON_Value *v);

/** Set array value from JSON value */
int16_t bake_json_set_array(
    ut_ll *ptr,
    const char *member,
    JSON_Value *v);

/** Find or create a JSON object inside the specified object */
JSON_Object* bake_json_find_or_create_object(
    JSON_Object *value,
    const char *member);

/* -- Setup -- */

/* Install bake */
int16_t bake_setup(
    bake_config *cfg,
    const char *cmd,
    bool local);

/* Reset bake environment */
int bake_reset(
    bake_config *cfg);

