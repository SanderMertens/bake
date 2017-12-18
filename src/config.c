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
int16_t bake_config_loadEnvironment(
    JSON_Object *envcfg)
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

        corto_ok("set $%s to '%s'", var, value);

        /* Shell environment takes precedence */
        if (!corto_getenv(var)) {
            if (corto_setenv(var, value)) {
                goto error;
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
            corto_ok("set '%s' to '%s'", name, *out ? "true" : "false");
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
int16_t bake_config_setPathVariables(void)
{
    char *path = corto_getenv("PATH");
    if (!path || !strlen(path)) {
        corto_setenv("PATH", strarg("~/.bake:%s/bin", corto_getenv("BAKE_HOME")));
    } else {
        corto_setenv(
            "PATH", strarg("~/.bake:%s/bin:%s",
                    corto_getenv("BAKE_HOME"), path));
    }

    char *ld = corto_getenv("LD_LIBRARY_PATH");
    if (!ld || !strlen(ld)) {
        corto_setenv(
            "LD_LIBRARY_PATH", strarg(".:%s/lib", corto_getenv("BAKE_HOME")));
    } else {
        corto_setenv(
            "LD_LIBRARY_PATH",
                strarg(".:%s/lib:%s", corto_getenv("BAKE_HOME"), ld));
    }

    return 0;
}

static
int16_t bake_config_parse (
    const char *file,
    bake_config *cfg_out,
    const char *cfg_name,
    const char *env_name)
{
    corto_trace("parse configuration from '%s'", file);

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
        if (bake_config_loadEnvironment(section)) {
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

    /* Set environment variables for searching binaries and libs */
    if (bake_config_setPathVariables()) {
        goto error;
    }

    return 0;
not_found:
    return 1;
error:
    return -1;
}

int16_t bake_config_load(
    bake_config *cfg_out,
    const char *cfg,
    const char *env)
{
    corto_log_push("config");

    char *file = bake_config_findFile(NULL);
    if (file) {
        int ret = 0;
        while (file && (ret = bake_config_parse(file, cfg_out, cfg, env)) && (ret == 1)) {
            /* Traverse up directories to find other configuration file */
            file = bake_config_findFile(file);
        }
        if (ret == -1) {
            corto_throw(NULL);
            goto error;
        } else if (!file) {

        } else {
            cfg_out->id = cfg;
        }
    }

    if (!file) {
        corto_log(
            "config:environment '%s:%s' found in path, load default config",
            cfg, env);

        /* Use default configuration and environment */
        *cfg_out = (bake_config){
            .id = cfg,
            .symbols = true,
            .debug = true,
            .optimizations = false,
            .coverage = false,
            .strict = false
        };
        char *bake_home = corto_envparse("~/.corto");
        if (!bake_home) {
            corto_throw(NULL);
            goto error;
        }

        if (corto_setenv("BAKE_HOME", "%s", bake_home)) {
            corto_throw(NULL);
            goto error;
        }
        if (corto_setenv("BAKE_TARGET", "%s", bake_home)) {
            corto_throw(NULL);
            goto error;
        }
        free(bake_home);
    }

    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
}
