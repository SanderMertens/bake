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

#include <bake.h>

typedef enum bake_src_lang {
    BAKE_SRC_LANG_C = 0,
    BAKE_SRC_LANG_CPP = 1,
    BAKE_SRC_LANG_OBJ_C = 2
} bake_src_lang;

static
char* obj_ext() 
{
    return ".o";
}

/* Get compiler */
static
const char *cc(
    bake_src_lang lang)
{
    const char *cxx = ut_getenv("CXX");
    const char *cc = ut_getenv("CC");

    if (cc && !strlen(cc)) cc = NULL;
    if (cxx && !strlen(cxx)) cxx = NULL;

    if (!is_darwin()) {
        if (lang == BAKE_SRC_LANG_CPP) {
            if (!cxx)
                cxx = "g++";
            return cxx;
        } else {
            if (!cc)
                cc = "gcc";
            return cc;
        }
    } else {
        /* On MacOS, invoking gcc and g++ actually invokes clang, unless 
         * explicitly configured otherwise. It is safest to assume clang, as
         * some gcc options can cause the clang linker to fail (like -z). */
        if (lang == BAKE_SRC_LANG_CPP) {
            if (!cxx || !strcmp(cxx, "g++"))
                cxx = "clang++";
            return cxx;
        } else {
            if (!cc || !strcmp(cc, "gcc"))
                cc = "clang";
            return cc;
        }
    }
}

static
bool is_compiler(
    const char *name,
    bake_src_lang lang)
{
    const char *compiler = cc(lang);
    size_t name_len = strlen(name);

    /* First test if user is running clang from a non-standard path. */
    const char *cc_command = strrchr(compiler, UT_OS_PS[0]);
    if (!cc_command) {
        cc_command = name;
    } else {
        cc_command ++;
    }

    if (!strncmp(cc_command, compiler, name_len)) {
        return true;
    } else {
        return false;
    }
}

/* Is current compiler clang */
static
bool is_clang(bake_src_lang lang)
{
    return is_compiler("clang", lang);
}

/* Is current compiler emcc */
static 
bool is_emcc(void) {
    return is_compiler("emcc", 0);
}

/* Is current compiler icc */
static 
bool is_icc(void) {
    return is_compiler("icc", 0);
}

static
void add_includes(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    ut_strbuf *cmd)
{
    /* Add configured include paths */
    bake_attr *include_attr = driver->get_attr("include");
    if (include_attr) {
        ut_iter it = ut_ll_iter(include_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *include = ut_iter_next(&it);
            char* file = include->is.string;

            if (file[0] == '/' || file[0] == '$') {
                ut_strbuf_append(cmd, " -I%s", file);
            } else {
                ut_strbuf_append(cmd, " -I%s/%s", project->path, file);
            }
        }
    }

    /* Add project include path */
    ut_strbuf_append(cmd, " -I%s/include", project->path);

    /* Add bake environment to include path */
    ut_strbuf_append(cmd, " -I %s/include", config->home);
}   

static
void add_flags(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    bake_src_lang lang,
    ut_strbuf *cmd)
{
    if (lang == BAKE_SRC_LANG_C) {
        /* CFLAGS for c projects */
        bake_attr *flags_attr = driver->get_attr("cflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attr *el = ut_iter_next(&it);
                ut_strbuf_append(cmd, " %s", el->is.string);
            }
        }
    } else if (lang == BAKE_SRC_LANG_CPP) {
        /* CXXFLAGS for c++ projects */
        bake_attr *flags_attr = driver->get_attr("cxxflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attr *el = ut_iter_next(&it);
                ut_strbuf_append(cmd, " %s", el->is.string);
            }
        }
    }

    /* Add defines */
    bake_attr *def_attr = driver->get_attr("defines");
    if (def_attr) {
        ut_iter it = ut_ll_iter(def_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *el = ut_iter_next(&it);
            ut_strbuf_append(cmd, " -D%s", el->is.string);
        }
    }

    /* Add defines from config */
    if (config->defines) {
        ut_iter it = ut_ll_iter(config->defines);
        while (ut_iter_hasNext(&it)) {
            const char *el = ut_iter_next(&it);
            ut_strbuf_append(cmd, " -D%s", el);
        }
    }

    /* Enable debugging code */
    if (!config->debug) {
        ut_strbuf_appendstr(cmd, " -DNDEBUG");
    } else if (!is_emcc()) {
        ut_strbuf_appendstr(cmd, " -fstack-protector-all");
    }

    /* Give project access to its own id */
    ut_strbuf_append(cmd, " -DBAKE_PROJECT_ID=\"%s\"", project->id);

    /* This macro is only set for source files of this project, and can be used
     * to exclude header statements for dependencies */
    char *building_macro = ut_asprintf(" -D%s_EXPORTS", project->id_underscore);
    ut_strbuf_appendstr(cmd, building_macro);
    free(building_macro);
}

static
void add_std(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    bake_src_lang lang,
    ut_strbuf *cmd,
    bool own_src)
{
    /* Set standard for C or C++ */
    if (lang == BAKE_SRC_LANG_CPP) {
        ut_strbuf_append(cmd, " -std=%s",
            driver->get_attr_string("cpp-standard"));
    } else if (lang == BAKE_SRC_LANG_C) {
        ut_strbuf_append(cmd, " -std=%s ",
            driver->get_attr_string("c-standard"));
        ut_strbuf_append(cmd, " -D_XOPEN_SOURCE=600");
    }

    ut_strbuf_appendstr(cmd, " -Wall -W -Wextra -pedantic"); 

    /* If strict, enable lots of warnings & treat warnings as errors */
    if (config->strict) {
        /* Enable lots of warnings in strict mode */
        ut_strbuf_appendstr(cmd, " -Werror -Wshadow -Wconversion -Wwrite-strings");
        ut_strbuf_appendstr(cmd, " -Wunused-parameter -Wsign-compare");
        ut_strbuf_appendstr(cmd, " -Wparentheses -Wsequence-point -Wpointer-arith");
        ut_strbuf_appendstr(cmd, " -Wdisabled-optimization");
        ut_strbuf_appendstr(cmd, " -Wsign-conversion");
        ut_strbuf_appendstr(cmd, " -Wunknown-pragmas");
        ut_strbuf_appendstr(cmd, " -Wfloat-conversion");
        ut_strbuf_appendstr(cmd, " -Wuninitialized");
        ut_strbuf_appendstr(cmd, " -Wmissing-field-initializers");
        ut_strbuf_appendstr(cmd, " -Wundef");

        if (is_clang(lang)) {
            ut_strbuf_appendstr(cmd, " -Wdocumentation");
            ut_strbuf_appendstr(cmd, " -Wconditional-uninitialized");
            ut_strbuf_appendstr(cmd, " -Wunreachable-code");
            ut_strbuf_appendstr(cmd, " -Wfour-char-constants");
            ut_strbuf_appendstr(cmd, " -Wenum-conversion");
            ut_strbuf_appendstr(cmd, " -Wshorten-64-to-32");
        }

        ut_strbuf_appendstr(cmd, " -Wredundant-decls -Wdouble-promotion");
        ut_strbuf_appendstr(cmd, " -Wcast-align");

        /* These warnings are not valid for C++ */
        if (lang != BAKE_SRC_LANG_CPP) {
            ut_strbuf_appendstr(cmd, " -Wstrict-prototypes");

        /* These warnings are only valid for C++ */
        } else {
            ut_strbuf_appendstr(cmd, " -Wnon-virtual-dtor");
            ut_strbuf_appendstr(cmd, " -Woverloaded-virtual");
            if (!is_icc()) {
                ut_strbuf_appendstr(cmd, " -Wold-style-cast");
            }
        }
        
        /* If project is a package, it should not contain global functions
         * without a declaration. */
        if (project->type == BAKE_PACKAGE) {
            ut_strbuf_appendstr(cmd, " -Wmissing-declarations");
        }
    } else {
        ut_strbuf_appendstr(cmd, " -Wno-missing-field-initializers");
        ut_strbuf_appendstr(cmd, " -Wno-unused-parameter");
    }

    /* If project contains imported source files from other projects, warnings
     * for unused functions are probably not going to be helpful. */
    if (!own_src) {
        ut_strbuf_appendstr(cmd, " -Wno-unused-function");
    }
}

static
void add_optimization(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    bake_src_lang lang,
    ut_strbuf *cmd,
    bool is_pch)
{
    /* Enable full optimizations, including cross-file */
    if (config->optimizations) {
        if ((!is_clang(lang) || !is_linux()) && !config->symbols) {
            ut_strbuf_appendstr(cmd, " -O3");

            /* LTO can hide warnings */
            if (!config->strict && !is_pch) {
                ut_strbuf_appendstr(cmd, " -flto");
            }
        } else {
            /* On some Linux versions clang has trouble loading the LTO plugin */
            ut_strbuf_appendstr(cmd, " -O3");
        }
    } else {
        ut_strbuf_appendstr(cmd, " -O0");
    }
}

static
void add_sanitizers(
    bake_config *config,
    ut_strbuf *cmd)
{
    if (config->sanitize_memory) {
        ut_strbuf_appendstr(cmd, " -fsanitize=address");
    }

    if (config->sanitize_undefined) {
        ut_strbuf_appendstr(cmd, " -fsanitize=undefined");
    }

    if (config->sanitize_thread) {
        ut_strbuf_appendstr(cmd, " -fsanitize=thread");
    }    
}

static
void add_misc(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    bake_src_lang lang,
    ut_strbuf *cmd)
{
    ut_strbuf_append(cmd, 
        " -fPIC"
        " -fvisibility=hidden");

    /* Include debugging information */
    if (config->symbols) {
        if (!is_emcc()) {
            ut_strbuf_appendstr(cmd, " -g");
        } else {
            ut_strbuf_appendstr(cmd, " -gsource-map");
        }
    }

    if (config->coverage && project->coverage) {
        ut_strbuf_appendstr(cmd, " -fprofile-arcs -ftest-coverage");
    }

    if (config->loop_test) {
        if (is_clang(lang)) {
            ut_strbuf_append(cmd,
                " -Rpass-missed=loop-vectorize");
        } else {
            ut_strbuf_append(cmd,
                " -fopt-info-vec-missed");
        }
    }

    if (config->assembly) {
        ut_strbuf_append(cmd, " -S -fverbose-asm");
    }

    if (config->profile_build) {
        ut_strbuf_append(cmd, " -ftime-trace");
    }

    add_sanitizers(config, cmd);
}

static
void add_misc_link(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    bake_src_lang lang,
    ut_strbuf *cmd)
{
    if (is_emcc()) {
        ut_strbuf_append(cmd, " -s ALLOW_MEMORY_GROWTH=1");
        ut_strbuf_append(cmd, " -s EXPORTED_RUNTIME_METHODS=cwrap");
        ut_strbuf_append(cmd, " -s MODULARIZE=1");
        ut_strbuf_append(cmd, " -s EXPORT_NAME=\"%s\"", project->id_underscore);

        if (ut_file_test("etc/assets") == 1) {
            ut_strbuf_append(cmd, " --embed-file etc/assets");
        }

        if (config->debug) {
            if (is_emcc()) {
                ut_strbuf_appendstr(cmd, " -s ASSERTIONS=2");
                // ut_strbuf_appendstr(cmd, " -s LINKABLE=1");
            }
        }
    }

    add_sanitizers(config, cmd);
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
    bool file_has_different_language = false;
    bake_src_lang lang = BAKE_SRC_LANG_C;

    if (ext && strcmp(ext, ".c")) {
        if (!strcmp(ext, ".m")) {
            lang = BAKE_SRC_LANG_OBJ_C;
        } else {
            lang = BAKE_SRC_LANG_CPP;
        }

        file_has_different_language = true;
    }

    /* Test if the source file is from the project itself. If the project
     * imports (amalgamated) source files from other projects, we should not
     * add the precompiled header options for the current project header. */
    bool own_source = true;
    char *relative_src = &source[strlen(project->path)];
    if (strncmp(relative_src, "deps"UT_OS_PS, 5)) {
        own_source = false;
    }

    ut_strbuf_append(&cmd, "%s", cc(lang));

    /* Add misc options */
    add_misc(driver, config, project, lang, &cmd);

    /* Add optimization flags */
    add_optimization(driver, config, project, lang, &cmd, false);

    /* Add c/c++ standard arguments */
    add_std(driver, config, project, lang, &cmd, own_source);

    /* Add CFLAGS */
    add_flags(driver, config, project, lang, &cmd);

    /* Add precompiled header.
     * Only add the precompiled header if the source file is of the same
     * language as the project configuration. Header compiled with different
     * compiler settings cannot be included */
    if (!file_has_different_language && own_source && driver->get_attr("precompile-header")) {
        if (driver->get_attr_bool("precompile-header") && !config->assembly) {
            if (is_clang(lang)) {
                char *pch_dir = driver->get_attr_string("pch-dir");
                ut_strbuf_append(&cmd, " -include %s/%s/%s.h", 
                    project->path, pch_dir, project->id_base);
            } else {
                /* Disable PCH for gcc for now, as it seems to slow down builds */
                //char *tmp_dir = driver->get_attr_string("tmp-dir");
                //ut_strbuf_append(&cmd, " -I%s", tmp_dir);
            }
        }
    }

    /* Add include directories */
    add_includes(driver, config, project, &cmd);

    /* Add source file and object file */
    ut_strbuf_append(&cmd, " -c %s", source);

    if (!config->assembly) {
        ut_strbuf_append(&cmd, " -o %s", target);
    }

    /* Execute command */
    char *cmdstr = ut_strbuf_get(&cmd);
    driver->exec(cmdstr);
    free(cmdstr);
}

static
void generate_precompiled_header(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{   
    bool cpp = is_cpp(project);
    bake_src_lang lang = cpp ? BAKE_SRC_LANG_CPP : BAKE_SRC_LANG_C;

    if (config->assembly) {
        return;
    }

    if (!is_clang(lang)) {
        return;
    }

    ut_strbuf cmd = UT_STRBUF_INIT;

    char *pch_dir = ut_asprintf("%s/%s", 
        project->path, 
        driver->get_attr_string("pch-dir"));

    char *source = ut_asprintf("%s/include/%s.h",
        project->path,
        project->id_base);

    char *target = ut_asprintf("%s/%s.h.%s",
        pch_dir,
        project->id_base,
        driver->get_attr_string("ext-pch"));

    /* Add source file and object file */
    ut_strbuf_append(&cmd, 
        "%s -x %s-header %s -o %s", cc(cpp), cpp ? "c++" : "c", source, target);

    free(target);
    free(source);

    /* Add misc options */
    add_misc(driver, config, project, lang, &cmd);

    /* Add optimization flags */
    add_optimization(driver, config, project, lang, &cmd, true);

    /* Add -std option */
    add_std(driver, config, project, lang, &cmd, true);

    /* Add CFLAGS */
    add_flags(driver, config, project, lang, &cmd);

    /* Add include directories */
    add_includes(driver, config, project, &cmd);

    /* Make sure PCH directory exists */
    ut_mkdir(pch_dir);

    free(pch_dir);

    /* Execute command */
    char *cmdstr = ut_strbuf_get(&cmd);
    driver->exec(cmdstr);
    free(cmdstr);
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
    char *file = ut_asprintf("%s/lib%s.a", config->lib, lib);
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

/* Link a binary */
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

    if (config->assembly) {
        return;
    }

    bool cpp = is_cpp(project);
    bake_src_lang lang = cpp ? BAKE_SRC_LANG_CPP : BAKE_SRC_LANG_C;

    ut_strbuf_append(&cmd, "%s -Wall", cc(cpp));

    if (project->type == BAKE_PACKAGE) {
        /* Set symbol visibility */
        bool export_symbols = driver->get_attr_bool("export-symbols");
        if (!export_symbols) {
            ut_strbuf_appendstr(&cmd, " -fvisibility=hidden");
            hide_symbols = true;
        }

        ut_strbuf_appendstr(&cmd, " -fno-stack-protector -shared");

        /* Fail when symbols are not found in library */
        if (!is_clang(cpp) && !is_emcc()) {
            ut_strbuf_appendstr(&cmd, " -z defs");
        }
    }

    add_misc_link(
        driver,
        config,
        project,
        cpp,
        &cmd);

    /* Set optimizations */
    if (config->optimizations) {
        ut_strbuf_appendstr(&cmd, " -O3");
        if (!config->strict) {
            ut_strbuf_appendstr(&cmd, " -flto");
        }
    } else {
        ut_strbuf_appendstr(&cmd, " -O0");
    }

    if (config->coverage && project->coverage) {
        ut_strbuf_appendstr(&cmd, " -fprofile-arcs -ftest-coverage");
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

    /* Link static library */
    bake_attr *static_lib_attr = driver->get_attr("static-lib");

    if (static_lib_attr) {
        if (static_lib_attr->kind != BAKE_ARRAY) {
            ut_error("attribute 'static-lib' is not of type array");
            project->error = true;
            return;
        }

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

    /* Add BAKE_TARGET to library path */
    if (ut_file_test(config->lib)) {
        ut_strbuf_append(&cmd, " -L%s", config->lib);
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

    /* Add project libpath */
    bake_attr *libpath_attr = driver->get_attr("libpath");
    if (libpath_attr) {
        ut_iter it = ut_ll_iter(libpath_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *lib = ut_iter_next(&it);
            ut_strbuf_append(&cmd, " -L%s", lib->is.string);

            if (is_clang(cpp)) {
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

    if (is_dylib(driver, project)) {
        ut_strbuf_append(&cmd, "install_name_tool -id @rpath/%s %s", 
            project->artefact, target);
        cmdstr = ut_strbuf_get(&cmd);
        driver->exec(cmdstr);
        free(cmdstr);
    }

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

/* Specify files to clean */
static
void clean(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    char *header = ut_asprintf("include/%s.h.pch", project->id_base);
    driver->remove(header);
    free(header);
    
    header = ut_asprintf("include/%s.h.gch", project->id_base);
    driver->remove(header);
    free(header);
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
        if (is_emcc()) {
            result = ut_asprintf("lib%s.so", id);
        } else {
            bool link_static = driver->get_attr_bool("static");

            if (link_static) {
                result = ut_asprintf("lib%s.a", id);
            } else {
                if (is_dylib(driver, project)) {
                    result = ut_asprintf("lib%s.dylib", id);
                } else {
                    result = ut_asprintf("lib%s.so", id);
                }
            }
        }
    } else {
        if (is_emcc()) {
            result = ut_asprintf("%s.js", id);
        } else {
            result = ut_strdup(id);
        }
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

typedef struct file_coverage_t {
    char *file;
    int total_lines;
    int uncovered_lines;
} file_coverage_t;

static const char *no_code =     "        -:";
static const char *not_covered = "    #####:";

static
file_coverage_t parse_gcov(
    bake_project *project, 
    const char *file)
{
    char *path = ut_asprintf("%s/gcov/%s", project->path, file);
    FILE *f = fopen(path, "r");

    int total_lines = 0;
    int uncovered_lines = 0;

    char line[30];
    while (ut_file_readln(f, line, 11)) {
        if (!strcmp(line, no_code)) {
            continue;
        } else if (!strcmp(line, not_covered)) {
            uncovered_lines ++;
        }

        total_lines ++;
    }

    char *c_file = ut_strdup(file);
    char *gcov_ext = strrchr(c_file, '.');
    *gcov_ext = '\0';

    file_coverage_t result = {
        .file = c_file,
        .total_lines = total_lines,
        .uncovered_lines = uncovered_lines
    };

    free(path);

    return result;
}

static
int coverage_compare(
    const void *f1_ptr,
    const void *f2_ptr)
{
    const file_coverage_t *f1 = f1_ptr;
    const file_coverage_t *f2 = f2_ptr;

    return f2->uncovered_lines - f1->uncovered_lines;
}

static
void print_coverage(
    const char *file,
    int file_len_max,
    float coverage,
    int total_lines,
    int missed_lines)
{
    if (coverage < 60) {
        ut_log("%*s: #[red]%.1f%%#[normal] (loc: %d, miss: %d)\n", file_len_max, file, 
            coverage, total_lines, missed_lines);
    } else if (coverage < 80) {
        ut_log("%*s: #[yellow]%.1f%%#[normal] (loc: %d, miss: %d)\n", file_len_max, file, 
            coverage, total_lines, missed_lines);
    } else {
        ut_log("%*s: #[green]%.1f%%#[normal] (loc: %d, miss: %d)\n", file_len_max, file, 
            coverage, total_lines, missed_lines);
    }
}

static
void parse_coverage(
    bake_project *project,
    int total_files)
{
    file_coverage_t *data = malloc(total_files * sizeof(file_coverage_t));
    char *gcov_dir = ut_asprintf("%s/gcov", project->path);

    ut_iter it;
    ut_dir_iter(gcov_dir, "*.gcov", &it);
    int i = 0, file_len_max = 0;
    int total_lines = 0;
    int uncovered_lines = 0;
    float coverage = 0;

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        data[i] = parse_gcov(project, file);
        
        int file_len = strlen(data[i].file);
        if (file_len > file_len_max) {
            file_len_max = file_len;
        }

        total_lines += data[i].total_lines;
        uncovered_lines += data[i].uncovered_lines;

        i ++;
    }

    qsort(data, total_files, sizeof(file_coverage_t), coverage_compare);

    for (i = 0; i < total_files; i ++) {
        coverage = 100.0 * (1.0 - 
            (float)data[i].uncovered_lines / (float)data[i].total_lines);
        print_coverage(data[i].file, file_len_max, coverage, 
            data[i].total_lines, data[i].uncovered_lines);

        free(data[i].file);
    }

    coverage = 100.0 * (1.0 - (float)uncovered_lines / total_lines);
    print_coverage("total", file_len_max, coverage, total_lines, uncovered_lines);
    printf("\n");

    free(gcov_dir);
    free(data);
}

static
void coverage(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    int total_files = 0;

    char *tmp_dir = driver->get_attr_string("tmp-dir");

    ut_iter it;
    char *src_dir = ut_asprintf("%s/src", project->path);
    ut_dir_iter(src_dir, "//*.c,*.cpp", &it);
    free(src_dir);

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        ut_strbuf cmd = UT_STRBUF_INIT;
        ut_strbuf_append(&cmd, "gcov --object-file %s/obj/%s %s/obj/%s", tmp_dir, file, tmp_dir, file);
        char *cmdstr = ut_strbuf_get(&cmd);

        int8_t rc;
        int sig = ut_proc_cmd_stderr_only(cmdstr, &rc);
        if (sig || rc) {
            ut_error("failed to run gcov command '%s'", cmdstr);
            project->error = 1;
            return;
        }

        free(cmdstr);

        total_files ++;
    }

    if (!total_files) {
        ut_error("no source files to analyze for coverage report");
        project->error = true;
        return;
    }

    char *gcov_dir = ut_asprintf("%s/gcov", project->path);
    ut_rm(gcov_dir);
    
    if (ut_mkdir(gcov_dir)) {
        ut_error("failed to create gcov directory '%s'", gcov_dir);
        project->error = 1;
        free(gcov_dir);
        return;
    }
    free(gcov_dir);

    ut_dir_iter(project->path, "*.gcov", &it);
    int total_gcov_files = 0;

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *dst_file = ut_asprintf("gcov/%s", file);

        if (ut_rename(file, dst_file)) {
            ut_error(
                "failed to move file '%s'", 
                file);
        }

        free(dst_file);
        total_gcov_files ++;
    }

    if (!total_gcov_files) {
        ut_error("no gcov files");
        project->error = true;
        return;
    }

    if (total_gcov_files < total_files) {
        ut_error("found %d source files, but only %d gcov files",
            total_files, total_gcov_files);
        project->error = true;
        return;
    } else if (total_gcov_files > total_files) {
        ut_error("found %d gcov files, but only %d source files",
            total_files, total_gcov_files);
        project->error = true;
        return;
    }

    parse_coverage(project, total_files);
}

static
void clean_coverage(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    char *tmp_dir = driver->get_attr_string("tmp-dir");
    char *obj_dir = ut_asprintf("%s/%s/obj", project->path, tmp_dir);

    if (ut_file_test(obj_dir) == 1) {
        ut_iter it;
        if (ut_dir_iter(obj_dir, "/*.gcda", &it)) {
            ut_raise();
            project->error = 1;
            return;
        }

        while (ut_iter_hasNext(&it)) {
            char *file = ut_iter_next(&it);
            char *file_path = ut_asprintf("%s/%s", obj_dir, file);
            ut_rm(file_path);
            free(file_path);
        }
    }

    free(obj_dir);
}
