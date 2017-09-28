
#include "bake.h"

static corto_ll languages;

extern corto_tls BAKE_LANGUAGE_KEY;

typedef int (*buildmain_cb)(bake_language *l);

static 
void bake_language_pattern_cb(
    const char *name, 
    const char *pattern) 
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    bake_language_pattern(l, name, pattern);
}

static 
void bake_language_rule_cb(
    const char *name, 
    const char *source, 
    bake_rule_target target, 
    bake_rule_action_cb action) 
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    bake_language_rule(l, name, source, target, action);
}

static 
void bake_language_dependency_rule_cb(
    const char *name, 
    const char *deps, 
    bake_rule_target dep_mapping, 
    bake_rule_action_cb action) 
{
    bake_language *l = corto_tls_get(BAKE_LANGUAGE_KEY);
    bake_language_dependency_rule(l, name, deps, dep_mapping, action);
}

static 
bake_rule_target bake_language_target_1_cb(
    const char *target) 
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_ONE;
    result.is.one = target;
    return result;
}

static 
bake_rule_target bake_language_target_pattern_cb(
    const char *pattern) 
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_PATTERN;
    result.is.pattern = pattern;
    return result;
}

static 
bake_rule_target bake_language_target_n_cb(
    bake_rule_map_cb mapping) 
{
    bake_rule_target result;
    result.kind = BAKE_RULE_TARGET_N;
    result.is.n = mapping;
    return result;
}

void bake_language_pattern(
    bake_language *l,     
    const char *name, 
    const char *pattern) {

}

void bake_language_rule(
    bake_language *l,     
    const char *name, 
    const char *source, 
    bake_rule_target target, 
    bake_rule_action_cb action) 
{

}

void bake_language_dependency_rule(
    bake_language *l,     
    const char *name, 
    const char *deps, 
    bake_rule_target dep_mapping, 
    bake_rule_action_cb action) 
{

}

bake_language* bake_language_get(
    const char *language)
{
    bool found = false;
    bake_language *l = NULL;
    char *package = corto_asprintf("driver/bake/%s", language);

    if (!languages) {
        languages = corto_ll_new();
    }

    /* Check if language is already loaded */
    corto_iter it = corto_ll_iter(languages);
    while (corto_iter_hasNext(&it) && !l) {
        bake_language *e = corto_iter_next(&it);
        if (!strcmp(e->package, package)) {
            l = e;
        }
    }

    if (!l) {
        l = corto_calloc(sizeof(bake_language));

        /* Set callbacks for populating rules */
        l->pattern = bake_language_pattern_cb;
        l->rule = bake_language_rule_cb;
        l->dependency_rule = bake_language_dependency_rule_cb;
        l->target_1 = bake_language_target_1_cb;
        l->target_pattern = bake_language_target_pattern_cb;
        l->target_n = bake_language_target_n_cb;

        corto_dl dl = NULL;
        buildmain_cb _main = corto_load_sym(package, &dl, "bakemain");

        if (!_main) {
            corto_seterr("failed load '%s': %s", 
                package,
                corto_lasterr());
            goto error;
        }
        l->dl = dl;

        if (_main(l)) {
            corto_seterr("bakemain for '%s' failed: %s", 
                language, 
                corto_lasterr());
            free(l);
            goto error;
        }

        l->package = strdup(package);
        corto_ll_append(languages, l);
    }

    corto_ll_append(languages, l);

    if (package) free(package);
    return l;
error:
    if (package) free(package);
    return NULL;
}

