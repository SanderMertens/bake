
#include <bake>

#define OBJ_DIR ".bake_cache/obj"

/* -- Mappings -- */

/* Obtain object name from source file */
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

/* TODO - used for header dependencies */
char* src_to_dep(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *in)
{
    return NULL;
}

char* obj_to_dep(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *in)
{
    return NULL;
}


/* -- Actions -- */

/* TODO - used for header dependencies */
static
void generate_deps(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{
}

/* Is current OS Darwin */
static
bool is_darwin(void)
{
    if (strcmp(UT_OS_STRING, "darwin")) {
        return false;
    }
    return true;
}

/* Is language C++ */
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

/* Get compiler */
static
const char *cc(
    bool is_cpp)
{
    const char *cxx = ut_getenv("CXX");
    const char *cc = ut_getenv("CC");

    if (!is_darwin()) {
        if (is_cpp) {
            if (!cxx)
                cxx = "g++";
            return cxx;
        } else {
            if (!cc)
                cc = "gcc";
            return cc;
        }
    } else {
        if (is_cpp) {
            if (!cxx)
                cxx = "clang++";
            return cxx;
        } else {
            if (!cc)
                cc = "gcc";
            return cc;
        }
    }
}

/* Compile source file */
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

    /* In obscure cases with static libs, stack protector can cause trouble */
    ut_strbuf_append(&cmd, "%s -Wall -fPIC -fno-stack-protector", cc(cpp));

    /* Set standard for C or C++ */
    if (cpp) {
        ut_strbuf_append(&cmd, " -std=%s",
            driver->get_attr_string("cpp-standard"));
    } else {
        ut_strbuf_append(&cmd, " -std=%s -D_XOPEN_SOURCE=600",
            driver->get_attr_string("c-standard"));
    }

    /* Give project access to its own id */
    ut_strbuf_append(&cmd, " -DBAKE_PROJECT_ID=\"%s\"", project->id);

    /* This macro is only set for source files of this project, and can be used
     * to exclude header statements for dependencies */
    char *building_macro = ut_asprintf(" -D%s_IMPL", project->id_underscore);
    strupper(building_macro);
    ut_strbuf_appendstr(&cmd, building_macro);
    free(building_macro);

    /* Include symbols */
    if (config->symbols) {
        ut_strbuf_appendstr(&cmd, " -g");
    }

    /* Enable debugging code */
    if (!config->debug) {
        ut_strbuf_appendstr(&cmd, " -DNDEBUG");
    }

    /* Enable full optimizations, including cross-file */
    if (config->optimizations) {
        ut_strbuf_appendstr(&cmd, " -O3 -flto");
    } else {
        ut_strbuf_appendstr(&cmd, " -O0");
    }

    /* If strict, enable lots of warnings & treat warnings as errors */
    if (config->strict) {
        ut_strbuf_appendstr(&cmd, " -Werror -Wextra -pedantic");
    }

    if (!cpp) {
        /* CFLAGS for c projects */
        bake_attr *flags_attr = driver->get_attr("cflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attr *flag = ut_iter_next(&it);
                ut_strbuf_append(&cmd, " %s", flag->is.string);
            }
        }
    } else {
        /* CXXFLAGS for c4cpp projects */
        bake_attr *flags_attr = driver->get_attr("cxxflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attr *flag = ut_iter_next(&it);
                ut_strbuf_append(&cmd, " %s", flag->is.string);
            }
        }
    }

    /* Add configured include paths */
    bake_attr *include_attr = driver->get_attr("include");
    if (include_attr) {
        ut_iter it = ut_ll_iter(include_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *include = ut_iter_next(&it);
            char* file = include->is.string;
            ut_strbuf_append(&cmd, " -I%s", file);
        }
    }

    /* Add BAKE_TARGET to include path */
    ut_strbuf_append(&cmd, " -I %s/include", config->target);

    /* Add BAKE_HOME to include path if it's different from BAKE_TARGET */
    if (strcmp(config->target, config->home)) {
        ut_strbuf_append(&cmd, " -I %s/include", config->home);
    }

    /* Add project root to include path */
    ut_strbuf_append(&cmd, " -I%s", project->path);

    /* Add source file and object file */
    ut_strbuf_append(&cmd, " -c %s -o %s", source, target);

    /* Execute command */
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

/* A better mechanism is needed to abstract away from the difference between
 * Linux-ish systems. */
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

/* Find a static library from a logical name */
static
char* find_static_lib(
    bake_driver_api *driver,
    bake_project *project,
    bake_config *config,
    const char *lib)
{
    int ret;

    /* Find static library in environment libpath */
    char *file = ut_asprintf("%s/lib%s.a", config->target_lib, lib);
    if ((ret = ut_file_test(file)) == 1) {
        return file;
    } else if (ret != 0) {
        free(file);
        ut_error("could not access '%s'", file);
        return NULL;
    }

    free(file);

    /* If static library is not found in environment, try project libpath */
    bake_attr *libpath_attr = driver->get_attr("libpath");
    if (libpath_attr) {
        ut_iter it = ut_ll_iter(libpath_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *lib_attr = ut_iter_next(&it);
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

/* Is binary a dylib */
static
bool is_dylib(
    bake_driver_api *driver,
    bake_project *project)
{
    if (is_darwin() && project->type == BAKE_PACKAGE) {
        return true;
    } else {
        return false;
    }
}

/* Link a library (package) */
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
    bool export_symbols = driver->get_attr_bool("export-symbols");

    ut_strbuf_append(&cmd, "%s -Wall -fPIC -fno-stack-protector --shared", cc(cpp));

    /* Set symbol visibility */
    if (!export_symbols && !is_darwin()) {
        ut_strbuf_appendstr(&cmd, " -Wl,-fvisibility=hidden");
        hide_symbols = true;
    }

    /* Fail when symbols are not found in library */
    if (!is_darwin()) {
        ut_strbuf_appendstr(&cmd, " -Wl,-z,defs");
    }

    /* Set optimizations */
    if (config->optimizations) {
        ut_strbuf_appendstr(&cmd, " -O3");
    } else {
        ut_strbuf_appendstr(&cmd, " -O0");
    }

    /* When strict, warnings are errors */
    if (config->strict) {
        ut_strbuf_appendstr(&cmd, " -Werror -pedantic");
    }

    /* Create a dylib when on MacOS */
    if (is_dylib(driver, project)) {
        ut_strbuf_appendstr(&cmd, " -dynamiclib");
    }

    /* Compiler flags provided to linker step */
    bake_attr *flags_attr = driver->get_attr("ldflags");
    if (flags_attr) {
        ut_iter it = ut_ll_iter(flags_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *flag = ut_iter_next(&it);
            ut_strbuf_append(&cmd, " %s", flag->is.string);
        }
    }

    /* Add object files */
    ut_strbuf_append(&cmd, " %s", source);

    /* Add BAKE_TARGET to library path */
    if (ut_file_test(config->target_lib)) {
        ut_strbuf_append(&cmd, " -L%s", config->target_lib);
    }

    /* Add BAKE_HOME to library path if it's different */
    if (strcmp(config->target, config->home)) {
        ut_strbuf_append(&cmd, " -L%s/lib", config->home);
    }

    /* Add libraries in 'link' attribute */
    ut_iter it = ut_ll_iter(project->link);
    while (ut_iter_hasNext(&it)) {
        char *dep = ut_iter_next(&it);
        ut_strbuf_append(&cmd, " -l%s", dep);
    }

    /* Link static library */
    bake_attr *static_lib_attr = driver->get_attr("static-lib");
    if (static_lib_attr) {
        ut_iter it = ut_ll_iter(static_lib_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *lib = ut_iter_next(&it);

            if (!hide_symbols) {
                ut_strbuf_append(&cmd, " -l%s", lib->is.string);
            } else {
                /* If hiding symbols and linking with static library, unpack
                 * library objects to temp directory. If the library would be
                 * linked as-is, symbols would be exported, even though
                 * fvisibility is set to hidden */
                char *static_lib = find_static_lib(
                    driver, project, config, lib->is.string);
                if (!static_lib) {
                    continue;
                }

                /* Unpack objects in static lib to temporary directory */
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

                /* Add path with object files to static_object_paths. These will
                 * be cleaned up after the compile command */
                ut_ll_append(static_object_paths, obj_path);
            }
        }
    }

    /* Add project libpath */
    bake_attr *libpath_attr = driver->get_attr("libpath");
    if (libpath_attr) {
        ut_iter it = ut_ll_iter(libpath_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *lib = ut_iter_next(&it);
            ut_strbuf_append(&cmd, " -L%s", lib->is.string);

            if (is_darwin()) {
                ut_strbuf_append(
                    &cmd, " -Xlinker -rpath -Xlinker %s", lib->is.string);
            }
        }
    }

    /* Add project libraries */
    bake_attr *lib_attr = driver->get_attr("lib");
    if (lib_attr) {
        ut_iter it = ut_ll_iter(lib_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *lib = ut_iter_next(&it);
            const char *mapped = lib_map(lib->is.string);
            if (mapped) {
                ut_strbuf_append(&cmd, " -l%s", mapped);
            }
        }
    }

    /* Specify output */
    ut_strbuf_append(&cmd, " -o %s", target);

    /* Execute command */
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

    ut_ll_free(static_object_paths);
}

/* Link a static library */
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

/* Link a library */
static
void link_binary(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    char *source,
    char *target)
{
    bool link_static = driver->get_attr_bool("static");

    if (link_static) {
        link_static_binary(driver, config, project, source, target);
    } else {
        link_dynamic_binary(driver, config, project, source, target);
    }
}

/* Initialize project defaults */
static
void init(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    bool cpp = is_cpp(project);

    if (cpp) {
        if (!driver->get_attr("cpp-standard")) {
            driver->set_attr_string("cpp-standard", "c++0x");
        }
    } else {
        if (!driver->get_attr("c-standard")) {
            driver->set_attr_string("c-standard", "c99");
        }
    }
}

/* Specify files to clean */
static
void clean(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    driver->remove("include/bake_config.h");

    /* Keep this to remove file from previous version. Remove in next version */
    driver->remove("include/prebaked.h");
}

/* Initialize directory with new project */
static
void setup_project(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    /* Get short project id */
    const char *id = project->id;
    bake_project_type kind = project->type;
    const char *short_id = project->id_short;

    /* Create directories */
    ut_mkdir("src");
    ut_mkdir("include");

    /* Create main source file */
    char *source_filename = ut_asprintf("src/main.c");
    FILE *f = fopen(source_filename, "w");
    if (!f) {
        ut_error("failed to open '%s'", source_filename);
        project->error = true;
        return;
    }

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
    if (!f) {
        ut_error("failed to open '%s'", header_filename);
        project->error = true;
        return;
    }

    fprintf(f,
        "#ifndef %s_H\n"
        "#define %s_H\n\n"
        "/* This generated file contains includes for project dependencies */\n"
        "#include \"bake_config.h\"\n",
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

    fclose(f);
}

/* -- Misc functions -- */

/* Return name of project artefact */
static
char* artefact_name(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    char *result;
    char *id = project->id_underscore;

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

    return result;
}

/* Get filename for library */
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

/* Generate list of include files used in bake_config.h */
static
void add_dependency_includes(
    bake_config *config,
    FILE *f,
    ut_ll dependencies)
{
    uint32_t count = 0;
    ut_iter it = ut_ll_iter(dependencies);
    while (ut_iter_hasNext(&it)) {
        char *project_id = ut_iter_next(&it);

        bool include_found = false;
        char *file = ut_asprintf("%s/include/%s", config->home, project_id);
        if (ut_file_test(file) != 1) {
            free(file);
            file = ut_asprintf("%s/include/%s", config->target, project_id);
            if (ut_file_test(file) == 1) {
                include_found = true;
            }
        } else {
            include_found = true;
        }
        if (include_found) {
            fprintf(f, "#include <%s>\n", project_id);
            count ++;
        }

        free(file);
    }

    if (!count) {
        fprintf(f, "/* No dependencies */\n");
    }
}

/* Generate bake_config.h */
static
void generate(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    const char *short_id = project->id_short;

    /* Create upper-case id for defines in header file */
    char *id_upper = ut_strdup(project->id_underscore);
    strupper(id_upper);

    /* Ensure include directory exists */
    ut_mkdir("%s/include", project->path);

    /* Create main header file */
    char *header_filename = ut_asprintf(
        "%s/include/bake_config.h", project->path);
    FILE *f = fopen(header_filename, "w");
    if (!f) {
        ut_error("failed to open file '%s'", header_filename);
        project->error = true;
        return;
    }

    fprintf(f,
"/*\n"
"                                   )\n"
"                                  (.)\n"
"                                  .|.\n"
"                                  | |\n"
"                              _.--| |--._\n"
"                           .-';  ;`-'& ; `&.\n"
"                          \\   &  ;    &   &_/\n"
"                           |\"\"\"---...---\"\"\"|\n"
"                           \\ | | | | | | | /\n"
"                            `---.|.|.|.---'\n"
"\n"
" * This file is generated by bake.lang.c for your convenience. Headers of\n"
" * dependencies will automatically show up in this file. Include prebake.h\n"
" * in your main project file. Do not edit! */\n\n"
"#ifndef %s_BAKE_CONFIG_H\n"
"#define %s_BAKE_CONFIG_H\n\n",
        id_upper,
        id_upper);

    fprintf(f, "/* Headers of public dependencies */\n");
    add_dependency_includes(config, f, project->use);

    fprintf(f, "\n/* Headers of private dependencies */\n");
    fprintf(f, "#ifdef %s_IMPL\n", id_upper);
    add_dependency_includes(config, f, project->use_private);
    fprintf(f, "#endif\n");

    fprintf(f, "\n/* Convenience macro for exporting symbols */\n");
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
