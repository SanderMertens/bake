
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
static char *path = ".";
static char *sources = "src";
static char *includes = "include";
static char *language = "c";
static char *kind = "library";
static char *artefact = NULL;
static bool managed = false;
static bool local = false;
static bool skip_preinstall = false;

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
            PARSE_OPTION('k', "kind", kind = argv[i + 1] ; i ++);
            PARSE_OPTION('a', "artefact", artefact = argv[i + 1] ; i ++);
            PARSE_OPTION(0, "managed", managed = true);
            PARSE_OPTION(0, "local", local = true);
            PARSE_OPTION(0, "skip-preinstall", skip_preinstall = true);
            
            if (!parsed) {
                corto_seterr("unknown option '%s' (use bake --help to see available options)\n", argv[i]);
                return -1;
            }
        } else {
            break;
        }
    }

    return i + 1;
}

static
int16_t bake_test_env(const char *env) {
    if (!corto_getenv(env)) {
        corto_seterr("environment variable '%s' is not set", env);
        return -1;
    }
    return 0;
}

static
int bake_action_default(bake_crawler c, bake_project* p, void *ctx) {
    corto_log_push("default");
    corto_trace("begin");

    bake_language *b = NULL;
    if (p->language) {
        b = bake_language_get(p->language);
        if (!b) {
            goto error;
        }        
    }

    /* Step 1: clean package hierarchy */
    if (bake_uninstall(p)) {
        goto error;
    }

    /* Step 2: pre-install files to package hierarchy */
    if (!skip_preinstall) {
        if (bake_pre(p)) {
            goto error;
        }
    }

    /* The next steps are only relevant if a language is configured */
    if (p->language) {

        /* Step 3: if managed, generate code */

        /* Step 4: build sources */

        /* Step 5: install artefact */
    }

    corto_ok("done");
    corto_log_pop();
    return 1; /* continue */
error:
    corto_log_pop();
    return 0; /* stop */
}

static
int bake_action_clean(bake_crawler c, bake_project* p, void *ctx) {
    return 1; /* continue */
error:
    return 0; /* stop */
}

static
int bake_action_install(bake_crawler c, bake_project* p, void *ctx) {
    corto_log_push("install");
    corto_trace("begin");

    /* Install package to package hierarchy */
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

int main(int argc, char* argv[]) {
    corto_log_fmt("%V %F:%L %c: %m");
    
    /* Initialize base library */
    base_init(argv[0]);

    /* Verify environment variables */
    if (bake_test_env("CORTO_HOME")) goto error;
    if (bake_test_env("CORTO_TARGET")) goto error;

    int last_parsed = parseArgs(argc - 1, &argv[1]);
    if (last_parsed == -1) {
        goto error;
    }

    /* Create crawler for finding corto projects */
    bake_crawler c = bake_crawler_new();

    if (id) {
        bake_project *p;

        /* If id is specified, project config is provided on cmd line */
        if (!(p = bake_crawler_addProject(c, path, NULL))) {
            goto error;
        }

        p->id = id;
        p->language = language;
        p->managed = managed;
        p->local = local;
        p->includes = includes;
        p->sources = sources;

        if (!stricmp(kind, "library")) {
            p->kind = BAKE_LIBRARY;
        } else if (!stricmp(kind, "application")) {
            p->kind = BAKE_APPLICATION;
        } else if (!strcmp(kind, "tool")) {
            p->kind = BAKE_TOOL;
        }

    } else {
        /* Crawl specified directory (default is current) */
        if (bake_crawler_search(c, path)) {
            goto error;
        }
    }

    if (!bake_crawler_count(c)) {
        corto_seterr("no projects found");
        goto error;
    }

    if (argc == last_parsed) {
        /* If after parsing arguments no actions were defined, execute the default
         * action. */
        if (!bake_crawler_walk(c, bake_action_default, NULL)) {
            goto error;
        }
    } else {
        bake_crawler_cb action;
        int i;
        for (i = last_parsed; i < argc; i ++) {
            if (!strcmp(argv[i], "default")) action = bake_action_default;
            else if (!strcmp(argv[i], "clean")) action = bake_action_clean;
            else if (!strcmp(argv[i], "install")) action = bake_action_install;
            else if (!strcmp(argv[i], "uninstall")) action = bake_action_uninstall;
            else {
                corto_error("unknown action '%s'", action);
                continue;
            }
            if (!bake_crawler_walk(c, action, NULL)) {
                goto error;
            }
        }
    }

    /* Cleanup resources */
    bake_crawler_free(c);

    return 0;
error:
    corto_error("%s", corto_lasterr());
    return -1;
}
