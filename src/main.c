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
    printf("  env                        Echo bake environment\n");
    printf("\n");

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
        !strcmp(arg, "clean"))
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

int bake_build(
    bake_config *config)
{
    bake_crawler *crawler = bake_crawler_new(config);

    bake_crawler_search(crawler, path);

    bake_crawler_free(crawler);

    return 0;
}

int bake_env(
    bake_config *config)
{
    const char *env;
    ut_strbuf buff = UT_STRBUF_INIT;
    int32_t count = 0;

    if (config->variables) {
        ut_iter it = ut_ll_iter(config->variables);
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

    ut_try (bake_parse_args(argc, argv), NULL);

    ut_log_push("init");
    ut_trace("configuration: %s", cfg);
    ut_trace("environment: %s", env);
    ut_trace("path: %s", path);
    ut_trace("action: %s", action);
    ut_log_pop();

    ut_try (bake_config_load(&config, cfg, env), NULL);

    if (build) {
        ut_try( bake_build(&config), NULL);
    } else {
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
