
#include <include/bake.h>
#include "project.h"

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
static char *language = "c";
static char *kind = "library";
static bool managed = false;
static bool local = false;

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
            PARSE_OPTION('l', "language", language = argv[i + 1]; i ++);
            PARSE_OPTION('k', "kind", kind = argv[i + 1] ; i ++);
            PARSE_OPTION(0, "managed", managed = true);
            PARSE_OPTION(0, "local", local = true);
            
            if (!parsed) {
                fprintf(stderr, "unknown option '%s' (use bake --help to see available options)\n", argv[i]);
                return -1;
            }
        } else {
            break;
        }
    }

    return i + 1;
}

int bake_project_action(bake_crawler c, bake_project* p, void *ctx) {
    corto_ok("found '%s' in '%s'", p->id, p->path);
    if (bake_preinstall(p)) {
        goto error;
    }

    return 1; /* continue */
error:
    return 0; /* stop */
}

int main(int argc, char* argv[]) {
    int last_parsed = parseArgs(argc - 1, &argv[1]);
    if (last_parsed == -1) {
        goto error;
    }

    /* Initialize base library */
    base_init(argv[0]);

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

        if (!stricmp(kind, "library")) {
            p->kind = BAKE_LIBRARY;
        } else if (!stricmp(kind, "application")) {
            p->kind = BAKE_APPLICATION;
        }

    } else {
        /* Crawl specified directory (default is current) */
        if (bake_crawler_search(c, path)) {
            goto error;
        }
    }

    /* Walk projects in correct dependency order */
    if (!bake_crawler_walk(c, bake_project_action, NULL)) {
        goto error;
    }

    /* Cleanup resources */
    bake_crawler_free(c);

    return 0;
error:
    corto_error("%s", corto_lasterr());
    return -1;
}
