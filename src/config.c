#include "bake.h"
#include "parson.h"

static
char* bake_config_findFile(
    char *previous_file)
{
    char *file = corto_getenv("BAKE_CONFIG_FILE");
    if (file && previous_file) {
        /* Don't go looking for files if one was explicitly specified */
        goto error;
    }

    if (!file) {
        char *cwd = previous_file;
        if (!cwd) {
            cwd = corto_strdup(corto_cwd());
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
            file = corto_asprintf("%s/.bake", cwd);

            /* If this is a .bake directory, look for config file inside of
             * directory */
            if (corto_isdir(file)) {
                char *tmp = file;
                file = corto_asprintf("%s/.bake/config.json", cwd);
                free(tmp);
            }

            if (!corto_file_test(file)) {
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
        file = corto_strdup(file);
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
    corto_ll env_set,
    const char *var)
{
    char *var_set = NULL;
    corto_iter it = corto_ll_iter(env_set);
    while (corto_iter_hasNext(&it)) {
        var_set = corto_iter_next(&it);
        if (strcmp(var_set, var)) {
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
    JSON_Object *envcfg,
    corto_ll env_set)
{
    int i;
    corto_log_push("load-env");
    for (i = 0; i < json_object_get_count(envcfg); i ++) {
        const char *var = json_object_get_name(envcfg, i);
        if (!bake_config_isVarValid(var)) {
            corto_throw("'%s' is not a valid environment variable name");
            goto error;
        }

        const char *value = json_object_get_string(envcfg, var);
        if (!value) {
            corto_throw("invalid value for environment variable '%s'", var);
            goto error;
        }

        if (env_set) {
            if (!bake_config_var_set(env_set, var)) {
                corto_ll_append(env_set, corto_strdup(var));
            }
        }

        /* Shell environment takes precedence */
        if (!corto_getenv(var)) {
            if (corto_setenv(var, value)) {
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
                    cfg_out->variables = corto_ll_new();
                }
                corto_ll_append(cfg_out->variables, corto_strdup(var));
            }
        }
    }
    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
}

#define CFG_SYMBOLS "symbols"
#define CFG_DEBUG "debug"
#define CFG_OPTIMIZATIONS "optimizations"
#define CFG_COVERAGE "coverage"
#define CFG_STRICT "strict"

static
int16_t bake_config_parseBool(
    JSON_Object *obj,
    const char *name,
    uint32_t i,
    bool *out)
{
    const char *item = json_object_get_name(obj, i);
    if (!strcmp(item, name)) {
        JSON_Value *v = json_object_get_value_at(obj, i);
        if (json_value_get_type(v) == JSONBoolean)
        {
            *out = json_value_get_boolean(v);
        } else {
            corto_throw(
                "invalid JSON: expected value of '%s' to be a boolean",
                name);
            goto error;
        }
    }
    return 0;
error:
    return -1;
}

static
int16_t bake_config_parseString(
    JSON_Object *obj,
    const char *name,
    uint32_t i,
    char **out)
{
    const char *item = json_object_get_name(obj, i);
    if (!strcmp(item, name)) {
        JSON_Value *v = json_object_get_value_at(obj, i);
        if (json_value_get_type(v) == JSONString)
        {
            const char *json_string = json_value_get_string(v);
            if (json_string) {
                *out = corto_envparse("%s", json_string);
            } else {
                *out = NULL;
            }
        } else {
            corto_throw(
                "invalid JSON: expected value of '%s' to be a string",
                name);
            goto error;
        }
    }
    return 0;
error:
    return -1;
}

static
int16_t bake_config_loadConfiguration(
    JSON_Object *cfg,
    bake_config *cfg_out)
{
    corto_log_push("load-cfg");
    int i;
    for (i = 0; i < json_object_get_count(cfg); i++) {
        if (bake_config_parseBool(cfg, CFG_SYMBOLS, i, &cfg_out->symbols)) {
            goto error;
        }
        if (bake_config_parseBool(cfg, CFG_DEBUG, i, &cfg_out->debug)) {
            goto error;
        }
        if (bake_config_parseBool(
            cfg, CFG_OPTIMIZATIONS, i, &cfg_out->optimizations))
        {
            goto error;
        }
        if (bake_config_parseBool(cfg, CFG_COVERAGE, i, &cfg_out->coverage)) {
            goto error;
        }
        if (bake_config_parseBool(cfg, CFG_STRICT, i, &cfg_out->strict)) {
            goto error;
        }
    }
    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
}

static
int16_t bake_config_findSection(
    JSON_Object *object,
    const char *lookfor,
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
                corto_throw(
                    "invalid json: expected value of '%s' to be an object",
                    name);
                goto error;
            }
            break;
        }
    }

    if (i == json_object_get_count(object)) {
        corto_trace("section '%s' not found", lookfor);
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
int16_t bake_config_setPathVariables(
    bake_config *cfg)
{
    char *path = corto_getenv("PATH");
    if (!path || !strlen(path)) {
        corto_setenv("PATH", strarg("~/.bake:%s", cfg->binpath));
    } else {
        corto_setenv(
            "PATH", strarg("~/.bake:%s:%s",
            cfg->binpath, path));
    }

    char *ld = corto_getenv("LD_LIBRARY_PATH");
    if (!ld || !strlen(ld)) {
        corto_setenv(
            "LD_LIBRARY_PATH", strarg(".:%s", cfg->libpath));
    } else {
        corto_setenv(
            "LD_LIBRARY_PATH",
                strarg(".:%s:%s", cfg->libpath, ld));
    }

    char *dyld = corto_getenv("DYLD_LIBRARY_PATH");
    if (!dyld || !strlen(dyld)) {
        corto_setenv(
            "DYLD_LIBRARY_PATH", strarg(".:%s", cfg->libpath));
    } else {
        corto_setenv(
            "DYLD_LIBRARY_PATH",
                strarg(".:%s:%s", cfg->libpath, dyld));
    }

    char *classpath = corto_getenv("CLASSPATH");
    if (!classpath || !strlen(classpath)) {
        corto_setenv(
            "CLASSPATH", strarg(".:%s/java", cfg->rootpath));
    } else {
        corto_setenv(
            "CLASSPATH",
                strarg(".:%s/java:%s", cfg->rootpath, classpath));
    }

    return 0;
}

static
int16_t bake_config_load_file (
    const char *file,
    bake_config *cfg_out,
    const char *cfg_name,
    const char *env_name,
    corto_ll env_set)
{
    corto_ok("load configuration '%s'", file);

    JSON_Value *json = json_parse_file(file);
    if (!json) {
        corto_throw("failed to parse bake configuration '%s'", file);
        goto error;
    }

    JSON_Object *jsonObj = json_value_get_object(json);
    if (!jsonObj) {
        corto_throw("invalid JSON: expected object");
        goto error;
    }

    /* Parse environment */
    JSON_Object *env = json_object_get_object(jsonObj, "environment");
    if (env) {
        JSON_Object *section = NULL;
        if (bake_config_findSection(
            env, env_name, &section) == -1)
        {
            goto error;
        }
        if (!section) {
            goto not_found;
        }
        if (bake_config_loadEnvironment(cfg_out, section, env_set)) {
            goto error;
        }
    }

    /* Parse configuration */
    JSON_Object *cfg = json_object_get_object(jsonObj, "configuration");
    if (cfg) {
        JSON_Object *section = NULL;
        if (bake_config_findSection(
            cfg, cfg_name, &section) == -1)
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
    corto_ll config_files,
    bake_config *cfg_out,
    const char *cfg_id,
    const char *env_id,
    corto_ll env_set)
{
    char *file;
    while ((file = corto_ll_takeFirst(config_files))) {
        if (bake_config_load_file(file, cfg_out, cfg_id, env_id, env_set)) {
            goto error;
        }
        free(file);
    }

    return 0;
error:
    return -1;
}

static
corto_ll bake_config_find_configs(
    char *path)
{
    corto_ll config_files = NULL; /* Collect list of all config files */
    char *cur_path = path;

    if (path[0] == '/') {
        /* Absolute path */
        cur_path = strdup(path);
    } else {
        /* Relative path */
        cur_path = corto_asprintf("%s/%s", corto_cwd(), path);
        corto_path_clean(cur_path, cur_path);
    }

    /* Check for a .bake in the current path */
    char *elem = NULL;
    do {
        char *file = corto_asprintf("%s/.bake", cur_path);
        if (corto_file_test(file) == 1) {
            if (corto_isdir(file)) {
                char *cfg = corto_asprintf("%s/config.json", file);
                if (corto_file_test(cfg)) {
                    if (!config_files) config_files = corto_ll_new();
                    corto_ll_append(config_files, cfg);
                } else {
                    free(cfg);
                }
            } else {
                if (!config_files) config_files = corto_ll_new();
                corto_ll_append(config_files, file);
            }
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
    corto_ll env_set = NULL;

    if (corto_log_verbosityGet() <= CORTO_OK) {
        /* If verbosity level is OK or less, log environment variables set in
         * the configuration to console. */
        env_set = corto_ll_new();
        corto_ll_append(env_set, corto_strdup("BAKE_HOME"));
        corto_ll_append(env_set, corto_strdup("BAKE_TARGET"));
        corto_ll_append(env_set, corto_strdup("BAKE_VERSION"));
        corto_ll_append(env_set, corto_strdup("BAKE_CONFIG"));
    }

    /* Use default configuration and environment */
    *cfg_out = (bake_config){
        .id = cfg_id,
        .symbols = true,
        .debug = true,
        .optimizations = false,
        .coverage = false,
        .strict = false
    };

    corto_log_push("config");

    /* Collect configuration files for current path */
    corto_ll config_files = bake_config_find_configs(".");
    if (config_files) {
        /* Load configurations */
        if (bake_config_load_configs(
            config_files,
            cfg_out,
            cfg_id,
            env_id,
            env_set))
        {
            goto error;
        }
    } else {
        corto_info(
            "config:environment '%s:%s' not found in path, load default config",
            cfg_id, env_id);

        char *bake_home = corto_envparse("~/corto");
        if (!bake_home) {
            corto_throw(NULL);
            goto error;
        }

        corto_try (corto_setenv("BAKE_HOME", "%s", bake_home), NULL);
        corto_try (corto_setenv("BAKE_TARGET", "%s", bake_home), NULL);

        free(bake_home);
    }

    corto_setenv("BAKE_CONFIG", cfg_id);
    corto_setenv("BAKE_ENVIRONMENT", env_id);
    corto_setenv("BAKE_PLATFORM", CORTO_PLATFORM_STRING);

    cfg_out->id = corto_strdup(cfg_id);

    cfg_out->rootpath = corto_asprintf(
        "%s/%s/%s-%s",
        corto_getenv("BAKE_TARGET"),
        corto_getenv("BAKE_VERSION"),
        corto_getenv("BAKE_PLATFORM"),
        cfg_out->id);

    cfg_out->homepath = corto_asprintf(
        "%s/%s/%s-%s",
        corto_getenv("BAKE_HOME"),
        corto_getenv("BAKE_VERSION"),
        corto_getenv("BAKE_PLATFORM"),
        cfg_out->id);

    cfg_out->libpath = corto_asprintf(
        "%s/lib", cfg_out->rootpath);

    cfg_out->binpath = corto_asprintf(
        "%s/bin", cfg_out->rootpath);

    /* Set environment variables for searching binaries and libs */
    if (bake_config_setPathVariables(cfg_out)) {
        goto error;
    }

    if (corto_log_verbosityGet() <= CORTO_OK) {
        corto_log_push("environment");

        corto_iter it = corto_ll_iter(env_set);
        while (corto_iter_hasNext(&it)) {
            char *env = corto_iter_next(&it);
            corto_ok("set '%s' to '%s'", env, corto_getenv(env));
        }

        corto_log_pop();

        corto_log_push("configuration");
        corto_ok("set '%s' to '%s'", CFG_SYMBOLS, cfg_out->symbols ? "true" : "false");
        corto_ok("set '%s' to '%s'", CFG_DEBUG, cfg_out->debug ? "true" : "false");
        corto_ok("set '%s' to '%s'", CFG_OPTIMIZATIONS, cfg_out->optimizations ? "true" : "false");
        corto_ok("set '%s' to '%s'", CFG_COVERAGE, cfg_out->coverage ? "true" : "false");
        corto_ok("set '%s' to '%s'", CFG_STRICT, cfg_out->strict ? "true" : "false");
        corto_log_pop();
    }


    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
}
