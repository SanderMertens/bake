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

#define CACHE_DIR ".bake_cache"

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

/* Is current OS Darwin */
static
bool is_darwin(void)
{
    if (stricmp(UT_OS_STRING, "Darwin")) {
        return false;
    }
    return true;
}

static
bool is_linux(void)
{
    if (stricmp(UT_OS_STRING, "Linux")) {
        return false;
    }
    return true;
}

/* Include compiler-specific implementation. Eventually this should be made
 * pluggable instead of being determined by the host platform. */
#ifdef _WIN32
#include "msvc/driver.c"
#else
#include "gcc/driver.c"
#endif

/* Obtain object name from source file */
static
char* src_to_obj(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *in)
{
    char *obj_dir = driver->get_attr_string("obj-dir");
    /* Add some dummy characters (__) to make room for the extension */
    char *result = ut_asprintf("%s"UT_OS_PS"%s__", obj_dir, in);
    char *ext = strrchr(result, '.');
    strcpy(ext, obj_ext());
    return result;
}

/* Initialize project defaults */
static
void init(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    /* Let code detect whether it is being built by bake or not */
    driver->set_attr_array("cflags", "-D__BAKE__");
    driver->set_attr_array("cxxflags", "-D__BAKE__");

    if (!driver->get_attr("cpp-standard")) {
        driver->set_attr_string("cpp-standard", "c++0x");
    }
    if (!driver->get_attr("c-standard")) {
        driver->set_attr_string("c-standard", "c99");
    }
    if (!driver->get_attr("precompile-header")) {
#ifndef _WIN32
        char *main_header = ut_asprintf("%s/include/%s.h", 
            project->path, project->id_base);

        if (ut_file_test(main_header) == 1) {
            driver->set_attr_bool("precompile-header", true);
        } else {
            driver->set_attr_bool("precompile-header", false);
        }
        free(main_header);
#else
        /* TODO: support precompiled headers on Windows */
        driver->set_attr_bool("precompile-header", false);
#endif
    }

    if (!driver->get_attr("static")) {
        driver->set_attr_bool("static", false);
    }
    if (driver->get_attr("static")->is.boolean) {
        char *value = ut_asprintf("-D%s_STATIC", project->id_underscore);
        driver->set_attr_array("cflags", value);
        driver->set_attr_array("cxxflags", value);
        free(value);
    }

    if (!driver->get_attr("export-symbols")) {
        driver->set_attr_bool("export-symbols", false);
    }

    bool cpp = is_cpp(project);
    if (is_clang(cpp)) {
        driver->set_attr_string("ext-pch", "pch");
    } else {
        driver->set_attr_string("ext-pch", "gch");
    }

    char *tmp_dir  = ut_asprintf(
        CACHE_DIR UT_OS_PS "%s-%s", UT_PLATFORM_STRING, config->configuration);
    driver->set_attr_string("tmp-dir", tmp_dir);
    
    char *obj_dir = ut_asprintf("%s"UT_OS_PS"obj", tmp_dir);
    driver->set_attr_string("obj-dir", obj_dir);
    free(obj_dir);

    char *pch_dir = ut_asprintf("%s"UT_OS_PS"include", tmp_dir);
    driver->set_attr_string("pch-dir", pch_dir);
    free(pch_dir);

    if (!strcmp(driver->get_attr("c-standard")->is.string, "c89")) {
        driver->set_attr_array("cflags", "-D__BAKE_LEGACY__");
    }
    
    free(tmp_dir);
}

/* Initialize directory with new project */
static
void setup_project(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    bool is_template = project->type == BAKE_TEMPLATE;

    /* Get short project id */
    const char *id = project->id;
    bake_project_type kind = project->type;
    const char *short_id = project->id_base;

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

    if (is_template) {
        fprintf(f,
            "#include <${id underscore}.h>\n"
            "\n"
            "int main(int argc, char *argv[]) {\n"
            "    return 0;\n"
            "}\n");
    } else {
        fprintf(f,
            "#include <%s.h>\n"
            "\n"
            "int main(int argc, char *argv[]) {\n"
            "    return 0;\n"
            "}\n",
            project->id_underscore);
    }

    fclose(f);
    free(source_filename);

    /* Create main header file */
    char *header_filename;
    if (is_template) {
        header_filename = ut_asprintf("include/__id_underscore.h", short_id);
    } else {
        header_filename = ut_asprintf("include/%s.h", project->id_underscore);
    }
    
    f = fopen(header_filename, "w");
    if (!f) {
        ut_error("failed to open '%s'", header_filename);
        project->error = true;
        return;
    }

    if (is_template) {
        fprintf(f,
            "#ifndef ${id upper}_H\n"
            "#define ${id upper}_H\n\n"
            "/* This generated file contains includes for project dependencies */\n"
            "#include \"${id underscore}/bake_config.h\"\n");
    } else {
        /* Create upper-case id for defines in header file */
        char *id_upper = strdup(id);
        strupper(id_upper);
        char *ptr, ch;
        for (ptr = id_upper; (ch = *ptr); ptr ++) {
            if (ch == '/' || ch == '.') {
                ptr[0] = '_';
            }
        }

        fprintf(f,
            "#ifndef %s_H\n"
            "#define %s_H\n\n"
            "/* This generated file contains includes for project dependencies */\n"
            "#include \"%s/bake_config.h\"\n",
            id_upper,
            id_upper,
            project->id_dash);

        free(id_upper);
    }

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

static
void test(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    if (config->coverage && project->coverage) {
        clean_coverage(driver, config, project);
    }
}

static
void build(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    if (driver->get_attr_bool("precompile-header")) {
        generate_precompiled_header(driver, config, project);
    }
}

static
char *project_header_file(
    const char *project_id)
{
    char *project_header = malloc(strlen(project_id) + 3);
    strcpy(project_header, project_id);

    char *ptr, ch;
    for (ptr = project_header; (ch = *ptr); ptr ++) {
        if (ch == '.') {
            *ptr = '_';
        }
    }

    strcat(project_header, ".h");

    return project_header;
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
        char *project_header = project_header_file(project_id);

        bool include_found = false;
        char *file = ut_asprintf("%s/include/%s", config->home, project_header);
        if (ut_file_test(file) != 1) {
            free(file);
            file = ut_asprintf("%s/include/%s", config->target, project_header);
            if (ut_file_test(file) == 1) {
                include_found = true;
            }
        } else {
            include_found = true;
        }

        if (include_found) {
            if (!strcmp(project_id, "bake.util")) {
                fprintf(f, "#ifdef __BAKE__\n");
            }
            fprintf(f, "#include <%s>\n", project_header);
            if (!strcmp(project_id, "bake.util")) {
                fprintf(f, "#endif\n");
            }
            count ++;
        }

        free(file);
        free(project_header);
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
    const char *short_id = project->id_base;
    const char *snake_id = project->id_underscore;

    /* Create upper-case id for defines in header file */
    char *id_upper = ut_strdup(project->id_underscore);
    strupper(id_upper);

    /* Ensure include directory exists */
    ut_mkdir("%s/include/%s", project->path, project->id_dash);

    /* Create main header file */
    char *header_filename = ut_asprintf(
        "%s/include/%s/bake_config.h", project->path, project->id_dash);
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
" * dependencies will automatically show up in this file. Include bake_config.h\n"
" * in your main project file. Do not edit! */\n\n"
"#ifndef %s_BAKE_CONFIG_H\n"
"#define %s_BAKE_CONFIG_H\n\n",
        id_upper,
        id_upper);

    fprintf(f, "/* Headers of public dependencies */\n");
    add_dependency_includes(config, f, project->use);

    if (project->type == BAKE_PACKAGE) {

    if (project->use_private && ut_ll_count(project->use_private)) {
        fprintf(f, "\n/* Headers of private dependencies */\n");
        fprintf(f, "#ifdef %s_EXPORT\n", snake_id);
        add_dependency_includes(config, f, project->use_private);
        fprintf(f, "#endif\n");
    }

    fprintf(f, "\n/* Convenience macro for exporting symbols */\n");
    fprintf(f,
      "#ifndef %s_STATIC\n"
      "#if %s_EXPORTS && (defined(_MSC_VER) || defined(__MINGW32__))\n"
      "  #define %s_EXPORT __declspec(dllexport)\n"
      "#elif %s_EXPORTS\n"
      "  #define %s_EXPORT __attribute__((__visibility__(\"default\")))\n"
      "#elif defined _MSC_VER\n"
      "  #define %s_EXPORT __declspec(dllimport)\n"
      "#else\n"
      "  #define %s_EXPORT\n"
      "#endif\n"
      "#else\n"
      "  #define %s_EXPORT\n"
      "#endif\n",
        snake_id, snake_id, id_upper, snake_id, id_upper, id_upper, id_upper, id_upper);

    }

    fprintf(f, "%s", "\n#endif\n\n");
    fclose(f);
}

/* -- Rules */
UT_EXPORT 
int bakemain(bake_driver_api *driver) 
{
    ut_init("bake.c");

    /* Create pattern that matches source files */
    driver->pattern("SOURCES", "//*.c|*.cpp|*.cxx");


    /* -- Compiling and linking source code -- */

    /* Create rule for dynamically generating object files from source files */
    driver->rule("objects", "$SOURCES", driver->target_map(src_to_obj), compile_src);

    /* Create rule for creating binary from objects */
    driver->rule("ARTEFACT", "$objects", driver->target_pattern(NULL), link_binary);

    /* Generate header file that automatically includes project dependencies */
    driver->generate(generate);

    /* Always build precompiled header right before rules are executed */
    driver->build(build);

    /* Callback that initializes projects with the right build dependencies */
    driver->init(init);

    /* Cleanup files specific to a binary, like coverage data files */
    driver->test(test);

    /* Coverage analysis */
    driver->coverage(coverage);    

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
