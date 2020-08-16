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

#define BAKE_VERSION "2.5.0"

ut_tls BAKE_DRIVER_KEY;
ut_tls BAKE_FILELIST_KEY;
ut_tls BAKE_PROJECT_KEY;
ut_tls BAKE_CONFIG_KEY;

/* Bake configuration */
const char *cfg = NULL;
const char *arch = NULL;
const char *env = "default";
const char *action = "build";
const char *path = ".";
bool discover = true;
bool build = true;
bool test = false;
bool local_setup = false;
bool load_bundles = false;
bool strict = false;
bool optimize = false;
bool is_test = false;
bool to_env = false;
bool always_clone = false;
bool fast_build = false;

/* Command line project configuration */
const char *id = NULL;
bake_project_type type = BAKE_APPLICATION;
bool private = false;
bool static_lib = false;
const char *artefact = NULL;
const char *includes = NULL;
const char *language = NULL;
const char *template = NULL;
const char *output_dir = NULL;

/* Command specific parameters */
const char *export_expr = NULL;
const char *use_expr = NULL;
const char *publish_cmd = NULL;
const char *run_prefix = NULL;
const char *test_prefix = NULL;
bool interactive = false;
bool recursive = false;
int run_argc = 0;
const char **run_argv = NULL;
const char *foreach_cmd = NULL;
const char *list_filter = NULL;
bool show_repositories = false;

#define ARG(short, long, action)\
    if (i < argc) {\
        if (argv[i][0] == '-') {\
            if (long && argv[i][1] == '-') {\
                if (!strcmp(&argv[i][2], long ? long : "")) {\
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
    printf("  --arch <architecture>        Specify architecture id\n");
    printf("  --env <environment>          Specify environment id\n");
    printf("  --strict                     Manually enable strict compiler options\n");
    printf("  --optimize                   Manually enable compiler optimizations\n");
    printf("\n");
    printf("  --package                    Set the project type to package\n");
    printf("  --template                   Set the project type to template\n");
    printf("  --test                       Create a test project\n");
    printf("  --to-env                     Clone projects to the bake environment source path (use with clone)\n");
    printf("  --always-clone               Clone dependencies even if found in the bake environment (use with clone)\n");    
    printf("\n");
    printf("  --id <project id>            Specify a project id\n");
    printf("  --type <package|template>    Specify a project type (default = \"application\")\n");
    printf("  --language <language>        Specify a language for project (default = \"c\")\n");
    printf("  --artefact <binary>          Specify a binary file for project\n");
    printf("  -i,--includes <include path> Specify an include path for project\n");
    printf("  --static                     Build statically linked version of binary\n");
    printf("  --private                    Specify a project to be private (not discoverable)\n");
    printf("\n");
    printf("  -- [arguments]               Pass arguments to application (use with run)\n");
    printf("  --interactive                Rebuild project when files change (use with run)\n");
    printf("  --run-prefix                 Specify prefix command for run\n");
    printf("  --test-prefix                Specify prefix command for tests run by test\n");
    printf("  --fast                       Don't add any instrumentations to test builds\n");
    printf("  -r,--recursive               Recursively build all dependencies of discovered projects\n");
    printf("  -t [id]                      Specify template for new project\n");
    printf("  -o [path]                    Specify output directory for new projects\n");
    printf("\n");
    printf("  --show-repositories          List loaded repositories (use with list)\n");
    printf("\n");
    printf("  -v,--verbosity <kind>        Set verbosity level (DEBUG, TRACE, OK, INFO, WARNING, ERROR, CRITICAL)\n");
    printf("  --trace                      Set verbosity to TRACE\n");
    printf("  --debug                      Set verbosity to DEBUG (highest verbosity)\n");
    printf("\n");
    printf("Commands:\n");
    printf("  new [path]                   Initialize new bake project\n");
    printf("  run [path|project id]        Build & run project\n");
    printf("  build [path]                 Build a project (default command)\n");
    printf("  rebuild [path]               Clean and build a project\n");
    printf("  clean [path]                 Clean a project\n");
    printf("  test [path]                  Run tests of project\n");
    printf("  coverage [path]              Run coverage analysis for project\n");
    printf("  cleanup                      Cleanup bake environment by removing dead or invalid projects\n");
    printf("  reset                        Resets bake environment to initial state, save for bake configuration\n");
    printf("  publish <patch|minor|major>  Publish new project version\n");
    printf("  install <project id>         Install project to bake environment (repository must be known)\n");
    printf("  uninstall [project id]       Remove project from bake environment\n");
    printf("  clone <git url>              Clone and build git repository and dependencies\n");
    printf("  use <project:bundle>         Configure the environment to use specified bundle\n");
    printf("  update [project id]          Update an installed package or application\n");
    printf("  foreach <cmd>                Run command for each discovered project\n");
    printf("\n");
    printf("  env                          Echo bake environment\n");
    printf("  upgrade                      Upgrade to new bake version\n");
    printf("  export <NAME>=|+=<VALUE>     Add variable to bake environment\n");
    printf("\n");
    printf("  info <package id>            Display info on a project in the bake environment\n");
    printf("  list [filter]                List packages in bake environment\n");
    printf("\n");
    printf("Examples:\n");
    printf("  bake                         Build all projects discovered in current directory\n");
    printf("  bake my_app                  Build all projects discovered in my_app directory\n");
    printf("  bake new                     Create new application project in current directory\n");
    printf("  bake new my_app              Create new application project in directory my_app\n");
    printf("  bake new my_lib --package    Create new package project in directory my_lib\n");
    printf("  bake new my_tmpl --template  Create new template project in directory my_tmpl\n");
    printf("  bake new game -t sdl2.basic  Create new project from the sdl2.basic template\n");
    printf("  bake run my_app -a hello     Run my_app project, pass 'hello' as argument\n");
    printf("  bake publish major           Increase major project version, create git tag\n");
    printf("  bake info foo.bar            Show information about package foo.bar\n");
    printf("  bake list foo.*              List all packages that start with foo.\n");
    printf("\n");
}

void bake_version(void)
{
    printf("bake version "BAKE_VERSION" (%s %s %s)\n",
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
    if (!strcmp(type, "template")) return BAKE_TEMPLATE;
    ut_throw("'%s' is not a valid project kind", type);
    return 0;
}

int16_t bake_init_action(
    const char *arg)
{
    /* Bundles are only loaded when bake interacts with git */
    if (!strcmp(arg, "clone") ||
        !strcmp(arg, "install") ||
        !strcmp(arg, "update") ||
        !strcmp(arg, "list"))
    {
        load_bundles = true;
    }

    if (!strcmp(arg, "build") ||
        !strcmp(arg, "rebuild") ||
        !strcmp(arg, "clean") ||
        !strcmp(arg, "meta-install") ||
        !strcmp(arg, "clone") ||
        !strcmp(arg, "install"))
    {
        discover = true;
        build = true;
        return 0;
    }

    if (!strcmp(arg, "env") ||
        !strcmp(arg, "setup") ||
        !strcmp(arg, "cleanup") ||
        !strcmp(arg, "reset") ||
        !strcmp(arg, "new") ||
        !strcmp(arg, "run") ||
        !strcmp(arg, "uninstall") ||
        !strcmp(arg, "upgrade") ||
        !strcmp(arg, "publish") ||
        !strcmp(arg, "info") ||
        !strcmp(arg, "list") ||
        !strcmp(arg, "use") ||
        !strcmp(arg, "unuse") ||
        !strcmp(arg, "export") ||
        !strcmp(arg, "unset"))
    {
        discover = false;
        build = false;
        return 0;
    }

    if (!strcmp(arg, "foreach") || 
        !strcmp(arg, "test") ||
        !strcmp(arg, "coverage") ||
        !strcmp(arg, "runall") ||
        !strcmp(arg, "update")) 
    {
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
    bool action_set = false;

    for (; i < argc; i ++) {
        if (argv[i][0] == '-') {
            bool parsed = false;
            ARG(0, "env", env = argv[i + 1]; i ++);
            ARG(0, "cfg", cfg = argv[i + 1]; i ++);
            ARG(0, "arch", arch = argv[i + 1]; i ++);
            ARG(0, "strict", strict = true;);
            ARG(0, "optimize", optimize = true; i ++);

            ARG(0, "trace", ut_log_verbositySet(UT_TRACE));
            ARG(0, "debug", ut_log_verbositySet(UT_DEBUG));
            ARG('v', "verbosity", bake_set_verbosity(argv[i + 1]); i ++);

            ARG(0, "id", id = argv[i + 1]; i ++);
            ARG(0, "type", ut_try(!(type = bake_parse_project_type(argv[i + 1])), NULL); i ++);
            ARG(0, "package", type = BAKE_PACKAGE);
            ARG(0, "template", type = BAKE_TEMPLATE);
            ARG(0, "test", is_test = true);
            ARG(0, "to-env", to_env = true);
            ARG(0, "always-clone", always_clone = true);
            ARG(0, "static", static_lib = true);

            ARG(0, "private", private = true);
            ARG('t', NULL, template = argv[i + 1]; i ++);
            ARG('o', NULL, output_dir = argv[i + 1]; i ++);
            ARG(0, "language", language = argv[i + 1]; i ++);
            ARG(0, "artefact", artefact = argv[i + 1]; i ++);
            ARG(0, "includes", includes = argv[i + 1]; i ++);

            ARG(0, "local", local_setup = true);
            ARG(0, "local-setup", ); /* deprecated */
            ARG(0, "fast", fast_build = true);
            ARG(0, "run-prefix", run_prefix = argv[i + 1]; i++);
            ARG(0, "test-prefix", test_prefix = argv[i + 1]; i++);
            ARG('i', "interactive", interactive = true);
            ARG('r', "recursive", recursive = true);
            ARG('a', "args", run_argc = argc - i; run_argv = &argv[i + 1]; break);
            ARG('-', NULL, run_argc = argc - i; run_argv = &argv[i + 1]; break);
            ARG(0, "show-repositories", show_repositories = true);

            ARG('h', "help", bake_usage(); action = NULL; i ++);
            ARG('v', "version", bake_version(); action = NULL; i ++);

            if (!parsed) {
                ut_throw(
                    "unknown option %s (use --help to list available options)",
                    argv[i]);
                return -1;
            }
        } else {
            if (!action_set) {
                const char *arg = argv[i];
                if (!strcmp(arg, "init")) {
                    ut_warning("bake init command deprecated, use bake new");
                    arg = "new";
                } else if (!strcmp(arg, "locate")) {
                    ut_warning("bake locate command deprecated, use bake info");
                    arg = "info";
                }
                if (!bake_init_action(arg)) {
                    action = arg;
                    action_set = true;
                } else {
                    path = arg;
                }
            } else if (!strcmp(action, "foreach")) {
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

    if (!strcmp(action, "meta-install")) {
        if (ut_file_test(path) != 1) {
            action = "install_remote";
            discover = false;
        }
    }

    if (!strcmp(action, "test")) {
        test = true;
        cfg = "test";
    }

    else if (!strcmp(action, "export") || !strcmp(action, "unset")) {
        if (!path_set) {
            ut_throw("missing expression for export command");
            goto error;
        }
        export_expr = path;
    }
    
    else if (!strcmp(action, "use") || !strcmp(action, "unuse")) {
        if (!path_set) {
            ut_throw("missing expression for use command");
            goto error;
        }
        use_expr = path;
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

    else if (!strcmp(action, "coverage")) {
        cfg = "test";
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
    project->public = !private;
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
        project_count = bake_crawler_search(config, path, recursive);
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
    else if (!strcmp(action, "meta-install")) cb = bake_do_install;
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
#ifndef _WIN32
            ut_strbuf_append(&buff, "%s=%s\n", var, ut_getenv(var));
#else
            ut_strbuf_append(&buff, "set %s=%s\n", var, ut_getenv(var));
#endif
        }
    }

    char *str = ut_strbuf_get(&buff);
    printf("%s\n", str);
    free(str);

    return 0;
}

/* Initialize a new bake project */
int bake_new_project(
    bake_config *config)
{    
    char *orig_path = ut_strdup(ut_cwd());

    if (path && strcmp(path, ".")) {
        /* Project id was parsed as path */
        char *project_path;
        if (output_dir) {
            project_path = ut_asprintf("%s"UT_OS_PS"%s", output_dir, path);
        } else {
            project_path = ut_strdup(path);
        }

        /* Is this a test project for an existing bake project */
        if (!is_test) {
            is_test = !strcmp(path, "test") && (ut_file_test("project.json") == 1);
        } else if (ut_file_test("project.json") != 1) {
            ut_throw("must create test project in existing project directory");
            goto error;
        }

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
        if (!id) {
            id = path;
        }

        /* When creating project from path, use current directory */
        path = ".";

    } else {
        /* Best guess project id from current working directory */
        char *cwd = ut_cwd();
        char *last_elem = strrchr(cwd, UT_OS_PS[0]);
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

    /* If id is test and we're in a bake project folder, create test project */
    if (is_test) {
        if (project->type != BAKE_APPLICATION) {
            ut_throw("test projects must be applications");
            goto error;
        }

        /* Load settings of project in current directory */
        bake_project *cwproject = bake_project_new(orig_path, config);
        if (!cwproject) {
            ut_throw("cannot create test project, failed to load project.json");
            goto error;
        }

        if (cwproject->author) {
            project->author = ut_strdup(cwproject->author);
        }

        project->description = ut_asprintf(
            "Test project for %s", cwproject->id);

        /* Language of test project is the same as main project */
        project->language = ut_strdup(cwproject->language);

        /* Add dependency to project if package */
        if (cwproject->type == BAKE_PACKAGE) {
            ut_ll_append(project->use, ut_strdup(cwproject->id));
        }

        /* Test projects are not public */
        project->public = false;
    } else {
        if (!project->language) {
            free(project->language);
            project->language = ut_strdup("c");
        }
    }

    /* Setup all project files, include invoking driver */
    if (template) {
        ut_try( bake_project_setup_w_template(config, project, template), NULL);
    } else {
        ut_try( bake_project_setup(config, project, is_test), NULL);
    }

    /* Install project metadata to bake env so it is discoverable by id */
    if (project->public) {
        if (project->type == BAKE_TEMPLATE) {
            ut_try( bake_install_template(config, project), NULL);
        } else {
            ut_try (bake_install_metadata(config, project), NULL);
        }
    }

    free(orig_path);

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

int bake_info(
    bake_config *config,
    const char *id,
    const char *cfg,
    bool in_current_cfg,
    bool test_lib,
    bake_project_type *type_out,
    bool clean_missing)
{
     if (type_out) *type_out = BAKE_PACKAGE;

    const char *path = ut_locate(id, NULL, UT_LOCATE_PROJECT);
    if (!path) {
        if (!clean_missing) {
            ut_log("#[green]?#[reset]  #[normal]%s #[red]!not found!#[normal]\n", id);
            goto error;
        }
    }

    bake_project *project = bake_project_new(path, config);
    if (!project) {
        if (!clean_missing) {
            ut_log("#[green]?#[reset]  #[normal]%s #[grey]=> #[normal]%s #[red]!invalid config!\n",
                id, cfg);
        }
        goto error;
    } else {
        if (project->language) {
            if (in_current_cfg && project->type == BAKE_PACKAGE) {
                ut_dl dl = NULL;
                const char *bin = ut_locate(id, NULL, UT_LOCATE_LIB);
                if (bin) {
                    if (test_lib) {
                        dl = ut_dl_open(bin);
                    }

                    if (test_lib && !dl) {
                        if (!clean_missing) {
                            ut_log("#[green]P#[reset]  #[normal]%s #[grey]=> #[normal]%s #[red]!load error!\n",
                                id, cfg);
                            ut_log("   %s\n", ut_dl_error());
                        }
                        goto error;

                    } else {
                        if (!clean_missing) {
                            ut_log("#[green]P#[reset]  #[normal]%s #[grey]=> #[normal]%s\n",
                                id, cfg);
                        }
                    }

                    if (dl) ut_dl_close(dl);
                } else {
                    const char *lib_static = ut_locate(id, NULL, UT_LOCATE_STATIC);
                    if (lib_static) {
                        if (!clean_missing) {
                            ut_log("#[green]S#[reset]  #[normal]%s #[grey]=> #[normal]%s\n",
                                id, cfg);
                        }
                    } else {
                        if (!clean_missing) {
                            ut_log("#[green]A#[reset]  #[normal]%s #[grey]=> #[normal]%s\n",
                                id, cfg);
                            if (type_out) *type_out = BAKE_APPLICATION;
                        }
                    }
                }

            } else {
                bool error = false;
                if (!cfg) {
                    cfg = "#[red]!missing binary!";
                    error = true;
                }
                if (project->type == BAKE_APPLICATION) {
                    if (!clean_missing) {
                        ut_log( "#[green]A#[reset]  %s #[grey]=> #[normal]%s\n", id, cfg);
                    }
                    if (type_out) *type_out = BAKE_APPLICATION;

                } else {
                    if (!clean_missing) {
                        ut_log( "#[green]P#[reset]  %s #[grey]=> #[normal]%s\n", id, cfg);
                    }
                }
                if (error) {
                    goto error;
                }
            }
        } else {
            if (!clean_missing) {
                ut_log("#[green]C#[reset]  #[normal]%s #[grey]=> #[grey]all#[normal]\n", id);
            }
        }
    }

    bake_project_free(project);

    return 0;
error:
    if (clean_missing) {
        bake_install_uninstall(config, id);
    }
    return -1;
}

typedef struct env_package {
    char *id;
    char *cfg;
    uint32_t count;
    bool in_current_cfg;
} env_package;

int env_package_compare(
    const void *ptr1,
    const void *ptr2)
{
    const env_package *package1 = ptr1;
    const env_package *package2 = ptr2;
    return strcmp(package1->id, package2->id);
}

static
int16_t list_configurations(
    bake_config *config,
    env_package *package)
{
    ut_strbuf buf = UT_STRBUF_INIT;

    if (!ut_project_is_buildtool(package->id)) {
        uint32_t count = 0;

        package->in_current_cfg = false;

        ut_strbuf_appendstr(&buf, "[");
        if (ut_file_test(config->platform) == 1) {
            ut_iter it;
            ut_try (ut_dir_iter(config->platform, NULL, &it), NULL);
            while (ut_iter_hasNext(&it)) {
                char *cfg = ut_iter_next(&it);

                if (ut_project_in_config(package->id, cfg)) {
                    if (count) {
                        ut_strbuf_appendstr(&buf, ", ");
                    }
                    ut_strbuf_append(&buf, "#[green]%s#[normal]", cfg);
                    count ++;

                    if (!strcmp(cfg, config->configuration)) {
                        package->in_current_cfg = true;
                    }
                }
            }
        }

        ut_strbuf_appendstr(&buf, "]");
        if (count) {
            package->cfg = ut_strbuf_get(&buf);
        }
        package->count = count;
    } else {
        package->cfg = ut_strdup("#[grey]all#[normal]");
        package->in_current_cfg = true;
    }

    return 0;
error:
    return -1;
}

int bake_list(
    bake_config *config,
    bool clean_missing)
{
    ut_ll packages = ut_ll_new();
    ut_iter it;
    bake_project_type type;

    uint32_t total = 0, package_count = 0, app_count = 0, template_count = 0, 
             error_count = 0;

    if (!clean_missing) {
        ut_log("\n#[grey]Listing projects for platform:\n * #[normal]%s\n", UT_PLATFORM);
    }

    /* Collect packages from BAKE_HOME */
    ut_try( ut_dir_iter(UT_META_PATH, "/*", &it), NULL);
    while (ut_iter_hasNext(&it)) {
        char *id = ut_iter_next(&it);
        env_package *ep = ut_calloc(sizeof(env_package));
        ep->id = ut_strdup(id);
        list_configurations(config, ep);
        ut_ll_append(packages, ep);
    }

    if (!clean_missing && config->bundles) {
        ut_log("\n#[grey]Loaded bundles:#[normal]\n");
        it = ut_ll_iter(config->bundles);
        while (ut_iter_hasNext(&it)) {
            bake_bundle *bundle = ut_iter_next(&it);
            ut_log(" * %s:%s\n", bundle->project, bundle->id);            
        }
    }

    if (show_repositories) {
        ut_log("\n#[grey]Known repositories:#[normal]\n");
        if (config->repositories) {
            it = ut_rb_iter(config->repositories);
            while (ut_iter_hasNext(&it)) {
                bake_repository *repo = ut_iter_next(&it);
                const char *branch = repo->branch ? repo->branch : "master";
                const char *tag = repo->tag;
                const char *commit = repo->commit;

                const char *bundle = repo->bundle;
                if (!bundle) {
                    bundle = "[value.repository]";
                }

                /* Show project in different colors depending on its state in
                 * the bake environment. If it is located, test if the project
                 * is under development or not. If the project is under 
                 * development, it means that the revision for the bundle is not
                 * enforced. */
                const char *located = ut_locate(
                    repo->id, NULL, UT_LOCATE_PROJECT);

                bool in_development = false;

                if (located) {
                    const char *src = ut_locate(
                        repo->id, NULL, UT_LOCATE_SOURCE);

                    const char *dev_src = ut_locate(
                        repo->id, NULL, UT_LOCATE_DEVSRC);

                    if (!src && dev_src) {
                        in_development = true;
                    } else if (src && dev_src) {
                        in_development = strcmp(src, dev_src);
                    }
                }

                ut_log(" * #[%s]%s: #[cyan]%s#[normal] -> #[green]%s:%s#[normal] (%s:%s)\n",
                    located
                        ? in_development
                            ? "yellow"
                            : "green"
                        : "grey"
                    ,
                    repo->id, 
                    repo->url, 
                    branch, 
                    commit
                        ? commit
                        : tag
                            ? tag
                            : "latest"
                    ,
                    repo->project, 
                    bundle);   
            }    

            ut_log(
                "\n Legend: #[green][installed] #[yellow][under development] #[grey][not installed]\n");
        } else {
            ut_log(" - no known repositories\n");
        }
    }

    if (!clean_missing) {
        ut_log("\n#[grey]Packages & Applications:#[normal]\n");
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

        if (!bake_info(
            config, package->id, package->cfg, package->in_current_cfg, true, &type,
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

    /* List templates */
    if (!clean_missing) {
        if (!ut_dir_iter(UT_TEMPLATE_PATH, "/*", &it)) {
            while (ut_iter_hasNext(&it)) {
                char *id = ut_iter_next(&it);

                if (!template_count) {
                    ut_log("\n#[grey]Templates:#[normal]\n");
                }

                ut_strbuf buf = UT_STRBUF_INIT;
                ut_strbuf_appendstr(&buf, "[");

                uint32_t lang_count = 0;
                char *path = ut_asprintf("%s"UT_OS_PS"%s", UT_TEMPLATE_PATH, id);
                ut_iter lang_it;
                ut_try(ut_dir_iter(path, NULL, &lang_it), NULL);
                while (ut_iter_hasNext(&lang_it)) {
                    char *lang = ut_iter_next(&lang_it);
                    if (lang_count) {
                        ut_strbuf_appendstr(&buf, ", ");
                    }
                    ut_strbuf_append(&buf, "#[green]%s#[normal]", lang);
                    lang_count ++;
                }

                ut_strbuf_appendstr(&buf, "]");

                if (!lang_count) {
                    ut_log("#[green]T#[reset]  %s #[red]!no languages!\n", id);
                    ut_strbuf_reset(&buf);
                } else {
                    char *languages = ut_strbuf_get(&buf);
                    ut_log("#[green]T#[reset]  %s #[grey]=> #[normal]%s\n", id, languages);
                    free(languages);
                }

                template_count ++;
            }
        } else {
            ut_catch();
        }
    }

    if (!clean_missing) {
        ut_log("\n#[grey]Summary:#[normal]\n");
        if (error_count) {
            ut_info(
                "#[reset]applications: %d, packages: %d, templates: %d, #[red]errors:#[reset] %d",
                app_count, package_count, template_count, error_count);
            printf("\n");
            ut_info("run 'bake cleanup' to uninstall packages with errors");
        } else {
            ut_info("#[reset]applications: %d, packages: %d, templates: %d", 
                app_count, package_count, template_count);
        }
    }
    printf("\n");

    ut_ll_free(packages);
    free(buffer);

    return 0;
error:
    return -1;
}

int bake_reset_dir(
    bake_config *cfg,
    const char *path)
{
    ut_iter it;
    ut_try( ut_dir_iter(path, NULL, &it), NULL);

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);

        if (strncmp(file, "bake", 4)) {
            char *file_path = ut_asprintf("%s/%s", path, file);
            ut_rm(file_path);
            free(file_path);
        }
    }

    return 0;
error:
    return -1;
}

int bake_reset(
    bake_config *cfg)
{
    ut_iter it;
    ut_try( ut_dir_iter(cfg->home, NULL, &it), NULL);

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *file_path = ut_asprintf("%s/%s", cfg->home, file);

        if (ut_isdir(file_path)) {
            if (!strcmp(file, "include") || !strcmp(file, "meta") || !strcmp(file, "src")) {
                bake_reset_dir(cfg, file_path);

            } else if (strcmp(file, "lib")) {
                ut_rm(file_path);
            }
        } else if (strcmp(file, "bake.json") && 
                strcmp(file, BAKE_EXEC UT_OS_BIN_EXT) &&
                strcmp(file, "bake" UT_OS_SCRIPT_EXT) &&
                strcmp(file, "bake-upgrade" UT_OS_SCRIPT_EXT)) 
        {
            ut_rm(file_path);
        }

        free(file_path);
    }

    /* Remove bundles from configuration file, as reset will remove the projects
     * from the bake environment */
    bake_config_reset_bundles(cfg);

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

/* Test all discovered projects */
int bake_test_action(
    bake_config *config,
    bake_project *project)
{
    char *test_path = ut_asprintf("%s/test", project->path);
    int result = 0;

    if (ut_file_test(test_path) == 1) {
        int8_t rc;
        
        bake_project_test(config, project);

        char *cmd = ut_asprintf("bake runall %s --cfg test", test_path);
        int sig = ut_proc_cmd(cmd, &rc);

        if (sig || rc) {
            ut_error("command '%s' failed", cmd);
            result = -1;
        }
    }

    free (test_path);
    
    return result;
error:
    return -1;
}

/* Test all discovered projects */
int bake_runall_action(
    bake_config *config,
    bake_project *project)
{
    ut_try( 
        bake_run(config, project->path, run_prefix, false, run_argc, run_argv), 
            NULL);
    return 0;
error:
    return -1;
}

/* Run coverage for all discovered projects */
int bake_coverage_action(
    bake_config *config,
    bake_project *project)
{
    char *test_path = ut_asprintf("%s/test", project->path);

    if (ut_file_test(test_path) == 1) {
        ut_try( bake_project_coverage(config, project), NULL);
    } else {
        ut_ok("cannot run coverage for '%s', project has no tests", 
            project->id);
    }

    return 0;
error:
    return -1;
}

void bake_dump_env() {
    ut_log_push("env-dump");
    ut_iter it;
    if (!ut_dir_iter(UT_HOME_PATH, "//", &it)) {
        while (ut_iter_hasNext(&it)) {
            char *file = ut_iter_next(&it);

            if (!strstr(file, UT_OS_PS".git") && 
                !strstr(file, UT_OS_PS".bake_cache")) 
            {
                char *is_dir = "";
                if (ut_isdir( strarg("%s"UT_OS_PS"%s", UT_HOME_PATH, file))) {
                    is_dir = "(D)";
                }

                ut_debug("#[normal]%s %s", file, is_dir);
            }
        }
    } else {
        ut_catch();
        ut_debug("could not iterate over files in %s", UT_HOME_PATH);
    }
    ut_log_pop();
}

int bake_run_template(
    bake_config *config,
    const char *path,
    bool interactive,
    const char *bake_exec,
    int argc,
    const char *argv[])
{
    const char *tmpl_path = ut_locate(path, NULL, UT_LOCATE_TEMPLATE);
    if (!tmpl_path) {
        ut_throw("cannot find template '%s'", path);
        goto error;
    }

    const char *repo_id = ut_locate(path, NULL, UT_LOCATE_REPO_ID);

    /* Check temporary path, remove if already exists */
    char *tmp_path = ut_envparse("$BAKE_HOME/temp/%s", repo_id);
    if (ut_file_test(tmp_path) == 1) {
        ut_rm(tmp_path);
    }

    /* Create temporary template instance */
    char *cmd = ut_envparse(
        "%s new %s -o $BAKE_HOME/temp -t %s --private", bake_exec, path, path);
    int8_t rc = 0;
    int sig = ut_proc_cmd(cmd, &rc);
    if (rc || sig) {
        ut_throw(
            "failed to create temporary project based on template '%s'", path);
        goto error;
    }

    ut_try (
        bake_run(config, tmp_path, run_prefix, interactive, run_argc, run_argv), NULL);

    free(tmp_path);

    return 0;
error:
    return -1;
}

void bake_message(
    int kind,
    const char *bracket_txt,
    const char *fmt,
    ...)
{
    va_list args;

    va_start(args, fmt);
    char *msg = ut_vasprintf(fmt, args);
    va_end(args);

    char *color;
    if (kind == UT_OK) {
        color = "green";
    } else if (kind == UT_WARNING) {
        color = "yellow";
    } else if (kind == UT_ERROR) {
        color = "red";
    } else {
        color = "green";
    }

    ut_log("#[%s][#[normal]%*s#[%s]]#[reset] %s\n", color, 7, bracket_txt, color, msg);

    free(msg);
}

int main(int argc, const char *argv[]) {

    if (ut_getenv("BAKE_ENVIRONMENT")) {
        env = ut_getenv("BAKE_ENVIRONMENT");
    }

    srand (time(NULL));

    ut_init("bake");

    /* Init thread keys, which are used to pass arguments in driver API */
    ut_try (ut_tls_new(&BAKE_DRIVER_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_FILELIST_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_PROJECT_KEY, NULL), NULL);
    ut_try (ut_tls_new(&BAKE_CONFIG_KEY, NULL), NULL);

    ut_try (bake_parse_args(argc, argv), NULL);

    if (ut_log_verbosityGet() <= UT_DEBUG) {
        ut_log_fmt("%f:%l: %C %V %m");
    } else {
        ut_log_fmt("%C %V %m");
    }

    if (ut_log_verbosityGet() <= UT_OK) {
        printf("\n");
        ut_log_push("bake "BAKE_VERSION);
        ut_strbuf buf = UT_STRBUF_INIT;
        int i;
        for (i = 0; i < argc; i ++) {
            ut_strbuf_append(&buf, "%s ", argv[i]);
        }
        
        char *args = ut_strbuf_get(&buf);
        ut_ok("cmd  #[cyan]%s#[normal]", args);
        free(args);
        ut_ok("cwd  #[cyan]%s", ut_cwd());
        ut_log_pop();
    }

    ut_log_push("init");

    /* Initialize package loader for default home, arch, os and config */
    ut_load_init(NULL, arch, NULL, cfg);

    /* If artefact is manually specified, translate to platform specific name */
    if (artefact) {
        if (type == BAKE_PACKAGE) {
            artefact = ut_asprintf("%s%s%s", UT_LIB_PREFIX, artefact, UT_LIB_EXT);
        }
    }

    ut_trace("configuration: %s", UT_CONFIG);
    ut_trace("environment: %s", env);
    ut_trace("path: %s", path);
    ut_trace("action: %s", action);
    ut_log_pop();

    /* Dump bake environment to console if in debug */
    if (ut_log_verbosityGet() <= UT_DEBUG) {
        bake_dump_env();
    }

    bake_config config = {
        .configuration = UT_CONFIG,
        .environment = env,
        .architecture = arch,
        .symbols = true,
        .debug = true,
        .optimizations = false,
        .coverage = false,
        .strict = false,
        .static_lib = false
    };

    ut_tls_set(BAKE_CONFIG_KEY, &config);

    if (!action) {
        return 0;
    }

    if (recursive) {
        /* If this is a recursive build, load bundles in case there are
         * repositories in the dependency tree that need to be cloned */
        load_bundles = true;
    }

#ifndef UT_OS_WINDOWS
    bool is_bake_parent = !ut_getenv("BAKE_CHILD") || strcmp(ut_getenv("BAKE_CHILD"), "TRUE");
    if (is_bake_parent) {
        /* If this is a parent process, don't load bundles. This is not 
         * necessary since the parent process won't do anything with it, 
         * and it can also limit the amount of errors thrown if the module
         * configuration has issues. */
        load_bundles = false;
    }
#endif

    ut_log_push("config");
    ut_try (bake_config_load(&config, env, load_bundles), NULL);
    ut_log_pop();

    if (strict) {
        config.strict = true;
    }
    if (optimize) {
        config.optimizations = true;
    }
    if (fast_build) {
        config.coverage = false;
        config.sanitize_memory = false;
        config.sanitize_undefined = false;
    }

#ifndef UT_OS_WINDOWS
    if (is_bake_parent) {
        ut_setenv("BAKE_CHILD", "TRUE");

        ut_trace("fork bake to export environment");

        /* Fork bake, to ensure that the environment gets propagated for
         * functions like dlopen */
        ut_proc pid = ut_proc_run(argv[0], argv);
        if (!pid) {
            ut_error("failed to spawn bake child process");
            return -1;
        } else {
            int8_t rc = 0;
            int sig = ut_proc_wait(pid, &rc);
            if (sig || rc) {
                if (sig) {
                    ut_error("bake child crashed with signal %d", sig);
                } else {
                    /* If this was a clean exit with an error code, no need to
                     * repeat the error since the bake child already did */
                }
                return -1;
            }
            return 0;
        }
    }
#endif

    /* Initialize crawler */
    bake_crawler_init();

    if (discover) {
        /* If discover is true, first discover projects in provided path */
        ut_log_push("discovery");
        bake_project *project = NULL;

        if (!strcmp(action, "clone")) {
            ut_try( 
                bake_clone(&config, path, to_env, always_clone, true, NULL, NULL), 
                NULL);
        } else if (!strcmp(action, "install")) {
            ut_try( bake_install(&config, path), NULL);
        }

        uint32_t count = bake_crawler_count();
        if (count) {
            action = "build";
        } else {
            ut_try( bake_discovery(&config), "discovery failed");
            count = bake_crawler_count();
        }

        ut_log_pop();

        /* If projects have been discovered, build them */
        if (count) {
            if (build) {
                ut_log_push("build");
                ut_try(bake_build(&config, action), NULL);
                ut_log_pop();
            } else {
                if (!strcmp(action, "foreach")) {
                    ut_try( bake_crawler_walk(
                        &config, action, bake_foreach_action), NULL);
                } else if (!strcmp(action, "update")) {
                    ut_try( bake_crawler_walk(
                        &config, action, bake_update_action), NULL);
                } else if (!strcmp(action, "test")) {
                    if (test_prefix) {
                        ut_setenv("BAKE_TEST_PREFIX", test_prefix);
                    }
                    ut_try( bake_crawler_walk(
                        &config, action, bake_test_action), NULL);
                } else if (!strcmp(action, "runall")) {
                    ut_try( bake_crawler_walk(
                        &config, action, bake_runall_action), NULL);
                } else if (!strcmp(action, "coverage")) {
                    ut_try( bake_crawler_walk(
                        &config, action, bake_coverage_action), NULL);
                }
            }
        }
    } else {
        /* Actions that don't need project discovery */
        if (!strcmp(action, "env")) {
            ut_try( bake_env(&config), NULL);
        } else if (!strcmp(action, "setup")) {
            ut_try (bake_setup(&config, argv[0], local_setup), NULL);
        } else if (!strcmp(action, "new")) {
            ut_try (bake_new_project(&config), NULL);
        } else if (!strcmp(action, "run")) {
            if (type == BAKE_TEMPLATE) {
                ut_try(
                  bake_run_template(
                    &config, path, interactive, argv[0], run_argc, run_argv), 
                    NULL);
            } else {
                ut_try (
                    bake_run(&config, path, run_prefix, interactive, run_argc, run_argv), 
                    NULL);
            }
        } else if (!strcmp(action, "publish")) {
            ut_try (bake_publish_project(&config), NULL);        
        } else if (!strcmp(action, "uninstall")) {
            if (type == BAKE_TEMPLATE) {
                ut_try (bake_install_uninstall_template(&config, id), NULL);
            } else {
                ut_try (bake_install_uninstall(&config, id), NULL);
            }
        } else if (!strcmp(action, "cleanup")) {
            ut_try (bake_list(&config, true), NULL);
        } else if (!strcmp(action, "reset")) {
            ut_try (bake_reset(&config), NULL);
        } else if (!strcmp(action, "info")) {
            bake_info(&config, path, NULL, true, true, NULL, false);
        } else if (!strcmp(action, "list")) {
            bake_list(&config, false);
        } else if (!strcmp(action, "export")) {
            ut_try (bake_config_export(&config, export_expr), NULL);
        } else if (!strcmp(action, "unset")) {
            ut_try (bake_config_unset(&config, export_expr), NULL);
        } else if (!strcmp(action, "use")) {
            ut_try (bake_use(&config, use_expr, true, false), NULL);
        } else if (!strcmp(action, "unuse")) {
            ut_try (bake_unuse(&config, use_expr), NULL);            
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
    if (ut_log_verbosityGet() <= UT_DEBUG) {
        ut_error("#[red]problems occurred, dumping environment");
        bake_dump_env();
    }
    ut_deinit();
    return -1;
}
