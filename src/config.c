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

#include "bake.h"

static
char* bake_config_findFile(
    char *previous_file)
{
    char *file = ut_getenv("BAKE_CONFIG_FILE");
    if (file && previous_file) {
        /* Don't go looking for files if one was explicitly specified */
        goto error;
    }

    if (!file) {
        char *cwd = previous_file;
        if (!cwd) {
            cwd = ut_strdup(ut_cwd());
        } else {
            char *last;
            /* Strip file */
            if ((last = strrchr(cwd, '/'))) last[0] = '\0';
            else goto error;

            if (!strcmp(last + 1, "config.json")) {
                /* Strip .bake directory */
                if ((last = strrchr(cwd, '/'))) last[0] = '\0';
                else goto error;
            }

            /* Strip last directory */
            if ((last = strrchr(cwd, '/'))) last[0] = '\0';
            else goto error;
        }
        do {
            file = ut_asprintf("%s/.bake", cwd);

            /* If this is a .bake directory, look for config file inside of
             * directory */
            if (ut_isdir(file)) {
                char *tmp = file;
                file = ut_asprintf("%s/.bake/config.json", cwd);
                free(tmp);
            }

            if (!ut_file_test(file)) {
                char *last = strrchr(cwd, '/');
                if (last) {
                    last[0] = '\0';
                }
                free(file);
                file = NULL;
            } else {
                break;
            }
        } while (strlen(cwd));
        free(cwd);
    } else {
        file = ut_strdup(file);
    }

    return file;
error:
    return NULL;
}

static
bool bake_config_isVarValid(
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
bool bake_config_var_set(
    ut_ll env_set,
    const char *var)
{
    char *var_set = NULL;
    ut_iter it = ut_ll_iter(env_set);
    while (ut_iter_hasNext(&it)) {
        var_set = ut_iter_next(&it);
        if (!strcmp(var_set, var)) {
            break;
        } else {
            var_set = NULL;
        }
    }

    return var_set != NULL;
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
        if (!bake_config_isVarValid(var)) {
            ut_throw("'%s' is not a valid environment variable name");
            goto error;
        }

        const char *value = json_object_get_string(envcfg, var);
        if (!value) {
            ut_throw("invalid value for environment variable '%s'", var);
            goto error;
        }

        /* Shell environment takes precedence */
        if (!ut_getenv(var)) {
            if (ut_setenv(var, value)) {
                goto error;
            }
            /* Add variable to list of environment variables, so it can be
             * exported later */
            if (strcmp(var, "BAKE_HOME") &&
                strcmp(var, "BAKE_TARGET") &&
                strcmp(var, "BAKE_VERSION") &&
                strcmp(var, "BAKE_CONFIG"))
            {
                if (!cfg_out->variables) {
                    cfg_out->variables = ut_ll_new();
                }
                ut_ll_append(cfg_out->variables, ut_strdup(var));
            }
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
        ut_throw("%s '%s' not found", item, lookfor);
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
void bake_config_appendEnv(
    const char *env,
    const char *value)
{
    char *existing = ut_getenv(env);
    if (!existing || !strlen(existing)) {
        ut_setenv(env, value);
    } else {
        ut_setenv(env, strarg("%s:%s", value, existing));
    }
}

static
void bake_config_setEnv(
    bake_config *cfg)
{
    bake_config_appendEnv("PATH", strarg("~/.bake:%s", cfg->target_bin));
    bake_config_appendEnv("LD_LIBRARY_PATH", strarg(".:%s", cfg->target_lib));
    bake_config_appendEnv("DYLD_LIBRARY_PATH", strarg(".:%s", cfg->target_lib));
    bake_config_appendEnv("CLASSPATH", strarg(".:%s/java", cfg->target));
}

static
int16_t bake_config_load_file (
    const char *file,
    bake_config *cfg_out,
    const char *cfg_name,
    const char *env_name)
{
    ut_ok("load configuration '%s'", file);

    JSON_Value *json = json_parse_file(file);
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
        if (bake_config_loadConfiguration(section, cfg_out)) {
            goto error;
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
int16_t bake_config_load_configs(
    ut_ll config_files,
    bake_config *cfg_out,
    const char *cfg_id,
    const char *env_id)
{
    char *file;
    while ((file = ut_ll_takeFirst(config_files))) {
        if (bake_config_load_file(file, cfg_out, cfg_id, env_id)) {
            goto error;
        }
        free(file);
    }

    return 0;
error:
    return -1;
}

static
ut_ll bake_config_find_configs(
    char *path)
{
    ut_ll config_files = NULL; /* Collect list of all config files */
    char *cur_path = path;

    char *path_parsed = ut_envparse(path);

    if (path_parsed[0] == '/') {
        /* Absolute path */
        cur_path = strdup(path_parsed);
    } else {
        /* Relative path */
        cur_path = ut_asprintf("%s/%s", ut_cwd(), path_parsed);
        ut_path_clean(cur_path, cur_path);
    }

    free (path_parsed);

    /* Check for a .bake in the current path */
    char *elem = NULL;
    do {
        char *file = ut_asprintf("%s/.bake", cur_path);
        if (ut_file_test(file) == 1) {
            if (ut_isdir(file)) {
                char *cfg = ut_asprintf("%s/config.json", file);
                if (ut_file_test(cfg)) {
                    if (!config_files) config_files = ut_ll_new();
                    ut_ll_append(config_files, cfg);
                    ut_trace("using configuration file '%s'", cfg);
                } else {
                    free(cfg);
                }
            } else {
                if (!config_files) config_files = ut_ll_new();
                ut_ll_append(config_files, file);
                ut_trace("using configuration file '%s'", file);
            }
        } else {
            free(file);
        }

        file = ut_asprintf("%s/bake.json", cur_path);
        if (ut_file_test(file) == 1) {
            ut_ll_append(config_files, file);
            ut_trace("using configuration file '%s'", file);
        } else {
            free(file);
        }

        /* Strip last directory from path */
        elem = strrchr(cur_path, '/');
        if (elem) {
            *elem = '\0';
        }
    } while (elem);

    return config_files;
}

int16_t bake_config_load(
    bake_config *cfg_out,
    const char *cfg_id,
    const char *env_id)
{
    /* Use default configuration and environment */
    *cfg_out = (bake_config){
        .id = cfg_id,
        .symbols = true,
        .debug = true,
        .optimizations = false,
        .coverage = false,
        .strict = false
    };

    cfg_out->variables = ut_ll_new();
    ut_ll_append(cfg_out->variables, ut_strdup("BAKE_HOME"));
    ut_ll_append(cfg_out->variables, ut_strdup("BAKE_TARGET"));
    ut_ll_append(cfg_out->variables, ut_strdup("BAKE_VERSION"));
    ut_ll_append(cfg_out->variables, ut_strdup("BAKE_CONFIG"));
    ut_ll_append(cfg_out->variables, ut_strdup("PATH"));
    ut_ll_append(cfg_out->variables, ut_strdup("LD_LIBRARY_PATH"));
    ut_ll_append(cfg_out->variables, ut_strdup("CLASSPATH"));
    if (ut_os_match("darwin")) {
        ut_ll_append(cfg_out->variables, ut_strdup("DYLD_LIBRARY_PATH"));
    }

    ut_log_push("config");

    /* Collect configuration files for current path */
    ut_ll config_files = bake_config_find_configs(".");
    if (!config_files) {
        config_files = bake_config_find_configs("~");
    }

    if (config_files) {
        /* Load configurations */
        if (bake_config_load_configs(
            config_files,
            cfg_out,
            cfg_id,
            env_id))
        {
            goto error;
        }
    } else {
        ut_info(
            "config:environment '%s:%s' not found in path, load default config",
            cfg_id, env_id);

        char *bake_home = ut_envparse("~/bake");
        if (!bake_home) {
            ut_throw(NULL);
            goto error;
        }

        ut_try (ut_setenv("BAKE_HOME", "%s", bake_home), NULL);
        ut_try (ut_setenv("BAKE_TARGET", "%s", bake_home), NULL);

        free(bake_home);
    }

    ut_setenv("BAKE_CONFIG", cfg_id);
    ut_setenv("BAKE_ENVIRONMENT", env_id);
    ut_setenv("BAKE_PLATFORM", UT_PLATFORM_STRING);

    cfg_out->id = ut_strdup(cfg_id);

    cfg_out->target = ut_asprintf(
        "%s/%s-%s",
        ut_getenv("BAKE_TARGET"),
        ut_getenv("BAKE_PLATFORM"),
        cfg_out->id);

    cfg_out->home = ut_strdup(ut_getenv("BAKE_HOME"));

    cfg_out->target_lib = ut_asprintf("%s/lib", cfg_out->target);
    cfg_out->target_bin = ut_asprintf("%s/bin", cfg_out->target);

    /* Set environment variables for searching binaries and libs */
    bake_config_setEnv(cfg_out);

    if (ut_log_verbosityGet() <= UT_OK) {
        ut_log_push("environment");

        ut_iter it = ut_ll_iter(cfg_out->variables);
        while (ut_iter_hasNext(&it)) {
            char *env = ut_iter_next(&it);
            ut_ok("set '%s' to '%s'", env, ut_getenv(env));
        }

        ut_log_pop();

        ut_log_push("configuration");
        ut_ok("set '%s' to '%s'", CFG_SYMBOLS, cfg_out->symbols ? "true" : "false");
        ut_ok("set '%s' to '%s'", CFG_DEBUG, cfg_out->debug ? "true" : "false");
        ut_ok("set '%s' to '%s'", CFG_OPTIMIZATIONS, cfg_out->optimizations ? "true" : "false");
        ut_ok("set '%s' to '%s'", CFG_COVERAGE, cfg_out->coverage ? "true" : "false");
        ut_ok("set '%s' to '%s'", CFG_STRICT, cfg_out->strict ? "true" : "false");
        ut_log_pop();
    }


    ut_log_pop();
    return 0;
error:
    ut_log_pop();
    return -1;
}
