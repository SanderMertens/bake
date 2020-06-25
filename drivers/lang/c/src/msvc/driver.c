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

static
char* obj_ext() 
{
    return ".obj";
}

/* Get compiler */
static
const char *cc(
    bool is_cpp)
{
    const char *cxx = ut_getenv("CXX");
    const char *cc = ut_getenv("CC");

    if (is_cpp) {
        if (!cxx)
            cxx = "cl.exe";
        return cxx;
    } else {
        if (!cc)
            cc = "cl.exe";
        return cc;
    }
}

/* Compile source file for Windows platform using MSVC*/
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

    ut_strbuf_append(&cmd, "%s", cc(cpp));

    /* Suppress Visual C++ banner */
    ut_strbuf_appendstr(&cmd, " /nologo");

    /* If building cpp, add /EHsc for unwind semantics */
    if (cpp) {
        ut_strbuf_append(&cmd, " /EHsc");
    }

    /* Give project access to its own id */
    ut_strbuf_append(&cmd, " /DBAKE_PROJECT_ID=\\\"%s\\\"", project->id);

    /* This macro is only set for source files of this project, and can be used
     * to exclude header statements for dependencies */
    char *building_macro = ut_asprintf(" /D%s_EXPORTS", project->id_underscore);
    ut_strbuf_appendstr(&cmd, building_macro);
    free(building_macro);

    /* Enable debugging code */
    if (!config->debug) {
        ut_strbuf_appendstr(&cmd, " /DNDEBUG");
        ut_strbuf_appendstr(&cmd, " /DEBUG:FULL");
    }

    /* Enable full optimizations, including cross-file */
    if (config->optimizations) {
        ut_strbuf_appendstr(&cmd, " /Ox /GL");
    } else {
        ut_strbuf_appendstr(&cmd, " /Od");
    }

    /* If strict, enable lots of warnings & treat warnings as errors */
    if (config->strict) {
        ut_strbuf_appendstr(&cmd, " /WX");
    }

    if (!cpp) {
        /* CFLAGS for c projects */
        bake_attr *flags_attr = driver->get_attr("cflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attr *attr = ut_iter_next(&it);
                ut_strbuf_append(&cmd, " %s", attr->is.string);
            }
        }
    } else {
        /* CXXFLAGS for c4cpp projects */
        bake_attr *flags_attr = driver->get_attr("cxxflags");
        if (flags_attr) {
            ut_iter it = ut_ll_iter(flags_attr->is.array);
            while (ut_iter_hasNext(&it)) {
                bake_attr *attr = ut_iter_next(&it);
                ut_strbuf_append(&cmd, " %s", attr->is.string);
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

            if (file[0] == '/' || file[0] == '$') {
                ut_strbuf_append(&cmd, " /I%s", file);
            } else {
                ut_strbuf_append(&cmd, " /I%s\\%s", project->path, file);
            }
        }
    }

    /* Add BAKE_HOME to include path if it's different from BAKE_TARGET */
    ut_strbuf_append(&cmd, " /I %s\\include", config->home);

    /* Add project include path */
    ut_strbuf_append(&cmd, " /I %s\\include", project->path);

    /* Add source file and object file */
    ut_strbuf_append(&cmd, " /c %s /Fo%s", source, target);

    /* Include symbols */
    if (config->symbols) {
        ut_strbuf_append(&cmd, " /Zi");
    }

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
    char *file = ut_asprintf("%s\\%s.lib", config->lib, lib);
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
            file = ut_asprintf("%s\\%s.lib", lib_attr->is.string, lib);

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

/* Link a binary for Windows platforms*/
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

    ut_strbuf_append(&cmd, "link");
    
    /* Suppress Visual C++ banner */
    ut_strbuf_appendstr(&cmd, " /NOLOGO");

    if (project->type == BAKE_PACKAGE) {
        /* Set symbol visibility */
        // On Windows this is control inside code, this flag will have no effect at this point
        if (!export_symbols) {
            hide_symbols = true;
        }

        ut_strbuf_appendstr(&cmd, " /DYNAMICBASE:NO /NXCOMPAT:NO /DLL");

    }

    /* Include symbols */
    if (config->symbols) {
        char *pdb_file = strdup(target);
        char *ext = strrchr(pdb_file, '.');
        strcpy(ext + 1, "pdb");
        ut_strbuf_append(&cmd, " /DEBUG /PDB:\"%s\"", pdb_file);
    }

    /* Enable full optimizations, including cross-file */
    if (config->optimizations) {
        ut_strbuf_appendstr(&cmd, " /LTCG");
    }

    /* When strict, warnings are errors */
    if (config->strict) {
        ut_strbuf_appendstr(&cmd, " /WX");
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
            ut_strbuf_append(&cmd, " %s.lib", lib->is.string);
        }
    }

    /* Add BAKE_TARGET to library path */
    if (ut_file_test(config->lib)) {
        ut_strbuf_append(&cmd, " /LIBPATH:%s", config->lib);
    }

    /* Add BAKE_HOME to library path if it's different */
    if (strcmp(config->target, config->home)) {
        ut_strbuf_append(&cmd, " /LIBPATH:%s\\lib", config->home);
    }

    /* Add libraries in 'link' attribute */
    ut_iter it = ut_ll_iter(project->link);
    while (ut_iter_hasNext(&it)) {
        char *dep = ut_iter_next(&it);
        ut_strbuf_append(&cmd, " %s.lib", dep);
    }

    /* Add project libpath */
    bake_attr *libpath_attr = driver->get_attr("libpath");
    if (libpath_attr) {
        ut_iter it = ut_ll_iter(libpath_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *lib = ut_iter_next(&it);
            ut_strbuf_append(&cmd, " /LIBPATH:%s", lib->is.string);
        }
    }

    /* Add project libraries */
    bake_attr *lib_attr = driver->get_attr("lib");
    if (lib_attr) {
        ut_iter it = ut_ll_iter(lib_attr->is.array);
        while (ut_iter_hasNext(&it)) {
            bake_attr *lib = ut_iter_next(&it);
            ut_strbuf_append(&cmd, " %s", lib);
        }
    }

    /* Specify output */
    ut_strbuf_append(&cmd, " /OUT:%s", target);

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

/* Specify files to clean */
static
void clean(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
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
        bool link_static = driver->get_attr_bool("static");

        if (link_static) {
            result = ut_asprintf("%s"UT_OS_STATIC_LIB_EXT, id);
        } else {
            result = ut_asprintf("%s"UT_OS_LIB_EXT, id);
        }
    } else {
        result = ut_asprintf("%s"UT_OS_BIN_EXT, id);
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
    if (ext && !strchr(name, UT_OS_PS[0])) {
        /* Hardcoded filename was provided but not found */
        return NULL;
    }

    /* Try finding a library called lib*.so or lib*.dylib (OSX only) or *.lib (Windwos only) */
    char *full_path = strdup(name);
    char *lib_name = strrchr(full_path, UT_OS_PS[0]);
    if (lib_name) {
        lib_name[0] = '\0';
        lib_name ++;
    } else {
        lib_name = full_path;
        full_path = NULL;
    }

    /* Try .so or .lib or .dylib */
    if (full_path) {
        // fullpath[\|/][lib]lib_name[.so|.dylib|.lib]
        char *so = ut_asprintf("%s"UT_OS_PS UT_OS_LIB_PREFIX "%s" UT_OS_LIB_EXT, full_path, lib_name);
        if (ut_file_test(so)) {
            result = so;
        }
    } else {
        // [lib]lib_name[.so|.dylib|.lib]
        char *so = ut_asprintf(UT_OS_LIB_PREFIX "%s" UT_OS_LIB_EXT, lib_name);
        if (ut_file_test(so)) {
            result = so;
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
void generate_precompiled_header(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    /* TODO: implement PCHs for msvc */
}

/* Is current compiler clang */
static
bool is_clang(bool is_cpp)
{
    return false;
}

static
void coverage(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    /* TODO */
}

static
void clean_coverage(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    /* TODO */
}