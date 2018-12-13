
#include <bake/bake.h>

#define OBJ_DIR ".bake_cache/obj"

static
char* get_short_name(
    const char *package)
{
    char *result = strrchr(package, '/');
    if (!result) {
        result = (char*)package;
    } else {
        result ++;
    }

    char *ptr = strrchr(package, '.');
    if (ptr) {
        result = ptr + 1;
    }

    return result;
}

/* -- Mappings */
static
char* src_to_obj(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *in)
{
    const char *cfg = config->configuration;
    char *result = malloc(strlen(in) + strlen(OBJ_DIR) + strlen(UT_PLATFORM_STRING) + strlen(cfg) + 4);
    sprintf(result, OBJ_DIR "/%s-%s/%s", UT_PLATFORM_STRING, cfg, in);
    char *ext = strrchr(result, '.');
    strcpy(ext + 1, "o");
    return result;
}

char* src_to_dep(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *in)
{
    printf("src to dep (%s)\n", in);
    return NULL;
}

char* obj_to_dep(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *in)
{
    printf("obj to dep (%s)\n", in);
    return NULL;
}


/* -- Actions */

static
void generate_deps(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{

}

static
bool is_darwin(void)
{
    if (strcmp(UT_OS_STRING, "darwin")) {
        return false;
    }
    return true;
}

static
bool is_cpp(
    bake_project *project)
{
    if (!strcmp(project->language, "cpp")) {
        return true;
    } else {
        return false;
    }
}

static
const char *cc(
    bool is_cpp)
{
    if (!is_darwin()) {
        if (is_cpp) {
            return "g++";
        } else {
            return "gcc";
        }
    } else {
        if (is_cpp) {
            return "clang++";
        } else {
            return "clang";
        }
    }
}

static
void compile_src(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{
    ut_strbuf cmd = UT_STRBUF_INIT;
    char *ext = strrchr(source, '.');
    bool cpp = is_cpp(project);

    if (ext && strcmp(ext, ".c")) {
        /* If extension is not c, treat as a C++ file */
        cpp = true;
    }

    ut_strbuf_append(&cmd, "%s -Wall -fPIC -fno-stack-protector", cc(cpp));

    if (cpp) {
        ut_strbuf_appendstr(&cmd, " -std=c++0x -Wno-write-strings");
    } else {
        ut_strbuf_appendstr(&cmd, " -std=c99 -D_XOPEN_SOURCE=600");
    }

    ut_strbuf_append(&cmd, " -DPACKAGE_ID=\"%s\"", project->id);

    char *building_macro = ut_asprintf(" -D%s_IMPL", project->id);
    strupper(building_macro);
    char *ptr, ch;
    for (ptr = building_macro; (ch = *ptr); ptr++) {
        if (ch == '/') {
            *ptr = '_';
        }
    }
    ut_strbuf_appendstr(&cmd, building_macro);
    free(building_macro);

    if (config->symbols) {
        ut_strbuf_appendstr(&cmd, " -g");
    }
    if (!config->debug) {
        ut_strbuf_appendstr(&cmd, " -DNDEBUG");
    }
    if (config->optimizations) {
        ut_strbuf_appendstr(&cmd, " -O3 -flto");
    } else {
        ut_strbuf_appendstr(&cmd, " -O0");
    }
    if (config->strict) {
        ut_strbuf_appendstr(&cmd, " -Werror -Wextra -pedantic");
    }

    if (!cpp) {
        /* CFLAGS for c projects */
        bake_attribute *flags_attr = driver->get_attr("cflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attribute *flag = ut_iter_next(&it);
                ut_strbuf_append(&cmd, " %s", flag->is.string);
            }
        }
    } else {
        /* CXXFLAGS for c4cpp projects */
        bake_attribute *flags_attr = driver->get_attr("cxxflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attribute *flag = ut_iter_next(&it);
                ut_strbuf_append(&cmd, " %s", flag->is.string);
            }
        }
    }

    bake_attribute *include_attr = driver->get_attr("include");
    if (include_attr) {
        ut_iter it = ut_ll_iter(include_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attribute *include = ut_iter_next(&it);
            char* file = include->is.string;
            ut_strbuf_append(&cmd, " -I%s", file);
        }
    }

    ut_strbuf_append(&cmd, " -I %s/include", config->target);

    if (strcmp(ut_getenv("BAKE_TARGET"), ut_getenv("BAKE_HOME"))) {
        ut_strbuf_append(&cmd, " -I %s/include", config->home);
    }

    ut_strbuf_append(&cmd, " -I%s -c %s -o %s", project->path, source, target);

    char *cmdstr = ut_strbuf_get(&cmd);
    driver->exec(cmdstr);
    free(cmdstr);
}

static
void obj_deps(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{

}

static
const char* lib_map(
    const char *lib)
{
    /* On darwin, librt does not exist */
    if (!strcmp(lib, "rt")) {
        if (is_darwin()) {
            return NULL;
        }
    }

    return lib;
}

static
char* find_static_lib(
    bake_driver_api *driver,
    bake_project *project,
    bake_config *config,
    const char *lib)
{
    int ret;

    /* Find static library in configuration libpath */
    char *file = ut_asprintf("%s/lib%s.a", config->target_lib, lib);
    if ((ret = ut_file_test(file)) == 1) {
        return file;
    } else if (ret != 0) {
        free(file);
        ut_error("could not access '%s'", file);
        return NULL;
    }

    free(file);

    /* If static library is not found in configuration, try libpath */
    bake_attribute *libpath_attr = driver->get_attr("libpath");
    if (libpath_attr) {
        ut_iter it = ut_ll_iter(libpath_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attribute *lib_attr = ut_iter_next(&it);
            file = ut_asprintf("%s/lib%s.a", lib_attr->is.string, lib);

            if ((ret = ut_file_test(file)) == 1) {
                return file;
            } else if (ret != 0) {
                free(file);
                ut_error("could not access '%s'", file);
                return NULL;
            }

            free(file);
        }
    }

    return NULL;
}

static
bool is_dylib(
    bake_driver_api *driver,
    bake_project *project)
{
    if (is_darwin()) {
        bool dylib = false;
        bake_attribute *dylib_attr = driver->get_attr("dylib");
        if (dylib_attr) {
            dylib = dylib_attr->is.boolean;
        }
        return dylib;
    } else {
        return false;
    }
}

static
char *project_name(
    const char *project_id)
{
    char *result = NULL;
    char *id = ut_strdup(project_id);
    char *ptr, ch;
    for (ptr = id; (ch = *ptr); ptr++) {
        if (ch == '/') {
            *ptr = '_';
        }
    }

    return id;
}

static
void link_dynamic_binary(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{
    ut_strbuf cmd = UT_STRBUF_INIT;
    bool hide_symbols = false;
    ut_ll static_object_paths = NULL;

    bool cpp = is_cpp(project);
    bool export_symbols = driver->get_attr_bool("export_symbols");

    ut_strbuf_append(&cmd, "%s -Wall -fPIC", cc(cpp));

    if (project->type == BAKE_PACKAGE) {
        if (!export_symbols && !is_darwin()) {
            ut_strbuf_appendstr(&cmd, " -Wl,-fvisibility=hidden");
            hide_symbols = true;
        }
        ut_strbuf_appendstr(&cmd, " -fno-stack-protector --shared");
        if (!is_darwin()) {
            ut_strbuf_appendstr(&cmd, " -Wl,-z,defs");
        }
    }

    if (config->optimizations) {
        ut_strbuf_appendstr(&cmd, " -O3");
    } else {
        ut_strbuf_appendstr(&cmd, " -O0");
    }

    if (config->strict) {
        ut_strbuf_appendstr(&cmd, " -Werror -pedantic");
    }

    if (is_dylib(driver, project)) {
        ut_strbuf_appendstr(&cmd, " -dynamiclib");
    }

    /* LDFLAGS */
    bake_attribute *flags_attr = driver->get_attr("ldflags");
    if (flags_attr) {
        ut_iter it = ut_ll_iter(flags_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attribute *flag = ut_iter_next(&it);
            ut_strbuf_append(&cmd, " %s", flag->is.string);
        }
    }

    ut_strbuf_append(&cmd, " %s", source);

    if (ut_file_test(config->target_lib)) {
        ut_strbuf_append(&cmd, " -L%s", config->target_lib);
    }

    if (strcmp(ut_getenv("BAKE_TARGET"), ut_getenv("BAKE_HOME"))) {
        ut_strbuf_append(&cmd, " -L%s/lib", config->home);
    }

    ut_iter it = ut_ll_iter(project->link);
    while (ut_iter_hasNext(&it)) {
        char *dep = ut_iter_next(&it);
        ut_strbuf_append(&cmd, " -l%s", dep);
    }

    bake_attribute *static_lib_attr = driver->get_attr("static_lib");
    if (static_lib_attr) {
        ut_iter it = ut_ll_iter(static_lib_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attribute *lib = ut_iter_next(&it);
            if (hide_symbols) {
                /* If hiding symbols and linking with static library, unpack
                 * library objects to temp directory. If the library would be
                 * linked as-is, symbols would be exported, even though
                 * fvisibility is set to hidden */
                char *static_lib = find_static_lib(
                    driver, project, config, lib->is.string);
                if (!static_lib) {
                    continue;
                }

                char *cwd = strdup(ut_cwd());
                char *obj_path = ut_asprintf(".bake_cache/obj_%s/%s-%s",
                    lib->is.string, UT_PLATFORM_STRING, config->configuration);
                char *unpack_cmd = ut_asprintf("ar x %s", static_lib);

                /* The ar command doesn't have an option to output files to a
                 * specific directory, so have to use chdir. This will be an
                 * issue for multithreaded builds. */
                ut_mkdir(obj_path);
                ut_chdir(obj_path);
                driver->exec(unpack_cmd);
                free(unpack_cmd);
                free(static_lib);
                ut_chdir(cwd);
                free(cwd);
                ut_strbuf_append(&cmd, " %s/*", obj_path);

                if (!static_object_paths) {
                    static_object_paths = ut_ll_new();
                }

                ut_ll_append(static_object_paths, obj_path);
            } else {
                ut_strbuf_append(&cmd, " -l%s", lib);
            }
        }
    }

    bake_attribute *libpath_attr = driver->get_attr("libpath");
    if (libpath_attr) {
        ut_iter it = ut_ll_iter(libpath_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attribute *lib = ut_iter_next(&it);
            ut_strbuf_append(&cmd, " -L%s", lib->is.string);

            if (is_darwin()) {
                ut_strbuf_append(
                    &cmd, " -Xlinker -rpath -Xlinker %s", lib->is.string);
            }
        }
    }

    bake_attribute *lib_attr = driver->get_attr("lib");
    if (lib_attr) {
        ut_iter it = ut_ll_iter(lib_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attribute *lib = ut_iter_next(&it);
            const char *mapped = lib_map(lib->is.string);
            if (mapped) {
                ut_strbuf_append(&cmd, " -l%s", mapped);
            }
        }
    }

    ut_strbuf_append(&cmd, " -o %s", target);

    char *cmdstr = ut_strbuf_get(&cmd);
    driver->exec(cmdstr);
    free(cmdstr);

    /* If static libraries were unpacked, cleanup temporary directories */
    it = ut_ll_iter(static_object_paths);
    while (ut_iter_hasNext(&it)) {
        char *obj_path = ut_iter_next(&it);
        ut_rm(obj_path);
        free(obj_path);
    }

    if (static_object_paths) {
        ut_ll_free(static_object_paths);
    }
}

static
void link_static_binary(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{
    ut_strbuf cmd = UT_STRBUF_INIT;
    ut_strbuf_append(&cmd, "ar rcs %s %s", target, source);
    char *cmdstr = ut_strbuf_get(&cmd);
    driver->exec(cmdstr);
    free(cmdstr);
}

static
void link_binary(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{
    bool link_static = driver->get_attr_bool("static_artefact");

    if (link_static) {
        link_static_binary(driver, config, project, source, target);
    } else {
        link_dynamic_binary(driver, config, project, source, target);
    }
}

static
void init(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{

}

static
void clean(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    driver->remove("include/dependencies.h");
}

static
void setup_project(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    /* Get short project id */
    const char *id = project->id;
    bake_project_type kind = project->type;
    const char *short_id = get_short_name(id);

    /* Create directories */
    ut_mkdir("src");
    ut_mkdir("include");

    /* Create project.json */
    FILE *f = fopen("project.json", "w");
    fprintf(f,
        "{\n"
        "    \"id\":\"%s\",\n"
        "    \"type\":\"%s\"\n"
        "}\n",
        id,
        kind == BAKE_APPLICATION
            ? "application"
            : "package"
    );
    fclose(f);

    /* Create main source file */
    char *source_filename = ut_asprintf("src/main.c");
    f = fopen(source_filename, "w");

    fprintf(f,
        "#include <include/%s.h>\n"
        "\n"
        "int main(int argc, char *argv[]) {\n"
        "    return 0;\n"
        "}\n",
        short_id
    );

    fclose(f);
    free(source_filename);

    /* Create upper-case id for defines in header file */
    char *id_upper = strdup(id);
    strupper(id_upper);
    char *ptr, ch;
    for (ptr = id_upper; (ch = *ptr); ptr ++) {
        if (ch == '/' || ch == '.') {
            ptr[0] = '_';
        }
    }

    /* Create main header file */
    char *header_filename = ut_asprintf("include/%s.h", short_id);
    f = fopen(header_filename, "w");

    fprintf(f,
        "#ifndef %s_H\n"
        "#define %s_H\n\n"
        "/* This generated file contains includes for project dependencies */\n"
        "#include \"dependencies.h\"\n",
        id_upper,
        id_upper);

    if (kind != BAKE_PACKAGE) {
        fprintf(f, "%s",
            "\n"
            "#ifdef __cplusplus\n"
            "extern \"C\" {\n"
            "#endif\n"
            "\n"
            "#ifdef __cplusplus\n"
            "}\n"
            "#endif\n");
    }

    fprintf(f, "%s",
        "\n"
        "#endif\n"
        "\n");
}

/* -- Infrastructure functions -- */

static
char* artefact_name(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    char *result;
    char *id = project_name(project->id);

    if (project->type == BAKE_PACKAGE) {
        bool link_static = driver->get_attr_bool("static_artefact");

        if (link_static) {
            result = ut_asprintf("lib%s.a", id);
        } else {
            if (is_dylib(driver, project)) {
                result = ut_asprintf("lib%s.dylib", id);
            } else {
                result = ut_asprintf("lib%s.so", id);
            }
        }
    } else {
        result = ut_strdup(id);
    }

    free(id);

    return result;
}

static
char *link_to_lib(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *name)
{
    char *result = NULL;

    /* If link points to hardcoded filename, return as is */
    if (ut_file_test(name)) {
        return ut_strdup(name);
    }

    /* If not found, check if provided name has an extension */
    char *ext = strrchr(name, '.');
    if (ext && !strchr(name, '/')) {
        /* Hardcoded filename was provided but not found */
        return NULL;
    }

    /* Try finding a library called lib*.so or lib*.dylib (OSX only) */
    char *full_path = strdup(name);
    char *lib_name = strrchr(full_path, '/');
    if (lib_name) {
        lib_name[0] = '\0';
        lib_name ++;
    } else {
        lib_name = full_path;
        full_path = NULL;
    }

    /* Try .so */
    if (full_path) {
        char *so = ut_asprintf("%s/lib%s.so", full_path, lib_name);
        if (ut_file_test(so)) {
            result = so;
        }
    } else {
        char *so = ut_asprintf("lib%s.so", lib_name);
        if (ut_file_test(so)) {
            result = so;
        }
    }

    /* Try .dylib */
    if (!result && !strcmp(UT_OS_STRING, "darwin")) {
        if (full_path) {
            char *dylib = ut_asprintf("%s/lib%s.dylib", full_path, lib_name);
            if (ut_file_test(dylib)) {
                result = dylib;
            }
        } else {
            char *dylib = ut_asprintf("lib%s.dylib", lib_name);
            if (ut_file_test(dylib)) {
                result = dylib;
            }
        }
    }

    /* Try .a */
    if (!result) {
        if (full_path) {
            char *a = ut_asprintf("%s/lib%s.a", full_path, lib_name);
            if (ut_file_test(a)) {
                result = a;
            }
        } else {
            char *a = ut_asprintf("lib%s.a", lib_name);
            if (ut_file_test(a)) {
                result = a;
            }
        }
    }

    if (full_path) {
        free(full_path);
    } else if (lib_name) {
        free(lib_name);
    }

    return result;
}

static
void add_dependency_includes(
    bake_config *config,
    FILE *f,
    ut_ll dependencies)
{
    ut_iter it = ut_ll_iter(dependencies);
    while (ut_iter_hasNext(&it)) {
        char *project_id = ut_iter_next(&it);
        char *short_project_id = get_short_name(project_id);
        fprintf(f, "#include <%s/%s.h>\n", project_id, short_project_id);
    }
}

static
void generate(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    const char *id = project->id;
    const char *short_id = get_short_name(id);

    /* Create upper-case id for defines in header file */
    char *id_upper = strdup(id);
    strupper(id_upper);
    char *ptr, ch;
    for (ptr = id_upper; (ch = *ptr); ptr ++) {
        if (ch == '/' || ch == '.') {
            ptr[0] = '_';
        }
    }

    /* Create main header file */
    char *header_filename = ut_asprintf(
        "%s/include/dependencies.h", project->path);
    FILE *f = fopen(header_filename, "w");
    if (!f) {
        ut_throw("failed to open file '%s'", header_filename);
        project->error = true;
        return;
    }

    fprintf(f,
        "#ifndef %s_DEPENDENCIES_H\n"
        "#define %s_DEPENDENCIES_H\n"
        "/* This file is generated. Do not modify. */\n\n",
        id_upper,
        id_upper);

    fprintf(f, "/* Public dependencies */\n");
    add_dependency_includes(config, f, project->use);

    fprintf(f, "\n/* Private dependencies */\n");
    fprintf(f, "#ifdef %s_IMPL\n", id_upper);
    add_dependency_includes(config, f, project->use_private);
    fprintf(f, "#endif\n");

    fprintf(f, "\n/* Macro for exporting symbols */\n");
    fprintf(f,
      "#if %s_IMPL && defined _MSC_VER\n"
      "#define %s_EXPORT __declspec(dllexport)\n"
      "#elif %s_IMPL\n"
      "#define %s_EXPORT __attribute__((__visibility__(\"default\")))\n"
      "#elif defined _MSC_VER\n"
      "#define %s_EXPORT __declspec(dllimport)\n"
      "#else\n"
      "#define %s_EXPORT\n"
      "#endif\n", id_upper, id_upper, id_upper, id_upper, id_upper, id_upper);

    fprintf(f, "%s", "\n#endif\n\n");
    fclose(f);
}

/* -- Rules */
int bakemain(bake_driver_api *driver) {

    ut_init("driver/bake/c");

    /* Create pattern that matches source files */
    driver->pattern("SOURCES", "//*.c|*.cpp|*.cxx");

    /* Create rule for dynamically generating dep files from source files */
    driver->rule("deps", "$SOURCES", driver->target_map(src_to_dep), generate_deps);

    /* Create rule for dynamically generating object files from source files */
    driver->rule("objects", "$SOURCES", driver->target_map(src_to_obj), compile_src);

    /* Create rule for dynamically generating dependencies for every object in
     * $objects, using the generated dependency files. */
    driver->dependency_rule("$objects", "$deps", driver->target_map(obj_to_dep), obj_deps);

    /* Create rule for creating binary from objects */
    driver->rule("ARTEFACT", "$objects", driver->target_pattern(NULL), link_binary);

    /* Generate header file that automatically includes project dependencies */
    driver->generate(generate);

    /* Callback that initializes projects with the right build dependencies */
    driver->init(init);

    /* Callback that specifies files to clean */
    driver->clean(clean);

    /* Callback for generating artefact name(s) */
    driver->artefact(artefact_name);

    /* Callback for looking up library from link */
    driver->link_to_lib(link_to_lib);

    /* Callback for setting up a project */
    driver->setup(setup_project);

    return 0;
}
