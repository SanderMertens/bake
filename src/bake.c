
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
static char *kind = "package";
static char *artefact = NULL;
static char *use = NULL;
static char *args = NULL;
static char *foreach_cmd = NULL;
static bool managed = false;
static bool public = true;
static bool skip_preinstall = false;
static bool skip_uninstall = false;
static bool showTime = false;

/* Pointer to global builtin configuration */
static bake_config config;

/* Combination of all paths specified on command line */
static corto_ll paths = NULL;

corto_tls BAKE_LANGUAGE_KEY;
corto_tls BAKE_FILELIST_KEY;
corto_tls BAKE_PROJECT_KEY;

static
int parseArgs(int argc, char *argv[])
{
    int i;
    bool parsed;

    for(i = 0; i < argc; i++) {
        parsed = false;
        if (argv[i][0] == '-') {
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

            PARSE_OPTION(0, "debug", corto_log_verbositySet(CORTO_DEBUG));
            PARSE_OPTION(0, "trace", corto_log_verbositySet(CORTO_TRACE));
            PARSE_OPTION(0, "ok", corto_log_verbositySet(CORTO_OK));
            PARSE_OPTION(0, "warning", corto_log_verbositySet(CORTO_WARNING));
            PARSE_OPTION(0, "error", corto_log_verbositySet(CORTO_ERROR));
            PARSE_OPTION(0, "show-time", showTime = true);

            PARSE_OPTION(0, "no-symbols", config.symbols = false);
            PARSE_OPTION(0, "no-debug", config.debug = false);
            PARSE_OPTION(0, "optimize", config.optimizations = true);
            PARSE_OPTION(0, "coverage", config.coverage = true);
            PARSE_OPTION(0, "strict", config.strict = true);

            if (!parsed) {
                corto_throw("unknown option '%s' (use bake --help to see available options)\n", argv[i]);
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
int16_t bake_test_env(const char *env) {
    if (!corto_getenv(env)) {
        corto_throw("environment variable '%s' is not set", env);
        return -1;
    }
    return 0;
}

static
int16_t bake_check_dependencies(
    bake_language *l,
    bake_project *p,
    char *artefact)
{
    time_t artefact_modified = 0;
    time_t project_modified = 0;

    if (corto_file_test("project.json")) {
        project_modified = corto_lastmodified("project.json");
    }

    if  (corto_file_test(artefact)) {
        artefact_modified = corto_lastmodified(artefact);
    }

    if (artefact_modified < project_modified) {
        if (artefact_modified) {
            corto_ok("cfg 'project.json' #[green](changed)#[normal]");
        }
        if (p->managed || artefact_modified) {
            p->sources_outdated = true;
        }
    }

    /* For each package, if use_generated_api is enabled, also include
     * the package that contains the generated api for the language of
     * the package */
    if (p->use_generated_api) {
        int i = 0, count = corto_ll_size(p->use);
        corto_iter it = corto_ll_iter(p->use);
        while (corto_iter_hasNext(&it)) {
            char *package = corto_iter_next(&it);
            char *lastElem = strrchr(package, '/');
            i ++;
            if (lastElem) {
                lastElem ++;
                /* Don't try to find a language specific
                 * package if this is already a language
                 * specific package */
                if (!strcmp(lastElem, p->language)) {
                    continue;
                }
            }

            /* Insert language-specific package with generated
             * API if it exists */
            char *lib = corto_locate(
                strarg("%s/%s", package, p->language),
                NULL,
                CORTO_LOCATION_LIB);
            if (lib) {
                corto_ll_append(
                    p->use,
                    corto_asprintf(
                        "%s/%s", package, p->language));
            }

            /* Reached end of list- don't process packages that
             * we just added */
            if (i == count) {
                break;
            }
        }
    }

    if (p->use) {
        corto_iter it = corto_ll_iter(p->use);
        while (corto_iter_hasNext(&it)) {
            char *package = corto_iter_next(&it);

            /* Don't check if this is the generated language binding
             * package, as it may still have to be generated */
            if (p->managed && p->language) {
                if (!strcmp(package, strarg("%s/c", p->id))) {
                    corto_ok("use '%s'",
                        package);
                    continue;
                }
            }

            char *lib = corto_locate(package, NULL, CORTO_LOCATION_LIB);
            if (!lib) {
                corto_info("use '%s' => #[red]missing#[normal]",
                    package,
                    lib);
                corto_throw("missing dependency '%s'", package);
                goto error;
            }

            time_t dep_modified = corto_lastmodified(lib);

            if (!artefact_modified || dep_modified <= artefact_modified) {
                corto_ok("use '%s' => '%s'",
                    package,
                    lib);
            } else {
                p->artefact_outdated = true;
                corto_ok("use '%s' => '%s' #[green](changed)#[normal]",
                    package,
                    lib);

            }
            free(lib);
        }
    }

    if (p->sources_outdated) {
        if (!artefact_modified) {
            corto_trace("old artefact '%s' not found, starting from clean slate",
                artefact);
        }

        if (bake_language_clean(l, p)) {
            goto error;
        }
    } else if (p->artefact_outdated) {
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

        artefact = bake_language_artefact(l, p);
        if (!artefact) {
            corto_throw("could not obtain artefact for project '%s'", p->id);
            goto error;
        }

        if (bake_check_dependencies(l, p, artefact)) {
            goto error;
        }
    } else {
        if (corto_file_test("src")) {
            corto_warning("found 'src' directory but no language configured. add language to 'project.json'");
        }
    }

    /* Step 1: clean package hierarchy */
    if (!skip_uninstall) {
        if (bake_uninstall(p)) {
            goto error;
        }
    }

    /* Step 3: if managed, generate code */
    if (p->managed && p->language) {
        if (bake_language_generate(l, p, &config)) {
            goto error;
        }
    }

    /* Step 2: pre-install files to package hierarchy */
    if (!skip_preinstall) {
        if (bake_pre(p)) {
            goto error;
        }
    }

    /* The next steps are only relevant if a language is configured */
    if (p->language) {

        /* Step 4: build sources */
        if (bake_language_build(l, p, &config)) {
            corto_throw("build failed");
            goto error;
        }

        /* Step 5: install artefact if project was rebuilt */
        if (artefact) {
            if (bake_post(p, artefact)) {
                goto error;
            }
        }
    }

    if (artefact) free(artefact);
    return 1; /* continue */
error:
    if (artefact) free(artefact);
    return 0; /* stop */
}

static
int bake_action_clean(bake_crawler c, bake_project* p, void *ctx) {

    if (p->language) {
        bake_language *l = NULL;
        l = bake_language_get(p->language);
        if (!l) {
            corto_throw(NULL);
            goto error;
        }

        bake_language_clean(l, p);
    }

    return 1; /* continue */
error:
    return 0; /* stop */
}

static
int bake_action_rebuild(bake_crawler c, bake_project* p, void *ctx) {

    if (!bake_action_clean(c, p, ctx)) {
        goto error;
    }

    if (!bake_action_build(c, p, ctx)) {
        goto error;
    }

    return 1; /* continue */
error:
    return 0; /* stop */
}

static
int bake_action_copy(bake_crawler c, bake_project* p, void *ctx) {
    corto_log_push("copy");
    corto_trace("begin");

    /* Copy package to package hierarchy */
    if (!skip_preinstall) {
        if (bake_pre(p)) {
            goto error;
        }
    }

    if (artefact != NULL) {
        if (bake_post(p, artefact)) {
            goto error;
        }
    }

    corto_ok("done");
    corto_log_pop();
    return 1;
error:
    corto_log_pop();
    return 0;
}

static
int bake_action_uninstall(bake_crawler c, bake_project* p, void *ctx) {
    /* Uninstall package from package hierarchy */
    return !bake_uninstall(p);
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
int bake_action_install(bake_crawler c, bake_project* p, void *ctx) {
    corto_log_push("install");
    corto_trace("begin");

    char *oldenv = corto_getenv("CORTO_TARGET");

    /* Uninstall from current environment */
    corto_trace("uninstall project from '%s'", oldenv);
    if (!bake_action_uninstall(c, p, ctx)) {
        goto error;
    }

    /* Clean project */
    if (!bake_action_clean(c, p, ctx)) {
        goto error;
    }

    /* Install to global environment */
    corto_trace("install project to '" BAKE_GLOBAL_ENV "'");
    corto_setenv("CORTO_TARGET", BAKE_GLOBAL_ENV);
    if (!bake_action_build(c, p, ctx)) {
        goto error;
    }

    corto_setenv("CORTO_TARGET", oldenv);
    corto_log_pop();
    return 1;
error:
    corto_setenv("CORTO_TARGET", oldenv);
    corto_log_pop();
    return 0;
}

int main(int argc, char* argv[]) {
    const char *action = "build";
    char *path_tokens = NULL;
    int last_parsed = 0;
    bool root_bake = false;

    paths = corto_ll_new();

    corto_log_fmt("%V %F:%L (%R) %C: %m");

    /* Initialize base library */
    base_init(argv[0]);

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

    /* Verify environment variables */
    if (bake_test_env("CORTO_HOME")) goto error;
    if (bake_test_env("CORTO_TARGET")) goto error;

    config = (bake_config){
        .symbols = true,
        .debug = true,
        .optimizations = false,
        .coverage = false,
        .strict = false
    };

    if (argc > 1) {
        int offset = 1;
        if (argv[1][0] != '-') {
            action = argv[1];
            offset ++;
        }

        last_parsed = parseArgs(argc - offset, &argv[offset]);
        if (last_parsed == -1) {
            goto error;
        }
    }

    if (showTime) {
        corto_log_fmt(
            strarg("%s %s",
                "%T",
                corto_log_fmtGet())
        );
    }

    if (action) {
        if (!strcmp(action, "setup")) {
            return bake_setup();
        } else if (!strcmp(action, "foreach")) {
            /* If action is foreach, mute all non-error tracing from bake */
            corto_log_verbositySet(CORTO_ERROR);
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

    if ((!action || strcmp(action, "foreach")) && !corto_getenv("BAKE_BUILDING")) {
        corto_print("🍰 #[bold]baking in#[normal] #[magenta]%s#[normal]\n", path_string);
        corto_setenv("BAKE_BUILDING", "");
        root_bake = true;
    }
    free(path_string);

    if (!corto_ll_size(paths)) {
        corto_ll_append(paths, ".");
    }

    corto_log_push("init");
    corto_trace("$CORTO_HOME = '%s'", corto_getenv("CORTO_HOME"));
    corto_trace("$CORTO_TARGET = '%s'", corto_getenv("CORTO_TARGET"));

    if (corto_log_verbosityGet() <= CORTO_TRACE) {
        corto_iter it = corto_ll_iter(paths);
        while (corto_iter_hasNext(&it)) {
            char *path = corto_iter_next(&it);
            corto_trace("searching '%s'", path);
        }
    }
    corto_log_pop();

    /* Create crawler for finding corto projects */
    bake_crawler c = bake_crawler_new();

    if (id) {
        bake_project *p;

        if (corto_ll_size(paths) > 1) {
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
            while (tok != NULL) {
                bake_project_addSource(p, tok);
                tok = strtok(NULL, ",");
            }
            free(source_tokens);
        }

        if (includes) {
            char *include_tokens = strdup(includes);
            char *tok = strtok(include_tokens, "");
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
        corto_throw("no projects found");
        goto error;
    }

    if (!action) {
        /* If after parsing arguments no actions were defined, execute the default
         * action. */
        if (!bake_crawler_walk(c, "build", bake_action_build, NULL)) {
            goto error;
        }
    } else {
        bake_crawler_cb action_cb;
        if (!strcmp(action, "build")) action_cb = bake_action_build;
        else if (!strcmp(action, "clean")) action_cb = bake_action_clean;
        else if (!strcmp(action, "rebuild")) action_cb = bake_action_rebuild;
        else if (!strcmp(action, "copy")) action_cb = bake_action_copy;
        else if (!strcmp(action, "install")) {
            config.symbols = false;
            config.debug = false;
            config.optimizations = true;
            action_cb = bake_action_install;
        }
        else if (!strcmp(action, "uninstall")) action_cb = bake_action_uninstall;
        else if (!strcmp(action, "foreach")) {
            action_cb = bake_action_foreach;
        }
        else {
            corto_error("unknown action '%s'", action);
            goto error;
        }
        if (!bake_crawler_walk(c, action, action_cb, NULL)) {
            goto error;
        }
    }

    if (root_bake) {
        corto_print("#[bold]🎂 done!#[normal]\n");
    }

    /* Cleanup resources */
    bake_crawler_free(c);
    base_deinit();

    if (path_tokens) free(path_tokens);
    if (paths) corto_ll_free(paths);

    return 0;
error:
    base_deinit();
    return -1;
}
