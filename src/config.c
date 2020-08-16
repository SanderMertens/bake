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

#include "bake.h"

static bool env_found = false;
static bool cfg_found = false;

static
int16_t bake_config_load_bundle(
    bake_config *cfg,
    const char *project_id,
    const char *bundle)
{
    /* First try to load project and bundle, to make it is available */
    const char *project_path = ut_locate(project_id, NULL, UT_LOCATE_PROJECT);
    if (!project_path) {
        ut_throw("cannot load bundle '%s:%s', project not found",
            project_id, bundle);
        goto error;
    }

    bake_project *project = bake_project_new(project_path, cfg);
    if (!project) {
        ut_throw("cannot load bundle '%s:%s', error loading project file",
            project_id, bundle);
        goto error;
    }

    ut_try( bake_project_load_bundle(cfg, project, bundle), 
        "cannot load bundle '%s:%s', error loading project bundle",
            project_id, bundle);

    if (!cfg->bundles) {
        cfg->bundles = ut_ll_new();
    }

    bake_bundle *new_bundle = malloc(sizeof(bake_bundle));
    new_bundle->project = ut_strdup(project_id);
    new_bundle->id = ut_strdup(bundle);

    ut_ll_append(cfg->bundles, new_bundle);

    return 0;
error:
    return -1;
}

static
bool bake_config_var_is_valid(
    const char *var)
{
    const char *ptr;
    char ch;
    for (ptr = var; (ch = *ptr); ptr++) {
        if (!isalpha(ch) && !isdigit(ch) && (ch != '_')) {
            return false;
        }
    }
    return true;
}

static
int32_t bake_config_var_is_set(
    bake_config *config,
    const char *var)
{
    int32_t index = 0;
    ut_iter it = ut_ll_iter(config->env_variables);
    while (ut_iter_hasNext(&it)) {
        if (!strcmp(ut_iter_next(&it), var)) {
            return index;
        }
        index ++;
    }

    return -1;
}

static
void bake_config_add_var(
    bake_config *config,
    const char *var,
    const char *value,
    bool append)
{
    int32_t index = bake_config_var_is_set(config, var);
    if (index == -1) {
        /* Variable hasn't been set yet */
        if (!value) value = "";
        ut_ll_append(config->env_variables, ut_strdup(var));
        ut_ll_append(config->env_values, ut_strdup(value));
    } else if (value) {
        /* Previous value has been set */
        if (append) {
            char *old_value = ut_ll_get(config->env_values, index);
            if (old_value) {
                char *new_value = ut_asprintf("%s"UT_ENV_PATH_SEPARATOR"%s",
                    old_value, value);
                    
                ut_ll_set(config->env_values, index, new_value);

                free(old_value);
            } else {
                ut_ll_set(config->env_values, index, ut_strdup(value));
            }
        } else {
            ut_ll_set(config->env_values, index, ut_strdup(value));
        }
    }
}

static
int16_t bake_config_loadEnvironment(
    bake_config *cfg_out,
    JSON_Object *envcfg)
{
    int i;
    ut_log_push("load-env");

    for (i = 0; i < json_object_get_count(envcfg); i ++) {
        const char *var = json_object_get_name(envcfg, i);
        if (!bake_config_var_is_valid(var)) {
            ut_warning(
                "'%s' is not a valid environment variable name, skipping", var);
            continue;
        }

        /* Accept both arrays and strings */
        const char *j_str = json_object_get_string(envcfg, var);
        char *value = (char*)j_str;
        bool append = false;

        if (!value) {
            JSON_Value *j_value = json_object_get_value(envcfg, var);
            if (json_value_get_type(j_value) == JSONArray) {
                JSON_Array *j_array = json_value_get_array(j_value);
                ut_strbuf buf = UT_STRBUF_INIT;
                uint32_t i, count = json_array_get_count(j_array);
                for (i = 0; i < count; i ++) {
                    const char *el = json_array_get_string(j_array, i);
                    if (i) {
                        ut_strbuf_appendstr(&buf, UT_ENV_PATH_SEPARATOR);
                    }
                    ut_strbuf_appendstr(&buf, el);
                }
                value = ut_strbuf_get(&buf);

                /* Append to, not replace previous value (if any) */
                append = true;
            } else {
                ut_throw("invalid value for environment variable '%s'", var);
                goto error;
            }
        }

        bake_config_add_var(cfg_out, var, value, append);

        if (j_str != value) {
            free(value);
        }
    }

    ut_log_pop();
    return 0;
error:
    ut_log_pop();
    return -1;
}

#define CFG_SYMBOLS "symbols"
#define CFG_DEBUG "debug"
#define CFG_OPTIMIZATIONS "optimizations"
#define CFG_COVERAGE "coverage"
#define CFG_STRICT "strict"

static
int16_t bake_config_loadConfiguration(
    JSON_Object *cfg,
    bake_config *cfg_out)
{
    ut_log_push("load-cfg");
    int i;
    for (i = 0; i < json_object_get_count(cfg); i++) {
        JSON_Value *value = json_object_get_value_at(cfg, i);

        if (bake_json_set_boolean(&cfg_out->symbols, CFG_SYMBOLS, value)) {
            goto error;
        }
        if (bake_json_set_boolean(&cfg_out->debug, CFG_DEBUG, value)) {
            goto error;
        }
        if (bake_json_set_boolean(&cfg_out->optimizations, CFG_OPTIMIZATIONS, value)) {
            goto error;
        }
        if (bake_json_set_boolean(&cfg_out->coverage, CFG_COVERAGE, value)) {
            goto error;
        }
        if (bake_json_set_boolean(&cfg_out->strict, CFG_STRICT, value)) {
            goto error;
        }
    }
    ut_log_pop();
    return 0;
error:
    ut_log_pop();
    return -1;
}

static
int16_t bake_config_findSection(
    JSON_Object *object,
    const char *lookfor,
    const char *item,
    JSON_Object **out)
{
    JSON_Object *result = NULL;
    if (!lookfor) {
        lookfor = "default";
    }

    int i;
    for (i = 0; i < json_object_get_count(object); i ++) {
        const char *name = json_object_get_name(object, i);
        if (!strcmp(name, lookfor)) {
            JSON_Value *section = json_object_get_value_at(object, i);
            result = json_value_get_object(section);
            if (!result) {
                ut_throw(
                    "invalid json: expected value of '%s' to be an object",
                    name);
                goto error;
            }
            break;
        }
    }

    if (i == json_object_get_count(object)) {
        ut_trace("%s '%s' not found", item, lookfor);
        goto not_found;
    }

    *out = result;

    return 0;
not_found:
    return 1;
error:
    return -1;
}

static
void bake_config_setEnv(
    bake_config *cfg)
{
    ut_iter it_var = ut_ll_iter(cfg->env_variables);
    ut_iter it_val = ut_ll_iter(cfg->env_values);
    while (ut_iter_hasNext(&it_var) && ut_iter_hasNext(&it_val)) {
        char *var = ut_iter_next(&it_var);
        char *val = ut_iter_next(&it_val);
        ut_setenv(var, val);
    }
}

static
int16_t bake_config_load_file (
    const char *file,
    bake_config *cfg_out,
    const char *cfg_name,
    const char *env_name,
    bool load_bundles)
{
    ut_ok("load configuration '%s'", file);

    JSON_Value *json = json_parse_file_with_comments(file);
    if (!json) {
        ut_throw("failed to parse bake configuration '%s'", file);
        goto error;
    }

    JSON_Object *jsonObj = json_value_get_object(json);
    if (!jsonObj) {
        ut_throw("invalid JSON: expected object");
        goto error;
    }

    /* Parse environment */
    JSON_Object *env = json_object_get_object(jsonObj, "environment");
    if (env) {
        JSON_Object *section = NULL;
        if (bake_config_findSection(
            env, env_name, "environment", &section) == -1)
        {
            goto error;
        }
        if (!section) {
            goto not_found;
        }

        env_found = true;

        if (bake_config_loadEnvironment(cfg_out, section)) {
            goto error;
        }
    }

    /* Parse configuration */
    JSON_Object *cfg = json_object_get_object(jsonObj, "configuration");
    if (cfg) {
        JSON_Object *section = NULL;
        if (bake_config_findSection(
            cfg, cfg_name, "configuration", &section) == -1)
        {
            goto error;
        }
        if (!section) {
            goto not_found;
        }

        cfg_found = true;

        if (bake_config_loadConfiguration(section, cfg_out)) {
            goto error;
        }
    }

    /* Parse bundles */
    if (load_bundles) {
        JSON_Object *bundles = json_object_get_object(jsonObj, "bundles");

        if (bundles) {
            int i;
            for (i = 0; i < json_object_get_count(bundles); i ++) {
                const char *project_id = json_object_get_name(bundles, i);
                JSON_Value *j_project = json_object_get_value_at(bundles, i);
                JSON_Object *o_project = json_object(j_project);
                if (!o_project) {
                    ut_throw("%s: invalid configuration, expected object for '%s'",
                        file, project_id);
                    goto error;    
                }

                const char *bundle = json_object_get_string(o_project, "bundle");
                if (!bundle) {
                    bundle = "default";
                }
                
                if (bake_config_load_bundle(cfg_out, project_id, bundle)) {
                    ut_warning(
                        "%s: bundle '%s:%s' not loaded due to errors, ignoring", 
                        file, project_id, bundle);
                }
            }
        }
    }

    json_value_free(json);

    return 0;
not_found:
    return 1;
error:
    return -1;
}

static
int16_t bake_config_load_config(
    ut_ll config_files,
    bake_config *cfg_out,
    const char *cfg_id,
    const char *env_id,
    bool load_bundles)
{
    char *file;
    while ((file = ut_ll_takeFirst(config_files))) {
        if (bake_config_load_file(file, cfg_out, cfg_id, env_id, load_bundles) == -1) {
            goto error;
        }
        free(file);
    }

    return 0;
error:
    return -1;
}

static
ut_ll bake_config_find_files(
    char *path)
{
    ut_ll config_files = NULL; /* Collect list of all config files */
    char *cur_path = path;

    char *path_parsed = ut_envparse(path);
    if (!ut_path_is_relative(path_parsed)) {
        /* Absolute path */
        cur_path = strdup(path_parsed);
    } else {
        /* Relative path */
        cur_path = ut_asprintf("%s"UT_OS_PS"%s", ut_cwd(), path_parsed);
        ut_path_clean(cur_path, cur_path);
    }

    free (path_parsed);

    /* Check for a bake in the current path */
    char *elem = NULL;
    do {
        char *file = ut_asprintf("%s"UT_OS_PS"bake%s", cur_path, UT_OS_BIN_EXT);
        if (ut_file_test(file) == 1) {
            if (ut_isdir(file)) {
                char *cfg = ut_asprintf("%s"UT_OS_PS"bake.json", file);
                if (ut_file_test(cfg) == 1) {
                    if (!config_files) config_files = ut_ll_new();
                    ut_ll_append(config_files, cfg);
                    ut_trace("using configuration file '%s'", cfg);
                } else {
                    free(cfg);
                }
            } else {
                free(file);
            }
        } else {
            free(file);
        }


        file = ut_asprintf("%s"UT_OS_PS"bake.json", cur_path);
        if (ut_file_test(file) == 1) {
            if (!config_files) config_files = ut_ll_new();
            ut_ll_append(config_files, file);
            ut_trace("using configuration file '%s'", file);
        } else {
            free(file);
        }

        /* Strip last directory from path */
        elem = strrchr(cur_path, UT_OS_PS[0]);
        if (elem) {
            *elem = '\0';
        }
    } while (elem);

    return config_files;
}

static
ut_ll bake_config_find_config(void)
{
    ut_ll config_files = NULL;

    if (ut_getenv("BAKE_HOME")) {
        char *file = ut_asprintf("%s"UT_OS_PS"bake.json", ut_getenv("BAKE_HOME"));
        if (ut_file_test(file) == 1) {
            config_files = ut_ll_new();
            ut_ll_append(config_files, file);
        }
    }
    if (!config_files) {
        config_files = bake_config_find_files(".");
    }
    if (!config_files) {
        config_files = bake_config_find_files("~");
    }

    return config_files;
}

int16_t bake_config_load(
    bake_config *cfg_out,
    const char *env_id,
    bool load_bundles)
{
    /* Use default configuration and environment */
    cfg_out->env_variables = ut_ll_new();
    cfg_out->env_values = ut_ll_new();

    /* Collect & load configuration files */
    ut_ll config_files = bake_config_find_config();

    if (config_files) {
        if (bake_config_load_config(
            config_files,
            cfg_out,
            UT_CONFIG,
            env_id,
            load_bundles))
        {
            goto error;
        }
    }

    if (!cfg_found) {
        if (!strcmp(UT_CONFIG, "debug")) {
            ut_ok("debug configuration not found in bake settings file, using defaults");
            cfg_out->debug = true;
            cfg_out->symbols = true;
            cfg_out->optimizations = false;
            cfg_out->coverage = false;
            cfg_out->sanitize_memory = false;
            cfg_out->sanitize_undefined = false;
        } else if (!strcmp(UT_CONFIG, "release")) {
            ut_ok("release configuration not found in bake settings file, using defaults");
            cfg_out->debug = false;
            cfg_out->symbols = false;
            cfg_out->optimizations = true;
            cfg_out->coverage = false;
            cfg_out->sanitize_memory = false;
            cfg_out->sanitize_undefined = false;            
        } else if (!strcmp(UT_CONFIG, "test")) {
            ut_ok("test configuration not found in bake settings file, using defaults");
            cfg_out->debug = true;
            cfg_out->symbols = true;
            cfg_out->optimizations = false;            
            cfg_out->coverage = true;
            cfg_out->sanitize_memory = false;
            cfg_out->sanitize_undefined = false;
        } else if (!strcmp(UT_CONFIG, "sanitize")) {
            ut_ok("test configuration not found in bake settings file, using defaults");
            cfg_out->debug = true;
            cfg_out->symbols = true;
            cfg_out->optimizations = false;            
            cfg_out->coverage = false;
            cfg_out->sanitize_memory = true;
            cfg_out->sanitize_undefined = true;
        } else if (!strcmp(UT_CONFIG, "perf")) {
            cfg_out->debug = false;
            cfg_out->symbols = true;
            cfg_out->optimizations = true;
            cfg_out->coverage = false;
        } else {
            ut_throw("unknown configuration '%s'",
                UT_CONFIG);
            goto error;
        }
    }

    /* Set BAKE_HOME to a default value if the config didn't specify it */
    if (!ut_getenv("BAKE_HOME")) {
#ifndef _WIN32
        char *bake_home = ut_envparse("~/bake");
#else
        char *bake_home = ut_envparse("~\\bake");
#endif
        if (!bake_home) {
            ut_throw(NULL);
            goto error;
        }

        ut_ok("$BAKE_HOME not set, default to '%s'", bake_home);

        ut_try (ut_setenv("BAKE_HOME", "%s", bake_home), NULL);
        free(bake_home);
    }

    /* Set BAKE_TARGET to a default value if the config didn't specify it */
    if (!ut_getenv("BAKE_TARGET")) {
        ut_ok("$BAKE_TARGET not set, default to $BAKE_HOME");
        ut_try (ut_setenv("BAKE_TARGET", "$BAKE_HOME"), NULL);
    }

    /* Ensure these environment variables are part of the bake environment */
    bake_config_add_var(cfg_out, "BAKE_HOME", ut_getenv("BAKE_HOME"), false);
    bake_config_add_var(cfg_out, "BAKE_TARGET", ut_getenv("BAKE_TARGET"), false);
    bake_config_add_var(cfg_out, "BAKE_PLATFORM", UT_PLATFORM_STRING, false);
    bake_config_add_var(cfg_out, "PATH", ut_getenv("PATH"), true);
    bake_config_add_var(cfg_out, "CLASSPATH", ut_getenv("CLASSPATH"), true);
    if (!ut_os_match("windows")) {
        bake_config_add_var(cfg_out, "LD_LIBRARY_PATH", ut_getenv("LD_LIBRARY_PATH"), true);
    }
    if (ut_os_match("darwin")) {
        bake_config_add_var(cfg_out, "DYLD_LIBRARY_PATH", ut_getenv("DYLD_LIBRARY_PATH"), true);
    }

    /* Ensure that these environment variables are set, so no errors are thrown
     * when they are referenced by the configuration */
    if (!ut_getenv("PATH")) ut_setenv("PATH", "");
    if (!ut_os_match("windows")) {
        if (!ut_getenv("LD_LIBRARY_PATH")) ut_setenv("LD_LIBRARY_PATH", "");
    }
    if (!ut_getenv("CLASSPATH")) ut_setenv("CLASSPATH", "");
    if (ut_os_match("darwin")) {
        if (!ut_getenv("DYLD_LIBRARY_PATH")) ut_setenv("DYLD_LIBRARY_PATH", "");
    }

    /* Set configured environment variables */
    bake_config_setEnv(cfg_out);

    /* Precompute bake paths */
    cfg_out->configuration = ut_strdup(UT_CONFIG);
    cfg_out->architecture = "x86";
    cfg_out->platform = UT_PLATFORM_PATH;
    cfg_out->target = UT_TARGET_PATH;
    cfg_out->home = UT_HOME_PATH;
    cfg_out->lib = UT_LIB_PATH;
    cfg_out->bin = UT_BIN_PATH;

    if (ut_log_verbosityGet() <= UT_OK) {
        ut_log_push("environment");
        ut_iter it = ut_ll_iter(cfg_out->env_variables);
        while (ut_iter_hasNext(&it)) {
            char *env = ut_iter_next(&it);
            ut_trace("set '%s' to '%s'", env, ut_getenv(env));
        }
        ut_log_pop();

        ut_log_push("configuration");
        ut_trace("set '%s' to '%s'", CFG_SYMBOLS, cfg_out->symbols ? "true" : "false");
        ut_trace("set '%s' to '%s'", CFG_DEBUG, cfg_out->debug ? "true" : "false");
        ut_trace("set '%s' to '%s'", CFG_OPTIMIZATIONS, cfg_out->optimizations ? "true" : "false");
        ut_trace("set '%s' to '%s'", CFG_COVERAGE, cfg_out->coverage ? "true" : "false");
        ut_trace("set '%s' to '%s'", CFG_STRICT, cfg_out->strict ? "true" : "false");
        ut_log_pop();
    }

    return 0;
error:
    ut_log_pop();
    return -1;
}

int16_t bake_config_export(
    bake_config *cfg,
    const char *expr)
{
    bool is_add = false;
    char *var = ut_strdup(expr), *value = NULL;

    value = strrchr(var, '=');
    if (value) {
        if (value == expr) {
            ut_throw("missing variable name in export expression");
            goto error;
        }

        if (value[-1] == '+') {
            is_add = true;
            if (value - 1 == expr) {
                ut_throw("missing variable name in export expression");
                goto error;
            }
            value[-1] = '\0';
        } else {
            value[0] = '\0';
        }
        value ++;
    }

    char *bake_json = ut_envparse("$BAKE_HOME/bake.json");

    if (ut_file_test(bake_json) != 1) {
        FILE *f = fopen(bake_json, "w");
        fprintf(f,
            "{\"environment\":{}}"
        );
        fclose(f);
    }

    JSON_Value *root = json_parse_file_with_comments(bake_json);
    if (!root) {
        ut_throw("failed to parse file '%s'", bake_json);
        goto error;
    }

    JSON_Object *root_obj = json_value_get_object(root);
    if (!root_obj) {
        ut_throw("expected JSON object as root of file '%s'", bake_json);
        goto error;
    }

    JSON_Object *env = bake_json_find_or_create_object(root_obj, "environment");
    if (!env) {
        goto error;
    }

    JSON_Object *cur = bake_json_find_or_create_object(env, cfg->environment);
    if (!cur) {
        goto error;
    }

    JSON_Value *val = json_object_get_value(cur, var);

    if (!is_add) {
        if (json_value_get_type(val) != JSONString) {
            json_object_remove(cur, var);
        }
        json_object_set_string(cur, var, value);
    } else {
        if (json_value_get_type(val) != JSONArray) {
            json_object_remove(cur, var);
            json_object_set_value(cur, var, json_value_init_array());
            val = json_object_get_value(cur, var);
        }
        JSON_Array *array = json_value_get_array(val);

        /* Don't add duplicate values */
        int32_t i;
        for (i = 0; i < json_array_get_count(array); i ++) {
            const char *str = json_array_get_string(array, i);
            if (!strcmp(str, value)) {
                break;
            }
        }

        if (i == json_array_get_count(array)) {
            json_array_append_string(array, value);
        }
    }

    json_set_escape_slashes(0);

    json_serialize_to_file_pretty(root, bake_json);

    return 0;
error:
    return -1;
}

int16_t bake_config_unset(
    bake_config *cfg,
    const char *var)
{
    char *bake_json = ut_envparse("$BAKE_HOME/bake.json");

    if (ut_file_test(bake_json) != 1) {
        FILE *f = fopen(bake_json, "w");
        fprintf(f,
            "{\"environment\":{}}"
        );
        fclose(f);
    }

    JSON_Value *root = json_parse_file_with_comments(bake_json);
    if (!root) {
        ut_throw("failed to parse file '%s'", bake_json);
        goto error;
    }

    JSON_Object *root_obj = json_value_get_object(root);
    if (!root_obj) {
        ut_throw("expected JSON object as root of file '%s'", bake_json);
        goto error;
    }

    JSON_Object *env = bake_json_find_or_create_object(root_obj, "environment");
    if (!env) {
        goto error;
    }

    JSON_Object *cur = bake_json_find_or_create_object(env, cfg->environment);
    if (!cur) {
        goto error;
    }

    json_object_remove(cur, var);

    json_set_escape_slashes(0);

    json_serialize_to_file_pretty(root, bake_json);

    return 0;
error:
    return -1;
}

int16_t bake_config_use_bundle(
    bake_config *cfg,
    const char *project_id,
    const char *bundle,
    bool *changed)
{
    ut_try( bake_config_load_bundle(cfg, project_id, bundle), NULL);

    /* Bake was able to load bundle, continue adding it to the config file */
    char *bake_json = ut_envparse("$BAKE_HOME/bake.json");

    if (ut_file_test(bake_json) != 1) {
        FILE *f = fopen(bake_json, "w");
        fprintf(f,
            "{\"bundles\":{}}"
        );
        fclose(f);
    }

    if (!bundle) {
        bundle = "default";
    }

    JSON_Value *root = json_parse_file_with_comments(bake_json);
    if (!root) {
        ut_throw("failed to parse file '%s'", bake_json);
        goto error;
    }

    JSON_Object *root_obj = json_value_get_object(root);
    if (!root_obj) {
        ut_throw("expected JSON object as root of file '%s'", bake_json);
        goto error;
    }

    JSON_Object *bundles = bake_json_find_or_create_object(root_obj, "bundles");
    if (!bundles) {
        goto error;
    }

    JSON_Object *cur = bake_json_find_or_create_object(bundles, project_id);
    if (!cur) {
        goto error;
    }

    JSON_Value *val = json_object_get_value(cur, "bundle");
    if (val) {
        if (json_value_get_type(val) != JSONString) {
            json_object_remove(cur, "bundle");
            if (changed) {
                *changed = true;
            }
        }


        const char *prev = json_string(val);
        if (prev) {
            if (strcmp(prev, bundle)) {
                if (changed) {
                    *changed = true;
                }
            }
        }
    } else {
        if (changed) {
            *changed = true;
        }
    }

    json_object_set_string(cur, "bundle", bundle);

    json_set_escape_slashes(0);

    json_serialize_to_file_pretty(root, bake_json);

    json_value_free(root);

    free(bake_json);

    return 0;
error:
    return -1;
}

int16_t bake_config_unuse_bundle(
    bake_config *cfg,
    const char *project_id,
    bool *changed)
{
    bool bundle_found = false;

    /* Bake was able to load bundle, continue adding it to the config file */
    char *bake_json = ut_envparse("$BAKE_HOME/bake.json");

    if (ut_file_test(bake_json) != 1) {
        goto no_bundle;
    }

    JSON_Value *root = json_parse_file_with_comments(bake_json);
    if (!root) {
        ut_throw("failed to parse file '%s'", bake_json);
        goto parse_error;
    }

    JSON_Object *root_obj = json_value_get_object(root);
    if (!root_obj) {
        ut_throw("expected JSON object as root of file '%s'", bake_json);
        goto error;
    }

    JSON_Object *bundles = json_object_get_object(root_obj, "bundles");
    if (!bundles) {
        goto no_bundle;
    }

    JSON_Object *cur = json_object_get_object(bundles, project_id);
    if (!cur) {
        goto no_bundle;
    }

    json_object_remove(bundles, project_id);

    json_set_escape_slashes(0);

    json_serialize_to_file_pretty(root, bake_json);

    bundle_found = true;

no_bundle:
    if (changed) {
        *changed = bundle_found;
    }

    free(bake_json);
    json_value_free(root);

    return 0;
parse_error:

error:
    free(bake_json);
    return -1;
}

int16_t bake_config_reset_bundles(
    bake_config *cfg)
{
    char *bake_json = ut_envparse("$BAKE_HOME/bake.json");

    if (ut_file_test(bake_json) != 1) {
        return 0;
    }

    JSON_Value *root = json_parse_file_with_comments(bake_json);
    if (!root) {
        ut_throw("failed to parse file '%s'", bake_json);
        goto error;
    }

    JSON_Object *root_obj = json_value_get_object(root);
    if (!root_obj) {
        ut_throw("expected JSON object as root of file '%s'", bake_json);
        goto error;
    }

    json_object_dotremove(root_obj, "bundles");

    json_set_escape_slashes(0);

    json_serialize_to_file_pretty(root, bake_json);

    free(bake_json);

    return 0;
error:
    return -1;
}