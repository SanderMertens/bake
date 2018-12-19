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

/* Bake configuration */
const char *cfg = "debug";
const char *env = "default";
const char *action = "build";
const char *path = ".";
bool build = true;
bool build_to_home = false;
bool local_setup = false;

/* Command line project configuration */
const char *id = NULL;
bake_project_type type = BAKE_PACKAGE;
const char *artefact = NULL;
const char *includes = NULL;
const char *language = NULL;

/* Environment variable export */
const char *export_expr = NULL;

#define ARG(short, long, action)\
    if (i < argc) {\
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
    printf("  --build-to-home            Install to BAKE_HOME instead of BAKE_TARGET\n");
    printf("\n");
    printf("  --id <project id>          Manually specify a project id\n");
    printf("  --type <project type>      Manually specify a project type (default = \"package\")\n");
    printf("  --language <language>      Manually specify a language for project (default = \"c\")\n");
    printf("  --artefact <binary>        Manually specify a binary file for project\n");
    printf("  --includes <include path>  Manually specify an include path for project\n");
    printf("\n");
    printf("  --trace                    Set verbosity to TRACE\n");
    printf("  -v,--verbosity <kind>      Set verbosity level (DEBUG, TRACE, OK, INFO, WARNING, ERROR, CRITICAL)\n");
    printf("\n");
    printf("Commands:\n");
    printf("  build                      Build a project\n");
    printf("  rebuild                    Clean and build a project\n");
    printf("  clean                      Clean a project\n");
    printf("  install                    Install project to bake environment\n");
    printf("  uninstall                  Remove project files from bake environment\n");
    printf("  env                        Echo bake environment\n");
    printf("  export NAME=|+=VALUE       Add variable to bake environment\n");
    printf("\n");
}

void bake_version(void)
{
    printf("bake version 2.0 (%s %s %s)\n",
        UT_PLATFORM_STRING,
        __DATE__,
        __TIME__);
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

bake_project_type bake_parse_project_type(
    const char *type)
{
    if (!strcmp(type, "application")) return BAKE_APPLICATION;
    if (!strcmp(type, "package")) return BAKE_PACKAGE;
    if (!strcmp(type, "tool")) return BAKE_TOOL;
    ut_throw("'%s' is not a valid project kind", type);
    return 0;
}

bool bake_is_action(
    const char *arg)
{
    if (!strcmp(arg, "build") ||
        !strcmp(arg, "rebuild") ||
        !strcmp(arg, "clean") ||
        !strcmp(arg, "install") ||
        !strcmp(arg, "uninstall") ||
        !strcmp(arg, "update") ||
        !strcmp(arg, "clone"))
    {
        return true;
    }

    if (!strcmp(arg, "env") ||
        !strcmp(arg, "setup") ||
        !strcmp(arg, "init") ||
        !strcmp(arg, "export") ||
        !strcmp(arg, "unset"))
    {
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
            ARG(0, "build-to-home", build_to_home = true; i ++);

            ARG(0, "trace", ut_log_verbositySet(UT_TRACE));
            ARG('v', "verbosity", bake_set_verbosity(argv[i + 1]); i ++);

            ARG(0, "local-setup", local_setup = true; i ++);

            ARG(0, "id", id = argv[i + 1]; i ++);
            ARG(0, "type", ut_try(!(type = bake_parse_project_type(argv[i + 1])), NULL); i ++);
            ARG(0, "language", language = argv[i + 1]; i ++);
            ARG(0, "artefact", artefact = argv[i + 1]; i ++);
            ARG(0, "includes", includes = argv[i + 1]; i ++);

            ARG('h', "help", bake_usage(); action = NULL; i ++);
            ARG('v', "version", bake_version(); action = NULL; i ++);

            if (!parsed) {
                ut_throw(
                    "unknown option %s (use --help to list available options)",
                    argv[i]);
                return -1;
            }
        } else {
            path = argv[i];
        }
    }

    if (!action) {
        return 0;
    }

    if (!path) {
        path = ".";
    }

    if (!strcmp(action, "install")) {
        if (ut_file_test(path) != 1) {
            action = "install_remote";
            build = false;
        }
    }

    else if (!strcmp(action, "export") || !strcmp(action, "unset")) {
        if (!path) {
            ut_throw("missing expression for export command");
            goto error;
        }
        export_expr = path;
    }

    if (artefact) {
        if (type == BAKE_PACKAGE) {
            artefact = ut_asprintf("lib%s" UT_OS_LIB_EXT, artefact);
        }
    }

    return 0;
error:
    return -1;
}

bake_project *bake_project_from_cmdline(
    bake_config *config)
{
    bake_project *project = bake_project_new(NULL, NULL);
    project->id = ut_strdup(id);
    project->type = type;
    project->artefact = ut_strdup(artefact);
    project->path = ut_strdup(path);
    project->public = true;
    project->freshly_baked = true;
    project->language = ut_strdup(language);
    if (includes) {
        project->includes = ut_ll_new();
        ut_ll_append(project->includes, ut_strdup(includes));
    }

    ut_try( bake_project_init(config, project), NULL);

    return project;
error:
    return NULL;
}


bake_crawler* bake_discovery(
    bake_config *config)
{
    bake_crawler *crawler = bake_crawler_new(config);
    uint32_t project_count = 0;

    if (!id) {
        /* Discover projects */
        project_count = bake_crawler_search(crawler, path);
        if (!project_count) {
            ut_log("no projects found in '%s'\n", path);
        }
    } else {
        language = "none";
        bake_project *project = bake_project_from_cmdline(config);
        ut_try(bake_crawler_add(crawler, project), NULL);
    }

    return crawler;
error:
    return NULL;
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
    else if (!strcmp(action, "install")) cb = bake_do_install;
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

int bake_init_project(
    bake_config *config)
{
    if (ut_file_test("project.json") == 1) {
        ut_throw("project already exists in '%s'", ut_cwd());
        goto error;
    }

    if (!id) {
        char *cwd = ut_cwd();
        char *last_elem = strrchr(cwd, '/');
        if (last_elem && last_elem[1]) {
            id = last_elem + 1;
        } else {
            id = cwd;
        }

        id = ut_strdup(id);
    }

    bake_project *project = bake_project_from_cmdline(config);
    if (!project) {
        goto error;
    }

    if (!project->language) {
        free(project->language);
        project->language = ut_strdup("c");
    }

    ut_try( bake_project_setup(config, project), NULL);

    return 0;
error:
    return -1;
}

int main(int argc, const char *argv[]) {
    bake_config config = {};

    ut_init("bake");

    ut_log_fmt("%C %V %m");

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

    if (!action) {
        return 0;
    }

    ut_log_push("config");
    ut_try (bake_config_load(&config, cfg, env, build_to_home), NULL);
    ut_log_pop();

    /* Initialize package loader */
    ut_load_init(
        ut_getenv("BAKE_TARGET"),
        ut_getenv("BAKE_HOME"),
        ut_getenv("BAKE_CONFIG"));

    if (build) {
        /* If build is true, first discover projects in provided path */
        ut_log_push("discovery");
        bake_crawler *crawler = NULL;
        bake_project *project = NULL;

        if (!strcmp(action, "clone")) {
            crawler = bake_clone(&config, NULL, path);
        } else if (!strcmp(action, "update")) {
            crawler = bake_update(&config, NULL, path);
        }

        if (crawler) {
            action = "build";
        } else {
            crawler = bake_discovery(&config);
        }

        ut_log_pop();

        /* If projects have been discovered, build them */
        if (crawler) {
            ut_log_push("build");
            ut_try(bake_build(&config, crawler, action), NULL);
            ut_log_pop();

            bake_crawler_free(crawler);
        } else {
            ut_throw("discovery failed");
            goto error;
        }
    } else {
        /* Actions that don't need project discovery */
        if (!strcmp(action, "env")) {
            ut_try( bake_env(&config), NULL);
        } else if (!strcmp(action, "setup")) {
            ut_try (bake_setup(local_setup), NULL);
        } else if (!strcmp(action, "init")) {
            ut_try (bake_init_project(&config), NULL);
        } else if (!strcmp(action, "export")) {
            ut_try (bake_config_export(&config, export_expr), NULL);
        } else if (!strcmp(action, "unset")) {
           ut_try (bake_config_unset(&config, export_expr), NULL);
        } else {
            ut_throw("invalid command '%s'", action);
            goto error;
        }
    }

    ut_deinit();
    return 0;
error:
    ut_deinit();
    return -1;
}
