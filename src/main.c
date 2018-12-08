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

ut_tls BAKE_DRIVER_KEY;
ut_tls BAKE_FILELIST_KEY;
ut_tls BAKE_PROJECT_KEY;

const char *cfg = "debug";
const char *env = "default";
const char *action = "build";
const char *path = NULL;
bool build = true;

#define ARG(short, long, action)\
    if (argv[i][0] == '-') {\
        if (argv[i][1] == '-') {\
            if (long && !strcmp(&argv[i][2], long ? long : "")) {\
                action;\
                parsed = true;\
            }\
        } else {\
            if (short && argv[i][1] == short) {\
                action;\
                parsed = true;\
            }\
        }\
    }

void bake_usage(void)
{
    printf("Usage: bake [options] <command> <path>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h,--help                  Display this usage information\n");
    printf("  -v,--version               Display version information\n");
    printf("\n");
    printf("  --cfg <configuration>      Specify configuration id\n");
    printf("  --env <environment>        Specify environment id\n");
    printf("\n");
    printf("  --trace                    Set verbosity to TRACE\n");
    printf("  -v,--verbosity <kind>      Set verbosity level (DEBUG, TRACE, OK, INFO, WARNING, ERROR, CRITICAL)\n");
    printf("\n");
    printf("Commands:\n");
    printf("  build                      Build a project\n");
    printf("  rebuild                    Clean and build a project\n");
    printf("  clean                      Clean a project\n");
    printf("  export                     Copy project files to bake environment\n");
    printf("  uninstall                  Remove project files from bake environment\n");
    printf("  env                        Echo bake environment\n");
    printf("\n");

    build = false;
}

void bake_version(void)
{
    printf("bake version 2.0 (%s %s %s)\n",
        UT_PLATFORM_STRING,
        __DATE__,
        __TIME__);

    build = false;
}

static
void bake_set_verbosity(
    const char *verbosity)
{
    if (!verbosity) {
        return;
    }

    if (!stricmp(verbosity, "DEBUG")) {
        ut_log_verbositySet(UT_DEBUG);
    } else if (!stricmp(verbosity, "TRACE")) {
        ut_log_verbositySet(UT_TRACE);
    } else if (!stricmp(verbosity, "OK")) {
        ut_log_verbositySet(UT_OK);
    } else if (!stricmp(verbosity, "INFO")) {
        ut_log_verbositySet(UT_INFO);
    } else if (!stricmp(verbosity, "WARNING")) {
        ut_log_verbositySet(UT_WARNING);
    } else if (!stricmp(verbosity, "ERROR")) {
        ut_log_verbositySet(UT_ERROR);
    } else if (!stricmp(verbosity, "CRITICAL")) {
        ut_log_verbositySet(UT_CRITICAL);
    } else {
        printf("not a valid verbosity level '%s'\n", verbosity);
    }
}

bool bake_is_action(
    const char *arg)
{
    if (!strcmp(arg, "build") ||
        !strcmp(arg, "rebuild") ||
        !strcmp(arg, "clean") ||
        !strcmp(arg, "export") ||
        !strcmp(arg, "uninstall"))
    {
        return true;
    }

    if (!strcmp(arg, "env")) {
        build = false;
        return true;
    } else {
        return false;
    }
}

int bake_parse_args(
    int argc,
    const char *argv[])
{
    int i = 1;

    const char *arg = argv[i];
    if (arg && arg[0] != '-') {
        if (bake_is_action(arg)) {
            action = arg;
            i ++;
            arg = argv[i];
        }
    }

    for (; i < argc; i ++) {
        if (argv[i][0] == '-') {
            bool parsed = false;
            ARG(0, "env", env = argv[i + 1]; i ++);
            ARG(0, "cfg", cfg = argv[i + 1]; i ++);

            ARG(0, "trace", ut_log_verbositySet(UT_TRACE));
            ARG('v', "verbosity", bake_set_verbosity(argv[i + 1]); i ++);

            ARG('h', "help", bake_usage(); i ++);
            ARG('v', "version", bake_version(); i ++);

            if (!parsed) {
                ut_throw(
                    "unknown option %s (use --help to list available options)",
                    argv[i]);
                return -1;
            }
        } else {
            if (!path) {
                path = argv[i];
            } else {
                ut_throw("more than one path provided (current = %s, arg = %s)",
                    path, argv[i]);
                return -1;
            }
        }
    }

    if (!path) {
        path = ".";
    }

    return 0;
}

bake_crawler* bake_discovery(
    bake_config *config)
{
    bake_crawler *crawler = bake_crawler_new(config);
    uint32_t project_count = 0;

    /* Discover projects */
    project_count = bake_crawler_search(crawler, path);
    if (!project_count) {
        ut_log("no projects found in '%s'\n", path);
        bake_crawler_free(crawler);
        crawler = NULL;
    }

    return crawler;
}

int bake_build(
    bake_config *config,
    bake_crawler *crawler,
    const char *action)
{
    bake_crawler_cb cb;

    if (!strcmp(action, "build")) cb = bake_do_build;
    else if (!strcmp(action, "clean")) cb = bake_do_clean;
    else if (!strcmp(action, "rebuild")) cb = bake_do_rebuild;
    else if (!strcmp(action, "export")) cb = bake_do_export;
    else if (!strcmp(action, "uninstall")) cb = bake_do_uninstall;
    else if (!strcmp(action, "foreach")) cb = bake_do_foreach;
    else {
        ut_error("unknown action '%s'", action);
        goto error;
    }

    /* Walk projects in correct dependency order */
    ut_try( bake_crawler_walk(config, crawler, action, cb), NULL);

    return 0;
error:
    return -1;
}

int bake_env(
    bake_config *config)
{
    const char *env;
    ut_strbuf buff = UT_STRBUF_INIT;
    int32_t count = 0;

    if (config->env_variables) {
        ut_iter it = ut_ll_iter(config->env_variables);
        while (ut_iter_hasNext(&it)) {
            char *var = ut_iter_next(&it);
            if (count) {
                ut_strbuf_appendstr(&buff, " ");
            }
            ut_strbuf_append(&buff, "%s=%s", var, ut_getenv(var));
            count ++;
        }
    }

    char *str = ut_strbuf_get(&buff);

    printf("%s\n", str);

    free(str);

    return 0;
}

int main(int argc, const char *argv[]) {
    bake_config config = {};

    ut_init("bake");

    /* Init thread keys, which are used to pass arguments in driver API */
    ut_try (ut_tls_new(&BAKE_DRIVER_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_FILELIST_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_PROJECT_KEY, NULL), NULL);

    ut_log_push("init");
    ut_try (bake_parse_args(argc, argv), NULL);
    ut_trace("configuration: %s", cfg);
    ut_trace("environment: %s", env);
    ut_trace("path: %s", path);
    ut_trace("action: %s", action);
    ut_log_pop();

    ut_log_push("config");
    ut_try (bake_config_load(&config, cfg, env), NULL);
    ut_log_pop();

    if (build) {
        /* If build is true, first discover projects in provided path */
        ut_log_push("discovery");
        bake_crawler *crawler = bake_discovery(&config);
        ut_log_pop();

        /* If projects have been discovered, build them */
        if (crawler) {
            ut_log_push("build");
            bake_build(&config, crawler, action);
            ut_log_pop();

            bake_crawler_free(crawler);
        }
    } else {
        /* Actions that don't need project discovery */
        if (!strcmp(action, "env")) {
            ut_try( bake_env(&config), NULL);
        }
    }

    ut_deinit();
    return 0;
error:
    ut_deinit();
    return -1;
}
