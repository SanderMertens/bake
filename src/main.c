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
ut_tls BAKE_CONFIG_KEY;

/* Bake configuration */
const char *cfg = "debug";
const char *env = "default";
const char *action = "build";
const char *path = ".";
bool discover = true;
bool build = true;
bool build_to_home = false;
bool local_setup = false;
bool strict = false;
bool optimize = false;

/* Command line project configuration */
const char *id = NULL;
bake_project_type type = BAKE_APPLICATION;
const char *artefact = NULL;
const char *includes = NULL;
const char *language = NULL;

/* Command specific parameters */
const char *export_expr = NULL;
const char *publish_cmd = NULL;
bool interactive = false;
int run_argc = 0;
bool clean_missing = false;
const char **run_argv = NULL;
const char *foreach_cmd = NULL;
const char *list_filter = NULL;

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
    printf("  -h,--help                    Display this usage information\n");
    printf("  -v,--version                 Display version information\n");
    printf("\n");
    printf("  --cfg <configuration>        Specify configuration id\n");
    printf("  --env <environment>          Specify environment id\n");
    printf("  --build-to-home              Build to BAKE_HOME instead of BAKE_TARGET\n");
    printf("  --strict                     Manually enable strict compiler options\n");
    printf("  --optimize                   Manually enable compiler optimizations\n");
    printf("\n");
    printf("  --id <project id>            Manually specify a project id\n");
    printf("  --type <package|application> Manually specify a project type (default = \"application\")\n");
    printf("  --package                    Manually set the project type to package\n");
    printf("  --language <language>        Manually specify a language for project (default = \"c\")\n");
    printf("  --artefact <binary>          Manually specify a binary file for project\n");
    printf("  --includes <include path>    Manually specify an include path for project\n");
    printf("\n");
    printf("  --interactive                Rebuild project when files change (use w/run)\n");
    printf("  -a,--args [arguments]        Pass arguments to application (use w/run)\n");
    printf("  --missing                    Uninstall projects with missing binaries or errors (use w/uninstall)\n");
    printf("\n");
    printf("  -v,--verbosity <kind>        Set verbosity level (DEBUG, TRACE, OK, INFO, WARNING, ERROR, CRITICAL)\n");
    printf("  --trace                      Set verbosity to TRACE\n");
    printf("\n");
    printf("Commands:\n");
    printf("  init [path]                  Initialize new bake project\n");
    printf("  run [path|project id]         Build & run project\n");
    printf("  build [path]                 Build a project (default command)\n");
    printf("  rebuild [path]               Clean and build a project\n");
    printf("  clean [path]                 Clean a project\n");
    printf("  publish <patch|minor|major>  Publish new project version\n");
    printf("  install [path]               Install project to bake environment\n");
    printf("  uninstall [project id]       Remove project from bake environment\n");
    printf("  clone <git url>              Clone and build git repository and dependencies\n");
    printf("  update [project id]          Update an installed package or application\n");
    printf("  foreach <cmd>                Run command for each discovered project\n");
    printf("\n");
    printf("  env                          Echo bake environment\n");
    printf("  upgrade                      Upgrade to new bake version\n");
    printf("  export <NAME>=|+=<VALUE>     Add variable to bake environment\n");
    printf("\n");
    printf("  locate <package id>          Locate and diagnose a project in the bake environment\n");
    printf("  list [filter]                List packages in bake environment\n");
    printf("\n");
    printf("Examples:\n");
    printf("  bake                         Build all projects discovered in current directory\n");
    printf("  bake my_app                  Build all projects discovered in my_app directory\n");
    printf("  bake init                    Initialize new application project in current directory\n");
    printf("  bake init my_app             Initialize new application project in directory my_app\n");
    printf("  bake init my_lib --package   Initialize new package project in directory my_lib\n");
    printf("  bake run my_app -a hello     Run my_app project, pass 'hello' as argument\n");
    printf("  bake publish major           Increase major project version, create git tag\n");
    printf("  bake locate foo.bar          Locate package foo.bar\n");
    printf("  bake list foo.*              List all packages that start with foo.\n");
    printf("\n");
}

void bake_version(void)
{
    printf("bake version 2.1.2 (%s %s %s)\n",
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

int16_t bake_init_action(
    const char *arg)
{
    if (!strcmp(arg, "build") ||
        !strcmp(arg, "rebuild") ||
        !strcmp(arg, "clean") ||
        !strcmp(arg, "install") ||
        !strcmp(arg, "update") ||
        !strcmp(arg, "clone"))
    {
        discover = true;
        build = true;
        return 0;
    }

    if (!strcmp(arg, "env") ||
        !strcmp(arg, "setup") ||
        !strcmp(arg, "init") ||
        !strcmp(arg, "run") ||
        !strcmp(arg, "uninstall") ||
        !strcmp(arg, "upgrade") ||
        !strcmp(arg, "publish") ||
        !strcmp(arg, "locate") ||
        !strcmp(arg, "list") ||
        !strcmp(arg, "export") ||
        !strcmp(arg, "unset"))
    {
        discover = false;
        build = false;
        return 0;
    }

    if (!strcmp(arg, "foreach")) {
        discover = true;
        build = false;
        return 0;
    }

    return -1;
}

int bake_parse_args(
    int argc,
    const char *argv[])
{
    int i = 1;

    /* Parse action (if any) */

    const char *arg = argv[i];
    if (arg && arg[0] != '-') {
        if (!bake_init_action(arg)) {
            action = arg;
            i ++;
            arg = argv[i];
        }
    }

    /* Parse remainder of arguments */

    for (; i < argc; i ++) {
        if (argv[i][0] == '-') {
            bool parsed = false;
            ARG(0, "env", env = argv[i + 1]; i ++);
            ARG(0, "cfg", cfg = argv[i + 1]; i ++);
            ARG(0, "build-to-home", build_to_home = true; i ++);
            ARG(0, "strict", strict = true; i ++);
            ARG(0, "optimize", optimize = true; i ++);

            ARG(0, "trace", ut_log_verbositySet(UT_TRACE));
            ARG('v', "verbosity", bake_set_verbosity(argv[i + 1]); i ++);

            ARG(0, "id", id = argv[i + 1]; i ++);
            ARG(0, "type", ut_try(!(type = bake_parse_project_type(argv[i + 1])), NULL); i ++);
            ARG(0, "package", type = BAKE_PACKAGE);
            ARG(0, "language", language = argv[i + 1]; i ++);
            ARG(0, "artefact", artefact = argv[i + 1]; i ++);
            ARG(0, "includes", includes = argv[i + 1]; i ++);

            ARG(0, "local-setup", local_setup = true; i ++);
            ARG(0, "interactive", interactive = true; i ++);
            ARG(0, "missing", clean_missing = true; i ++);
            ARG('a', "args", run_argc = argc - i; run_argv = &argv[i + 1]; break);

            ARG('h', "help", bake_usage(); action = NULL; i ++);
            ARG('v', "version", bake_version(); action = NULL; i ++);

            if (!parsed) {
                ut_throw(
                    "unknown option %s (use --help to list available options)",
                    argv[i]);
                return -1;
            }
        } else {
            if (!strcmp(action, "foreach")) {
                 if (!foreach_cmd) {
                    foreach_cmd = argv[i];
                } else if (!strcmp(path, ".")) {
                    path = foreach_cmd;
                    foreach_cmd = argv[i];
                } else {
                    path = argv[i];
                }
            } else {
                path = argv[i];
            }
        }
    }

    /* Certain options like --help and --version don't require further action */
    if (!action) {
        return 0;
    }

    /* Set command-specific variables & do input checking */

    bool path_set = strcmp(path, ".");

    if (!strcmp(action, "install")) {
        if (ut_file_test(path) != 1) {
            action = "install_remote";
            discover = false;
        }
    }

    else if (!strcmp(action, "export") || !strcmp(action, "unset")) {
        if (!path_set) {
            ut_throw("missing expression for export command");
            goto error;
        }
        export_expr = path;
    }

    else if (!strcmp(action, "publish")) {
        if (!path_set) {
            ut_throw("missing publish command (specify patch, minor or major)");
            goto error;
        } else {
            if (strcmp(path, "patch") && strcmp(path, "minor") &&
                strcmp(path, "major"))
            {
                ut_throw(
                    "invalid publish command (expected patch, minor or major)");
                goto error;
            }
        }

        publish_cmd = path;
    }

    else if (!strcmp(action, "foreach") && !foreach_cmd) {
        foreach_cmd = path;
    }

    else if (!strcmp(action, "list") && path_set) {
        list_filter = path;
    }

    else if (!strcmp(action, "uninstall") && path_set) {
        id = path;
    }

    /* If artefact is manually specified, translate to platform specific name */
    if (artefact) {
        if (type == BAKE_PACKAGE) {
            artefact = ut_asprintf("lib%s" UT_OS_LIB_EXT, artefact);
        }
    }

    return 0;
error:
    return -1;
}

/* Create project from manually specified parameters */
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

/* Discover all projects in a directory */
int16_t bake_discovery(
    bake_config *config)
{
    uint32_t project_count = 0;

    if (!id) {
        /* Discover projects */
        project_count = bake_crawler_search(config, path);
        if (project_count == -1) {
            goto error;
        }

        if (!project_count) {
            ut_log("no projects found in '%s'\n", path);
        }
    } else {
        language = "none";
        bake_project *project = bake_project_from_cmdline(config);
        ut_try( bake_crawler_add(config, project), NULL);
    }

    return 0;
error:
    return -1;
}

/* Build all discovered projects */
int bake_build(
    bake_config *config,
    const char *action)
{
    bake_crawler_cb cb;

    if (!strcmp(action, "build")) cb = bake_do_build;
    else if (!strcmp(action, "clean")) cb = bake_do_clean;
    else if (!strcmp(action, "rebuild")) cb = bake_do_rebuild;
    else if (!strcmp(action, "install")) cb = bake_do_install;
    else {
        ut_error("unknown action '%s'", action);
        goto error;
    }

    /* Walk projects in correct dependency order */
    ut_try( bake_crawler_walk(config, action, cb), NULL);

    return 0;
error:
    return -1;
}

/* Print environment to stdout */
int bake_env(
    bake_config *config)
{
    const char *env;
    ut_strbuf buff = UT_STRBUF_INIT;

    if (config->env_variables) {
        ut_iter it = ut_ll_iter(config->env_variables);
        while (ut_iter_hasNext(&it)) {
            char *var = ut_iter_next(&it);
            ut_strbuf_append(&buff, "%s=%s\n", var, ut_getenv(var));
        }
    }

    char *str = ut_strbuf_get(&buff);
    printf("%s\n", str);
    free(str);

    return 0;
}

/* Initialize a new bake project */
int bake_init_project(
    bake_config *config)
{
    if (path && strcmp(path, ".")) {
        /* Project id was parsed as path */
        char *project_path = ut_strdup(path);

        /* Replace '.' with '-' for path */
        char *ptr = project_path, ch;
        for (; (ch = *ptr); ptr ++) {
            if (ch == '.') {
                *ptr = '-';
            }
        }

        /* Create & move to new path */
        ut_try( ut_mkdir(project_path), NULL);
        ut_try( ut_chdir(project_path), NULL);
        free(project_path);

        /* Project id is provided path */
        id = path;

        /* When creating project from path, use current directory */
        path = ".";

    } else {
        /* Best guess project id from current working directory */
        char *cwd = ut_cwd();
        char *last_elem = strrchr(cwd, '/');
        if (last_elem && last_elem[1]) {
            id = last_elem + 1;
        } else {
            id = cwd;
        }

        /* The output of ut_cwd may change */
        id = ut_strdup(id);
    }

    /* Test if a project already exists in the current working directory */
    if (ut_file_test("project.json") == 1) {
        ut_throw("project already exists in '%s'", ut_cwd());
        goto error;
    }

    /* Create project from command line (and computed) options */
    bake_project *project = bake_project_from_cmdline(config);
    if (!project) {
        goto error;
    }

    if (!project->language) {
        free(project->language);
        project->language = ut_strdup("c");
    }

    /* Setup all project files, include invoking driver */
    ut_try( bake_project_setup(config, project), NULL);

    /* Install project metadata to bake env so it is discoverable by id */
    ut_try (bake_install_metadata(config, project), NULL);

    return 0;
error:
    return -1;
}

/* Publish a new version for a project */
int bake_publish_project(
    bake_config *config)
{
    if (ut_file_test("project.json") != 1) {
        ut_throw("directory '%s' does not contain a bake project", ut_cwd());
        goto error;
    }

    printf("This command creates a new tag. Commit all your changes"
           " before continuing!\n Proceed? [y/N] ");
    char answer = getchar();
    if (answer != 'y' && answer != 'Y') {
        return 0;
    }

    bake_project *project = bake_project_new(".", config);
    if (!project) {
        goto error;
    }

    ut_try(bake_publish(config, project, publish_cmd), NULL);

    return 0;
error:
    return -1;
}

int bake_locate(
    bake_config *config,
    const char *id,
    const char *env,
    bool test_lib,
    bake_project_type *type_out,
    bool clean_missing)
{
    const char *bin = ut_locate(id, NULL, UT_LOCATE_BIN);
    if (!bin) {
        const char *path = ut_locate(id, NULL, UT_LOCATE_PROJECT);
        if (!path) {
            ut_log("?  #[normal]%s #[red]!not found!#[normal]\n", id);
            goto error;
        } else {
            bake_project *project = bake_project_new(path, config);
            if (!project) {
                ut_log("?  #[normal]%s #[grey]=> #[cyan]%s#[normal] #[red]!invalid config!\n",
                    id, env ? env : path);
                goto error;
            } else {
                if (project->language) {
                    if (project->type == BAKE_APPLICATION) {
                        ut_log(
                          "A  %s #[grey]=> #[cyan]%s#[normal] #[red]!missing binary!\n",
                          id, env ? env : path);
                        goto error;
                    } else {
                        ut_log(
                          "P  %s #[grey]=> #[cyan]%s#[normal] #[red]!missing binary!\n",
                          id, env ? env : path);
                        goto error;
                    }
                    bake_project_free(project);
                    goto error;
                } else {
                    if (!clean_missing) {
                        ut_log("C  #[normal]%s #[grey]=> #[cyan]%s#[normal]\n",
                            id, env ? env : path);
                    }
                    bake_project_free(project);

                    if (type_out) *type_out = BAKE_PACKAGE;
                }
            }
        }
    } else {
        const char *lib = ut_locate(id, NULL, UT_LOCATE_LIB);
        if (lib) {
            ut_dl dl = NULL;
            if (test_lib) {
                dl = ut_dl_open(bin);
            }
            if (test_lib && !dl) {
                ut_log("P  #[normal]%s #[grey]=> #[cyan]%s#[normal] #[red]!load error!\n",
                    id, env ? env : bin);
                ut_log("   %s\n", ut_dl_error());
                goto error;
            } else {
                if (!clean_missing) {
                    ut_log("P  #[normal]%s #[grey]=> #[cyan]%s#[normal]\n",
                        id, env ? env : bin);
                }
                if (dl) ut_dl_close(dl);
                if (type_out) *type_out = BAKE_PACKAGE;
            }
        } else if (!clean_missing) {
            const char *lib_static = ut_locate(id, NULL, UT_LOCATE_STATIC);
            if (lib_static) {
                ut_log("S  #[normal]%s #[grey]=> #[cyan]%s#[normal]\n",
                    id, env ? env : bin);
                if (type_out) *type_out = BAKE_PACKAGE;
            } else {
                ut_log("A  #[normal]%s #[grey]=> #[cyan]%s#[normal]\n",
                    id, env ? env : bin);
                if (type_out) *type_out = BAKE_APPLICATION;
            }
        }
    }

    return 0;
error:
    if (clean_missing) {
        ut_log("#[grey]uninstall '%s'#[normal]\n", id);
        bake_install_uninstall(config, id);
    }
    return -1;
}

typedef struct env_package {
    char *id;
    bool in_home;
    bool in_target;
} env_package;

int env_package_find(
    void *id,
    void *ptr)
{
    env_package *package = ptr;
    return strcmp(package->id, id);
}

int env_package_compare(
    const void *ptr1,
    const void *ptr2)
{
    const env_package *package1 = ptr1;
    const env_package *package2 = ptr2;
    return strcmp(package1->id, package2->id);
}

char* bake_print_env(
    bake_config *config,
    env_package *package)
{
    if (package->in_home && package->in_target) {
        return "#[green]BAKE_HOME#[normal],#[cyan]BAKE_TARGET";
    } else if (package->in_home) {
        return "#[green]BAKE_HOME";
    } else if (package->in_target) {
        return "#[cyan]BAKE_TARGET";
    } else {
        return "???";
    }
}

int bake_list(
    bake_config *config,
    bool clean_missing)
{
    ut_ll packages = ut_ll_new();
    ut_iter it;
    bake_project_type type;

    uint32_t total = 0, package_count = 0, app_count = 0, error_count = 0;

    /* Collect packages from BAKE_HOME */
    char *home_meta = ut_asprintf("%s/meta", config->home);
    ut_try( ut_dir_iter(home_meta, "/*", &it), NULL);
    while (ut_iter_hasNext(&it)) {
        char *id = ut_iter_next(&it);
        env_package *ep = ut_calloc(sizeof(env_package));
        ep->id = ut_strdup(id);
        ep->in_home = true;
        ut_ll_append(packages, ep);
    }

    /* Collect packages from BAKE_TARGET */
    char *target_meta = ut_asprintf("%s/meta", config->target);
    if (ut_file_test(target_meta) == 1) {
        ut_try( ut_dir_iter(target_meta, "/*", &it), NULL);
        while (ut_iter_hasNext(&it)) {
            char *id = ut_iter_next(&it);

            env_package *ep = ut_ll_find(packages, env_package_find, id);
            if (!ep) {
                ep = ut_calloc(sizeof(env_package));
                ep->id = ut_strdup(id);
                ut_ll_append(packages, ep);
            }

            ep->in_target = true;
        }
    } else {
        ut_trace("no bake target environment detected");
    }

    /* Copy packages to array so they can be sorted with qsort */
    env_package *buffer = malloc(sizeof(env_package) * ut_ll_count(packages));
    uint32_t i = 0;
    it = ut_ll_iter(packages);
    while (ut_iter_hasNext(&it)) {
        env_package *ep = ut_iter_next(&it);
        buffer[i ++] = *ep;
        free(ep);
    }

    /* Sort array */
    qsort(buffer, i, sizeof(env_package), env_package_compare);

    for (i = 0; i < ut_ll_count(packages); i ++) {
        env_package *package = &buffer[i];

        if (list_filter) {
            if (!ut_expr(list_filter, package->id)) {
                continue;
            }
        }

        if (!bake_locate(
            config, package->id, bake_print_env(config, package), true, &type,
            clean_missing))
        {
            if (type == BAKE_APPLICATION) {
                app_count ++;
            } else {
                package_count ++;
            }
        } else {
            error_count ++;
        }

        free(buffer[i].id);
    }

    printf("\n");
    if (error_count) {
        ut_log("#[normal]applications: %d, packages: %d, #[red]errors:#[normal] %d\n",
            app_count, package_count, error_count);
    } else {
        ut_log("#[normal]applications: %d, packages: %d\n", app_count, package_count);
    }
    printf("\n");

    ut_ll_free(packages);
    free(home_meta);
    free(target_meta);
    free(buffer);

    return 0;
error:
    return -1;
}

int bake_foreach_action(
    bake_config *config,
    bake_project* project)
{
    int8_t ret = 0;
    ut_setenv("BAKE_PROJECT_ID", project->id);
    ut_chdir(project->fullpath);

    char *cmd = ut_envparse("%s", foreach_cmd);

    int result = ut_proc_cmd(cmd, &ret);
    free(cmd);
    return result || ret;
}

int main(int argc, const char *argv[]) {
    if (ut_getenv("BAKE_CONFIG")) {
        cfg = ut_getenv("BAKE_CONFIG");
    }
    if (ut_getenv("BAKE_ENVIRONMENT")) {
        env = ut_getenv("BAKE_ENVIRONMENT");
    }

    bake_config config = {
        .configuration = cfg,
        .environment = env,
        .symbols = true,
        .debug = true,
        .optimizations = false,
        .coverage = false,
        .strict = false
    };

    srand (time(NULL));

    ut_init("bake");

    ut_log_fmt("%C %V %m");

    /* Init thread keys, which are used to pass arguments in driver API */
    ut_try (ut_tls_new(&BAKE_DRIVER_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_FILELIST_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_PROJECT_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_CONFIG_KEY, NULL), NULL);

    ut_tls_set(BAKE_CONFIG_KEY, &config);

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

    if (strict) {
        config.strict = true;
    }
    if (optimize) {
        config.optimizations = true;
    }

    ut_log_push("config");
    ut_try (bake_config_load(&config, cfg, env, build_to_home), NULL);
    ut_log_pop();

    /* Initialize package loader */
    ut_load_init(
        ut_getenv("BAKE_TARGET"),
        ut_getenv("BAKE_HOME"),
        ut_getenv("BAKE_CONFIG"));

    /* Initialize crawler */
    bake_crawler_init();

    if (discover) {
        /* If discover is true, first discover projects in provided path */
        ut_log_push("discovery");
        bake_project *project = NULL;

        if (!strcmp(action, "clone")) {
            ut_try( bake_clone(&config, path), NULL);
        } else if (!strcmp(action, "update")) {
            ut_try( bake_update(&config, path), NULL);
        }

        uint32_t count = bake_crawler_count();
        if (count) {
            action = "build";
        } else {
            ut_try( bake_discovery(&config), "discovery failed");
        }

        ut_log_pop();

        /* If projects have been discovered, build them */
        if (build) {
            ut_log_push("build");
            ut_try(bake_build(&config, action), NULL);
            ut_log_pop();
        } else {
            if (!strcmp(action, "foreach")) {
                ut_try( bake_crawler_walk(
                    &config, action, bake_foreach_action), NULL);
            }
        }
    } else {
        /* Actions that don't need project discovery */
        if (!strcmp(action, "env")) {
            ut_try( bake_env(&config), NULL);
        } else if (!strcmp(action, "setup")) {
            ut_try (bake_setup(argv[0], local_setup), NULL);
        } else if (!strcmp(action, "init")) {
            ut_try (bake_init_project(&config), NULL);
        } else if (!strcmp(action, "run")) {
            ut_try (
              bake_run(&config, path, interactive, run_argc, run_argv), NULL);
        } else if (!strcmp(action, "publish")) {
            ut_try (bake_publish_project(&config), NULL);
        } else if (!strcmp(action, "uninstall")) {
            if (!clean_missing) {
                ut_try (bake_install_uninstall(&config, id), NULL);
            } else {
                ut_try (bake_list(&config, true), NULL);
            }
        } else if (!strcmp(action, "locate")) {
            bake_locate(&config, path, NULL, true, NULL, false);
        } else if (!strcmp(action, "list")) {
            bake_list(&config, false);
        } else if (!strcmp(action, "export")) {
            ut_try (bake_config_export(&config, export_expr), NULL);
        } else if (!strcmp(action, "unset")) {
            ut_try (bake_config_unset(&config, export_expr), NULL);
        } else if (!strcmp(action, "upgrade")) {
            ut_log("#[bold]Cannot upgrade bake while bake is running\n");
            printf("  This is likely happening because the bake environment is exported. To\n");
            printf("  upgrade bake, either start a new terminal with a clean environment and\n");
            printf("  try again, or run this command from this terminal:\n");
            printf("    /usr/local/bin/bake upgrade\n");
        } else {
            ut_throw("invalid command '%s'", action);
            goto error;
        }
    }

    /* Cleanup crawler */
    bake_crawler_free();

    ut_deinit();
    return 0;
error:
    ut_deinit();
    return -1;
}
