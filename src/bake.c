
#include <include/bake.h>

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

int bake_project_action(bake_crawler c, bake_project p, void *ctx) {
    printf("project found!\n");
}

int main(int argc, char* argv[]) {

    corto_log_verbositySet(CORTO_TRACE);

    /* Initialize base library */
    base_init(argv[0]);

    /* Create crawler for finding corto projects */
    bake_crawler c = bake_crawler_new();

    /* Crawl current directory */
    if (bake_crawler_search(c, ".")) {
        goto error;
    }

    /* Walk projects in correct dependency order */
    if (!bake_crawler_walk(c, bake_project_action, NULL)) {
        goto error;
    }

    return 0;
error:
    corto_error("%s", corto_lasterr());
    return -1;
}
