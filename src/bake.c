
#include "bake.h"

#define PARSE_OPTION(short, long, action)\
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

/* Global variables for static project configuration (from command line) */
static char *id = NULL;
static char *path = NULL;
static char *sources = NULL; /* 'src' is added by default */
static char *includes = "include";
static char *language = "c";
static char *kind = "executable";
static char *artefact = NULL;
static char *use = NULL;
static char *args = NULL;
static char *foreach_cmd = NULL;
static bool managed = false;
static bool public = true;
static bool skip_preinstall = false;
static bool skip_uninstall = false;
static bool showLines = true;
static bool showTime = false;
static bool showDelta = false;
static bool showProc = false;
static bool mute_foreach = true;
static bool profile = false;
static bool local = false;
static bool mono = false;
static char *action = "build";
static char *env = "default";
static char *cfg = "debug";
static bool dont_build = false;

/* Pointer to global builtin configuration */
static bake_config config;

/* Combination of all paths specified on command line */
static corto_ll paths = NULL;

corto_tls BAKE_LANGUAGE_KEY;
corto_tls BAKE_FILELIST_KEY;
corto_tls BAKE_PROJECT_KEY;

static
void printUsage(void) {
    printf("Usage: bake [options] <command> [arguments]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h,--help                  Display this usage information\n");
    printf("\n");
    printf("  project:\n");
    printf("  -p,--path                  Specify project path\n");
    printf("  -i,--includes              Specify include paths (default: 'include')\n");
    printf("  -s,--sources               Specify source paths (default: 'src')\n");
    printf("  -l,--lang                  Specify language\n");
    printf("  -k,--kind                  Specify project kind (default: 'executable')\n");
    printf("  -a,--artefact              Specify artefact name\n");
    printf("  -u,--use                   Specify project dependencies\n");
    printf("\n");
    printf("  config:\n");
    printf("  --no-symbols               Disable symbols (default: enabled)\n");
    printf("  --no-debug                 Disable debugging (default: enabled)\n");
    printf("  --optimize                 Enable optimization (default: disabled)\n");
    printf("  --coverage                 Enable code coverage (default: disabled)\n");
    printf("  --strict                   Enable strict mode (default: disabled)\n");
    printf("\n");
    printf("  tracing:\n");
    printf("  --debug                    Set verbosity to DEBUG\n");
    printf("  --trace                    Set verbosity to TRACE\n");
    printf("  --verbosity                Set verbosity level (DEBUG, TRACE, OK, INFO, WARNING, ERROR, CRITICAL)\n");
    printf("  --log-depth                Set verbosity depth\n");
    printf("  --exit-on-exception        Exit program on exception\n");
    printf("  --abort-on-exception       Abort program on exception\n");
    printf("  --profile                  Enable profiling\n");
    printf("  --mono                     Disable colors\n");
    printf("  --show-lines               Show linenumbers of log messages\n");
    printf("  --show-time                Show time of log messages\n");
    printf("  --show-delta               Show time delta between log messages\n");
    printf("  --show-proc                Show process id\n");
    printf("  --mute                     Mute errors from loaded packages\n");
    printf("  --backtrace                Enable backtraces for tracing\n");
}

static
void set_verbosity(
    const char *verbosity)
{
    if (!verbosity) {
        return;
    }

    if (!stricmp(verbosity, "DEBUG")) {
        corto_log_verbositySet(CORTO_DEBUG);
    } else if (!stricmp(verbosity, "TRACE")) {
        corto_log_verbositySet(CORTO_TRACE);
    } else if (!stricmp(verbosity, "OK")) {
        corto_log_verbositySet(CORTO_OK);
    } else if (!stricmp(verbosity, "INFO")) {
        corto_log_verbositySet(CORTO_INFO);
    } else if (!stricmp(verbosity, "WARNING")) {
        corto_log_verbositySet(CORTO_WARNING);
    } else if (!stricmp(verbosity, "ERROR")) {
        corto_log_verbositySet(CORTO_ERROR);
    } else if (!stricmp(verbosity, "CRITICAL")) {
        corto_log_verbositySet(CORTO_CRITICAL);
    } else {
        printf("not a valid verbosity level '%s'\n", verbosity);
    }
}

static
int parseArgs(int argc, char *argv[])
{
    int i;
    bool parsed;

    for(i = 0; i < argc; i++) {
        parsed = false;
        if (argv[i][0] == '-') {
            PARSE_OPTION('h', "help", printUsage(); dont_build = true);

            PARSE_OPTION(0, "id", id = argv[i + 1]; i ++);
            PARSE_OPTION('p', "path", path = argv[i + 1]; i ++);
            PARSE_OPTION('i', "includes", includes = argv[i + 1]; i ++);
            PARSE_OPTION('s', "sources", sources = argv[i + 1]; i ++);
            PARSE_OPTION('l', "lang", language = argv[i + 1]; i ++);
            PARSE_OPTION('k', "kind", kind = argv[i + 1]; i ++);
            PARSE_OPTION('a', "artefact", artefact = argv[i + 1]; i ++);
            PARSE_OPTION('u', "use", use = argv[i + 1]; i ++);
            PARSE_OPTION(0, "args", args = argv[i + 1]; i ++);
            PARSE_OPTION(0, "managed", managed = true);
            PARSE_OPTION(0, "local", public = false);
            PARSE_OPTION(0, "skip-preinstall", skip_preinstall = true);
            PARSE_OPTION(0, "skip-uninstall", skip_uninstall = true);
            PARSE_OPTION(0, "do", foreach_cmd = argv[i + 1]; i++);
            PARSE_OPTION(0, "env", env = argv[i + 1]; i++);
            PARSE_OPTION(0, "cfg", cfg = argv[i + 1]; i++);

            PARSE_OPTION(0, "debug", corto_log_verbositySet(CORTO_DEBUG));
            PARSE_OPTION(0, "trace", corto_log_verbositySet(CORTO_TRACE));
            PARSE_OPTION('v', "verbosity", set_verbosity(argv[i + 1]); i ++);
            PARSE_OPTION(0, "log-depth", corto_log_verbositySetDepth(atoi(argv[i + 1])); i ++);
            PARSE_OPTION(0, "profile", corto_log_profile(true));
            PARSE_OPTION(0, "dont-mute-foreach", mute_foreach = false);
            PARSE_OPTION(0, "mono", mono = true);
            PARSE_OPTION(0, "show-lines", showLines = true);
            PARSE_OPTION(0, "show-time", showTime = true);
            PARSE_OPTION(0, "show-delta", showDelta = true);
            PARSE_OPTION(0, "show-proc", showProc = true);
            PARSE_OPTION(0, "exit-on-exception", corto_log_setExceptionAction(CORTO_LOG_ON_EXCEPTION_EXIT));
            PARSE_OPTION(0, "abort-on-exception", corto_log_setExceptionAction(CORTO_LOG_ON_EXCEPTION_ABORT));

            PARSE_OPTION(0, "no-symbols", config.symbols = false);
            PARSE_OPTION(0, "no-debug", config.debug = false);
            PARSE_OPTION(0, "optimize", config.optimizations = true);
            PARSE_OPTION(0, "coverage", config.coverage = true);
            PARSE_OPTION(0, "strict", config.strict = true);

            /* Setup only */
            PARSE_OPTION(0, "local", local = true);

            if (!parsed) {
                corto_throw(
                    "unknown option '%s'",
                    argv[i]);
                return -1;
            }
        } else {
            /* Extract paths from commands */
            corto_ll_append(paths, argv[i]);
        }
    }

    return i + 1;
}

static
int parseArgs_logging(int argc, char *argv[])
{
    int i;
    bool parsed;

    for(i = 0; i < argc; i++) {
        parsed = false;
        if (argv[i][0] == '-') {
            PARSE_OPTION(0, "debug", corto_log_verbositySet(CORTO_DEBUG));
            PARSE_OPTION(0, "trace", corto_log_verbositySet(CORTO_TRACE));
            PARSE_OPTION(0, "ok", corto_log_verbositySet(CORTO_OK));
            PARSE_OPTION(0, "warning", corto_log_verbositySet(CORTO_WARNING));
            PARSE_OPTION(0, "error", corto_log_verbositySet(CORTO_ERROR));
            PARSE_OPTION(0, "profile", corto_log_profile(true));
            PARSE_OPTION(0, "dont-mute-foreach", mute_foreach = false);
            PARSE_OPTION(0, "show-lines", showLines = true);
            PARSE_OPTION(0, "show-time", showTime = true);
            PARSE_OPTION(0, "show-delta", showDelta = true);
            PARSE_OPTION(0, "show-proc", showProc = true);
            PARSE_OPTION(0, "mono", mono = true);

            PARSE_OPTION(0, "exit-on-exception", corto_log_setExceptionAction(CORTO_LOG_ON_EXCEPTION_EXIT));
            PARSE_OPTION(0, "abort-on-exception", corto_log_setExceptionAction(CORTO_LOG_ON_EXCEPTION_ABORT));
        }
    }

    return 0;
}

static
int16_t bake_test_env(
    const char *env)
{
    if (!corto_getenv(env)) {
        corto_throw("environment variable '%s' is not set", env);
        return -1;
    }
    return 0;
}

static
int16_t bake_check_dependency(
    bake_project *p,
    const char *dependency,
    uint32_t artefact_modified,
    bool private)
{
    /* Don't check if this is the generated language binding
     * package, as it may still have to be generated */
    if (bake_project_is_generated_package(p, dependency)) {
        const char *fmt = private ? "use '%s' #[yellow](private)" : "use '%s'";
        corto_ok(fmt, dependency);
        goto proceed;
    }

    const char *lib = corto_locate(dependency, NULL, CORTO_LOCATE_PACKAGE);
    if (!lib) {
        corto_info("use '%s' => #[red]missing#[normal]", dependency, lib);
        corto_throw("missing dependency '%s'", dependency);
        goto error;
    }

    time_t dep_modified = corto_lastmodified(lib);

    if (!artefact_modified || dep_modified <= artefact_modified) {
        const char *fmt = private
            ? "use '%s' => '%s' #[yellow](private)"
            : "use '%s' => '%s'"
            ;
        corto_ok(fmt, dependency, lib);
    } else {
        p->artefact_outdated = true;
        const char *fmt = private
            ? "use '%s' => '%s' #[green](changed) #[yellow](private)"
            : "use '%s' => '%s' #[green](changed)"
            ;
        corto_ok(fmt, dependency, lib);
    }

proceed:
    return 0;
error:
    return -1;
}

static
int16_t bake_check_dependencies(
    bake_language *l,
    bake_project *p,
    char *artefact)
{
    time_t artefact_modified = 0;

    char *artefact_full = corto_asprintf("bin/%s-%s/%s",
        CORTO_PLATFORM_STRING, p->cfg->id, artefact);
    if  (corto_file_test(artefact_full)) {
        artefact_modified = corto_lastmodified(artefact_full);
    }
    free(artefact_full);

    if (p->use) {
        corto_iter it = corto_ll_iter(p->use);
        while (corto_iter_hasNext(&it)) {
            char *package = corto_iter_next(&it);
            if (bake_check_dependency(p, package, artefact_modified, false)) {
                goto error;
            }
        }
    }

    if (p->use_private) {
        corto_iter it = corto_ll_iter(p->use_private);
        while (corto_iter_hasNext(&it)) {
            char *package = corto_iter_next(&it);
            if (bake_check_dependency(p, package, artefact_modified, true)) {
                goto error;
            }
        }
    }

    if (p->artefact_outdated) {
        if (corto_rm(artefact)) {
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

static
int bake_action_build(bake_crawler c, bake_project* p, void *ctx) {
    bake_language *l = NULL;
    char *artefact = NULL;

    if (p->language) {
        l = bake_language_get(p->language);
        if (!l) {
            corto_throw(NULL);
            goto error;
        }
    }

    /* Step 1: clean package hierarchy */
    if (!skip_uninstall && p->public) {
        if (bake_uninstall(&config, p)) {
            corto_throw(NULL);
            goto error;
        }
    }

    /* Step 2: Copy project file to package hierarchy. After this the project
     * can be located using corto_locate. */
    if (!skip_preinstall && p->public) {
        if (bake_install(&config, p)) {
            corto_throw(NULL);
            goto error;
        }
    }

    /* Step 3: parse attributes in project configuration. May use corto_locate to
     * resolve paths. */
    corto_log_push("parse_config");
    if (bake_project_parse_attributes(p)) {
        corto_throw(NULL);
        corto_log_pop();
        goto error;
    }

    /* Step 3.5: parse attributes from dependee configurations */
    if (bake_project_parse_dependee_attributes(p)) {
        corto_throw(NULL);
        corto_log_pop();
        goto error;
    }
    corto_log_pop();

    /* Step 4: check dependencies of project. Obtaining the artefact name
     * requires the project configuration to be parsed. */
    if (l) {
        artefact = bake_language_artefact(l, p);
        if (!artefact) {
            corto_throw("could not obtain artefact for project '%s'", p->id);
            goto error;
        }

        if (bake_check_dependencies(l, p, artefact)) {
            goto error;
        }
    }

    /* Step 5: Add generated packages as dependencies to project */
    if (bake_project_add_generated_dependencies(p)) {
        goto error;
    }

    /* Step 6: if a managed project, call code generator */
    if (p->managed && p->language) {
        if (bake_language_generate(l, p, &config)) {
            corto_throw(NULL);
            goto error;
        }
    }

    /* Step 7: install custom and generated files to package hierarchy */
    if (!skip_preinstall && p->public) {
        if (bake_pre(&config, p)) {
            corto_throw(NULL);
            goto error;
        }
    }

    /* The next steps are only relevant if a language is configured */
    if (p->language) {
        bool use_existing_binary = false;

        /* If project keep_binary is true, and a binary exists in the binary
         * folder, skip build. Only an explicit 'bake rebuild' triggers a build */
        if (p->keep_binary) {
            char *artefact_full = corto_asprintf("bin/%s-%s/%s",
                CORTO_PLATFORM_STRING, p->cfg->id, artefact);
            if (corto_file_test(artefact_full)) {
                use_existing_binary = true;
                corto_ok(
                  "not building '%s' because keep_binary is true and '%s' exists",
                    p->id, artefact_full);
            }
            free(artefact_full);
        }

        /* If code generation yielded a folder with the name of the
         * project language, this is a new project that contains the
         * generated language binding api. */
        if (p->use_generated_api && p->managed) {
            int sig;
            int8_t ret;
            if (corto_file_test("c") == 1) {
                if ((sig = corto_proc_cmd("bake build c", &ret)) || ret) {
                    corto_throw(NULL);
                    goto error;
                }
            }
            if (corto_file_test("cpp") == 1) {
                if ((sig = corto_proc_cmd("bake build cpp", &ret)) || ret) {
                    corto_throw(NULL);
                    goto error;
                }
            }
        }

        /* Step 8: build sources */
        if (!use_existing_binary) {
            if (bake_language_build(l, p, &config)) {
                corto_throw("build failed");
                goto error;
            }
        }
    }

    /* Step 9: install artefact if project was rebuilt */
    if (p->public) {
        if (bake_post(p, artefact)) {
            corto_throw(NULL);
            goto error;
        }
    } else {
        corto_debug("skipping install, project is not public");
    }

    if (artefact) free(artefact);
    return 1; /* continue */
error:
    if (artefact) free(artefact);
    return 0; /* stop */
}

static
int bake_action_clean(bake_crawler c, bake_project* p, void *ctx) {

    /* Run language-specific clean routines */
    if (p->language) {
        bake_language *l = NULL;
        l = bake_language_get(p->language);
        if (!l) {
            corto_throw(NULL);
            goto error;
        }

        bake_language_clean(l, p);
    }

    /* Clear bin directory which contains the artefact */
    if (!p->keep_binary) {
        if (corto_rm("bin")) {
            goto error;
        }
    }

    /* Always clean .bake_cache directory */
    if (corto_rm(".bake_cache")) {
        goto error;
    }

    return 1; /* continue */
error:
    return 0; /* stop */
}

static
int bake_action_rebake(bake_crawler c, bake_project* p, void *ctx) {

    /* Run language-specific clean routines */
    if (p->language) {
        bake_language *l = NULL;
        l = bake_language_get(p->language);
        if (!l) {
            corto_throw(NULL);
            goto error;
        }

        bake_language_clean(l, p);
    }

    /* Clean .bake_cache directory for current environment */
    if (corto_rm(strarg(".bake_cache/obj/%s-%s", CORTO_PLATFORM_STRING, config.id)))
    {
        goto error;
    }

    if (corto_rm(strarg(".bake_cache/gen", CORTO_PLATFORM_STRING, config.id)))
    {
        goto error;
    }

    /* Only if bake is explicitly rebuilding, remove bin directory when project
     * sets keep_binary to true */
    if (p->keep_binary) {
        /* Make sure to only clean the bin directory for the current env */
        if (corto_rm(strarg("bin/%s-%s", CORTO_PLATFORM_STRING, config.id))) {
            goto error;
        }
    }

    if (!bake_action_build(c, p, ctx)) {
        goto error;
    }

    return 1; /* continue */
error:
    return 0; /* stop */
}

static
int bake_action_install(bake_crawler c, bake_project* p, void *ctx) {
    corto_log_push("install");
    corto_trace("begin");

    /* Copy package to package hierarchy */
    if (!skip_preinstall && p->public) {
        if (bake_install(&config, p)) {
            goto error;
        }

        if (bake_pre(&config, p)) {
            goto error;
        }
    }

    if (p->public) {
        if (bake_post(p, artefact)) {
            goto error;
        }
    }

    corto_log_pop();
    return 1;
error:
    corto_log_pop();
    return 0;
}

static
int bake_action_uninstall(bake_crawler c, bake_project* p, void *ctx) {
    /* Uninstall package from package hierarchy */
    return !bake_uninstall(&config, p);
}

static
int bake_action_foreach(bake_crawler c, bake_project* p, void *ctx) {
    if (foreach_cmd) {
        int8_t ret = 0;
        corto_setenv("BAKE_PROJECT_ID", p->id);
        char *cmd = corto_envparse("%s", foreach_cmd);
        int result = corto_proc_cmd(cmd, &ret);
        free(cmd);
        return !result && !ret;
    } else {
        return 1;
    }
}

static
int bake_action_getenv()
{
    const char *env;
    corto_buffer buff = CORTO_BUFFER_INIT;

    corto_buffer_append(&buff, "BAKE_HOME=%s", corto_getenv("BAKE_HOME"));
    corto_buffer_append(&buff, " BAKE_TARGET=%s", corto_getenv("BAKE_TARGET"));
    corto_buffer_append(&buff, " BAKE_VERSION=%s", corto_getenv("BAKE_VERSION"));
    corto_buffer_append(&buff, " BAKE_CONFIG=%s", corto_getenv("BAKE_CONFIG"));
    corto_buffer_append(&buff, " BAKE_PLATFORM=%s", corto_getenv("BAKE_PLATFORM"));

    if ((env = corto_getenv("BAKE_ENVIRONMENT"))) {
        corto_buffer_append(&buff, " BAKE_ENVIRONMENT=%s", env);
    }

    if (config.variables) {
        corto_iter it = corto_ll_iter(config.variables);
        while (corto_iter_hasNext(&it)) {
            char *var = corto_iter_next(&it);
            corto_buffer_append(&buff, " %s=%s", var, corto_getenv(var));
        }
    }

    if ((env = corto_getenv("LD_LIBRARY_PATH"))) {
        corto_buffer_append(&buff, " LD_LIBRARY_PATH=%s", env);
    }
    if ((env = corto_getenv("DYLD_LIBRARY_PATH"))) {
        corto_buffer_append(&buff, " DYLD_LIBRARY_PATH=%s", env);
    }
    if ((env = corto_getenv("PATH"))) {
        corto_buffer_append(&buff, " PATH=%s", env);
    }
    if ((env = corto_getenv("CLASSPATH"))) {
        corto_buffer_append(&buff, " CLASSPATH=%s", env);
    }

    printf("%s\n", corto_buffer_str(&buff));

    return 0;
}

static
int16_t bake_action_setup_project(void)
{
    if (corto_file_test("project.json")) {
        corto_throw("a project already exists in this directory");
        goto error;
    }

    bake_language *l = bake_language_get(language);
    if (!l) {
        corto_throw("language '%s' not found", language);
        goto error;
    }

    char *project_id = id;
    if (!id) {
        char *cwd = strdup(corto_cwd());
        id = strrchr(cwd, '/');
        if (id) {
            id ++;
        } else {
            id = cwd;
        }
    }

    bake_project_kind project_kind;

    if (!strcmp(kind, "package")) {
        project_kind = BAKE_PACKAGE;
    } else if (!strcmp(kind, "application")) {
        project_kind = BAKE_APPLICATION;
    } else if (!strcmp(kind, "tool")) {
        project_kind = BAKE_TOOL;
    } else if (!strcmp(kind, "library")) {
        project_kind = BAKE_PACKAGE;
        public = false;
        managed = false;
    } else if (!strcmp(kind, "executable")) {
        project_kind = BAKE_APPLICATION;
        public = false;
        managed = false;
    } else {
        corto_throw("unknown project kind '%s'", kind);
        goto error;
    }

    if (bake_language_setup_project(l, id, project_kind)) {
        goto error;
    }

    corto_info("%s project '%s' initialized, run bake to build", kind, id);

    return 0;
error:
    return -1;
}

static
int16_t bake_project_fromArguments(
    bake_crawler c)
{
    bake_project *p;

    if (corto_ll_count(paths) > 1) {
        corto_throw(
            "multiple paths specified for single project '%s'", id);
        goto error;
    }

    /* If id is specified, project config is provided on cmd line */
    if (!(p = bake_crawler_addProject(c, corto_ll_get(paths, 0)))) {
        goto error;
    }

    p->id = id;
    p->language = language;
    p->managed = managed;
    p->public = public;
    p->args = args;

    if (sources) {
        char *source_tokens = strdup(sources);
        char *tok = strtok(source_tokens, "");
        corto_ll_clear(p->sources);
        while (tok != NULL) {
            bake_project_addSource(p, tok);
            tok = strtok(NULL, ",");
        }
        free(source_tokens);
    }

    if (includes) {
        char *include_tokens = strdup(includes);
        char *tok = strtok(include_tokens, "");
        corto_ll_clear(p->includes);
        while (tok != NULL) {
            bake_project_addInclude(p, tok);
            tok = strtok(NULL, ",");
        }
        free(include_tokens);
    }

    if (use) {
        p->use = corto_ll_new();
        char *ptr = strtok(use, ",");
        do {
            corto_ll_append(p->use, corto_strdup(ptr));
        } while ((ptr = strtok(NULL, ",")));
    }

    if (!stricmp(kind, "package")) {
        p->kind = BAKE_PACKAGE;
    } else if (!stricmp(kind, "application")) {
        p->kind = BAKE_APPLICATION;
    } else if (!strcmp(kind, "tool")) {
        p->kind = BAKE_TOOL;
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_install_tool(
    const char *name)
{
    char *filename = corto_asprintf("/usr/local/bin/%s", name);
    FILE *f = fopen (filename, "w");
    if (!f) {
        corto_throw("cannot open '%s': #[red]%s#[normal]\n",
            filename,
            strerror(errno));
        goto error;
    }

    fprintf(f, "export `bake env`\n");
    fprintf(f, "exec $BAKE_HOME/$BAKE_VERSION/$BAKE_PLATFORM-$BAKE_CONFIG/bin/%s $@\n", name);
    fclose(f);

    /* Make executable for everyone */
    if (corto_setperm(filename, 0755)) {
        corto_throw("failed to set execute permissions for '%s'\n", filename);
        goto error;
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_do_action(
    bake_crawler c,
    const char *action)
{
    bake_crawler_cb action_cb;
    if (!strcmp(action, "build")) action_cb = bake_action_build;
    else if (!strcmp(action, "clean")) action_cb = bake_action_clean;
    else if (!strcmp(action, "rebake")) action_cb = bake_action_rebake;
    else if (!strcmp(action, "rebuild")) action_cb = bake_action_rebake;
    else if (!strcmp(action, "install")) action_cb = bake_action_install;
    else if (!strcmp(action, "uninstall")) action_cb = bake_action_uninstall;
    else if (!strcmp(action, "foreach")) {
        action_cb = bake_action_foreach;
    }
    else {
        corto_error("unknown action '%s'", action);
        goto error;
    }
    if (!bake_crawler_walk(c, action, action_cb, NULL)) {
        corto_throw(NULL);
        goto error;
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_init(int argc, char* argv[])
{
    /* Initialize base library */
    corto_platform_init(argv[0]);

    /* Initialize thread key for language */
    if (corto_tls_new(&BAKE_LANGUAGE_KEY, NULL)) {
        goto error;
    }

    /* Initialize thread key for filelist */
    if (corto_tls_new(&BAKE_FILELIST_KEY, NULL)) {
        goto error;
    }

    /* Initialize thread key for filelist */
    if (corto_tls_new(&BAKE_PROJECT_KEY, NULL)) {
        goto error;
    }

    if (corto_getenv("BAKE_CONFIG")) {
        cfg = corto_getenv("BAKE_CONFIG");
    }

    if (corto_getenv("BAKE_ENVIRONMENT")) {
        env = corto_getenv("BAKE_ENVIRONMENT");
    }

    int offset = 1;

    if (argc > 1) {
        if (argv[1][0] != '-') {
            action = argv[1];
            offset ++;
        }

        /* Parse logging arguments before loading config */
        if (parseArgs_logging(argc - offset, &argv[offset]) == -1) {
            corto_raise();
            printUsage();
            goto error;
        }
    }

    if (bake_config_load(&config, cfg, env)) {
        goto error;
    }

    if (argc > 1) {
        if (parseArgs(argc - offset, &argv[offset]) == -1) {
            corto_raise();
            printUsage();
            goto error;
        }
    }

    /* If dont_build is set, exit here */
    if (dont_build) {
        exit(0);
    }

    if (showLines) {
        char *fmt = corto_asprintf("%s %s", "%f:%l", corto_log_fmtGet());
        corto_log_fmt(fmt);
        free(fmt);
    }

    if (showTime) {
        corto_log_fmt(
            strarg("%s %s",
                "%T",
                corto_log_fmtGet())
        );
    }

    if (showDelta) {
        corto_log_fmt(
            strarg("%s %s",
                "%d",
                corto_log_fmtGet())
        );
    }

    if (showProc) {
        char *fmt = corto_asprintf("%s %s", "%A", corto_log_fmtGet());
        corto_log_fmt(fmt);
        free(fmt);
    }

    if (mono) {
        corto_log_useColors(false);
    }

    if (!strcmp(action, "foreach")) {
        /* If action is foreach, mute all non-error tracing from bake */
        if (mute_foreach) {
            corto_log_verbositySet(CORTO_ERROR);
        }
    }

    return 0;
error:
    return -1;
}

int main(int argc, char* argv[]) {
    char *path_tokens = NULL;
    bool root_bake = false;

    paths = corto_ll_new();

    if (bake_init(argc, argv)) {
        goto error;
    }

    bake_crawler c = bake_crawler_new(&config);

    /* Verify environment variables */
    if (bake_test_env("BAKE_HOME")) goto error;
    if (bake_test_env("BAKE_TARGET")) goto error;

    /* Initialize loader */
    corto_load_init(
        corto_getenv("BAKE_TARGET"),
        corto_getenv("BAKE_HOME"),
        corto_getenv("BAKE_VERSION"),
        NULL,
        false);

    corto_log_fmt("%C %V %m");

    /* Bake commands that do not use the build engine */
    if (action) {
        if (!strcmp(action, "setup")) {
            return bake_setup(argv[0], local);
        } else if (!strcmp(action, "env") || !strcmp(action, "getenv")) {
            return bake_action_getenv();
        } else if (!strcmp(action, "install-script")) {
            return bake_setup_globalScript();
        } else if (!strcmp(action, "install-tool")) {
            if (!argv[2]) {
                corto_throw("missing argument for 'install-tool'");
                goto error;
            }
            return bake_install_tool(argv[2]);
        } else if (!strcmp(action, "init")) {
            if (bake_action_setup_project()) {
                corto_throw(NULL);
                goto error;
            }
            return 0;
        } else {

            /* If not a known action to bake, interpret argument as project so
             * a user can type 'bake corto' */
            if (strcmp(action, "build") &&
                strcmp(action, "clean") &&
                strcmp(action, "rebake") &&
                strcmp(action, "rebuild") &&
                strcmp(action, "install") &&
                strcmp(action, "foreach"))
            {
                corto_ll_append(paths, action);
                action = "build";
            }
        }
    }

    /* Parse path arguments for additional paths */
    if (path) {
        path_tokens = strdup(path);
        char *tok = strtok(path_tokens, ",");
        while (tok != NULL) {
            corto_ll_append(paths, tok);
            tok = strtok(NULL, ",");
        }
    }

    /* Build string that contains all paths */
    corto_buffer path_string_buff = CORTO_BUFFER_INIT;
    char *path_string = NULL;
    corto_iter it = corto_ll_iter(paths);
    int i = 0;
    while (corto_iter_hasNext(&it)) {
        if (i) {
            corto_buffer_appendstr(&path_string_buff, ",");
        }
        corto_buffer_append(&path_string_buff, "%s", corto_iter_next(&it));
        i++;
    }
    path_string = corto_buffer_str(&path_string_buff);
    if (!path_string) {
        path_string = corto_strdup("<current directory>");
    }

    if (!corto_ll_count(paths)) {
        corto_ll_append(paths, ".");
    }

    corto_log_push("init");
    corto_trace("$BAKE_HOME = '%s'", corto_getenv("BAKE_HOME"));
    corto_trace("$BAKE_TARGET = '%s'", corto_getenv("BAKE_TARGET"));

    if (corto_log_verbosityGet() <= CORTO_TRACE) {
        corto_iter it = corto_ll_iter(paths);
        while (corto_iter_hasNext(&it)) {
            char *path = corto_iter_next(&it);
            corto_trace("searching '%s'", path);
        }
    }
    corto_log_pop();

    if (id) {
        if (bake_project_fromArguments(c)) {
            goto error;
        }
    } else {
        /* Crawl specified directories (default is current) */
        corto_iter it = corto_ll_iter(paths);
        while (corto_iter_hasNext(&it)) {
            char *path = corto_iter_next(&it);
            if (bake_crawler_search(c, path)) {
                goto error;
            }
        }
    }

    if (!bake_crawler_count(c)) {
        corto_log("no projects found in '%s'\n", path_string);
        return 0;
    }

    if ((!action || strcmp(action, "foreach")) && !corto_getenv("BAKE_BUILDING")) {
        corto_setenv("BAKE_BUILDING", "");
        root_bake = true;
    }

    if (bake_do_action(c, action)) {
        corto_throw(NULL);
        goto error;
    }

    /* Cleanup resources */
    bake_crawler_free(c);
    corto_platform_deinit();

    if (path_tokens) free(path_tokens);
    if (paths) corto_ll_free(paths);
    if (path_string) free(path_string);

    return 0;
error:
    corto_platform_deinit();
    return -1;
}
