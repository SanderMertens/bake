
#include "bake.h"

static corto_ll builders;

typedef int (*buildmain_cb)(bake_language *b);

bake_language* bake_language_get(
    const char *language)
{
    bool found = false;
    bake_language *b = NULL;
    char *package = corto_asprintf("driver/bake/%s", language);

    if (!builders) {
        builders = corto_ll_new();
    }

    /* Check if builder is already loaded */
    corto_iter it = corto_ll_iter(builders);
    while (corto_iter_hasNext(&it) && !b) {
        bake_language *e = corto_iter_next(&it);
        if (!strcmp(e->package, package)) {
            b = e;
        }
    }

    if (!b) {
        bake_language *b = corto_calloc(sizeof(bake_language));

        corto_dl dl = NULL;
        buildmain_cb _main = corto_load_sym(package, &dl, "bakemain");
        if (!_main) {
            corto_seterr("failed load '%s': %s", 
                package,
                corto_lasterr());
            goto error;
        }
        b->dl = dl;

        if (_main(b)) {
            corto_seterr("bakemain for '%s' failed: %s", 
                language, 
                corto_lasterr());
            free(b);
            goto error;
        }

        b->package = strdup(package);
        corto_ll_append(builders, b);
    }

    corto_ll_append(builders, b);

    if (package) free(package);
    return b;
error:
    if (package) free(package);
    return NULL;
}

