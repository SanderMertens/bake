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
#include <bake_test.h>

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
int generate_testcase_fwd_decls(
    ut_code *src,
    JSON_Array *suites)
{
    uint32_t i, count = json_array_get_count(suites);

    /* The JSON structure has already been validated, so no need to do error
     * checking again. */

    for (i = 0; i < count; i ++) {
        JSON_Object *suite = json_array_get_object(suites, i);
        const char *id = json_object_get_string(suite, "id");

        ut_code_write(src, "// Testsuite '%s'\n", id);

        int has_setup = json_object_get_boolean(suite, "setup");
        if (has_setup && has_setup != -1) {
            ut_code_write(src, "void %s_setup(void);\n", id);
        }

        int has_teardown = json_object_get_boolean(suite, "teardown");
        if (has_teardown && has_teardown != -1) {
            ut_code_write(src, "void %s_teardown(void);\n", id);
        }    

        JSON_Array *testcases = json_object_get_array(suite, "testcases");
        uint32_t t, t_count = json_array_get_count(testcases);

        for (t = 0; t < t_count; t ++) {
            const char *testcase = json_array_get_string(testcases, t);
            ut_code_write(src, "void %s_%s(void);\n", id, testcase);
        }

        ut_code_write(src, "\n");
    }

    return 0;
}

static
int generate_suite_data(
    ut_code *src,
    JSON_Array *suites)
{
    uint32_t i, count = json_array_get_count(suites);

    /* The JSON structure has already been validated, so no need to do error
     * checking again. */

    for (i = 0; i < count; i ++) {
        JSON_Object *suite = json_array_get_object(suites, i);
        const char *id = json_object_get_string(suite, "id");

        JSON_Array *testcases = json_object_get_array(suite, "testcases");
        uint32_t t, t_count = json_array_get_count(testcases);

        if (i) {
            ut_code_write(src, ",\n");
        }

        ut_code_write(src, "{\n");
        ut_code_indent(src);
        ut_code_write(src, "\"%s\",\n", id);        

        int has_setup = json_object_get_boolean(suite, "setup");
        if (has_setup && has_setup != -1) {
            ut_code_write(src, "%s_setup,\n", id);
        } else {
            ut_code_write(src, "NULL,\n");
        }

        int has_teardown = json_object_get_boolean(suite, "teardown");
        if (has_teardown && has_teardown != -1) {
            ut_code_write(src, "%s_teardown,\n", id);
        } else {
            ut_code_write(src, "NULL,\n");
        }

        ut_code_write(src, "%d,\n", t_count);
        ut_code_write(src, "(bake_test_case[]){");
        ut_code_indent(src);

        for (t = 0; t < t_count; t ++) {
            const char *testcase = json_array_get_string(testcases, t);
            if (t) {
                ut_code_write(src, ",");
            }
            ut_code_write(src, "\n");
            ut_code_write(src, "{\n");
            ut_code_indent(src);
            ut_code_write(src, "\"%s\",\n", testcase);
            ut_code_write(src, "%s_%s\n", id, testcase);
            ut_code_dedent(src);
            ut_code_write(src, "}");
        }
        ut_code_write(src, "\n");

        ut_code_dedent(src);
        ut_code_write(src, "}\n");

        ut_code_dedent(src);
        ut_code_write(src, "}");
    }

    ut_code_write(src, "\n");

    return 0;
}

static
int generate_testmain(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    JSON_Array *suites)
{
    (void)driver;
    (void)config;
    
    const char *ext = "c";
    if (is_cpp(project)) {
        ext = "cpp";
    }

    ut_code *src = ut_code_open("%s/src/main.%s", project->path, ext);
    if (!src) {
        fprintf(stderr, "failed to open %s/src/main.%s", project->path, ext);
        project->error = true;
        return -1;
    }

    ut_code_write(src, "\n");
    ut_code_write(src, "/* A friendly warning from bake.test\n");
    ut_code_write(src, " * ----------------------------------------------------------------------------\n");
    ut_code_write(src, " * This file is generated. To add/remove testcases modify the 'project.json' of\n");
    ut_code_write(src, " * the test project. ANY CHANGE TO THIS FILE IS LOST AFTER (RE)BUILDING!\n");
    ut_code_write(src, " * ----------------------------------------------------------------------------\n");
    ut_code_write(src, " */\n");

    ut_code_write(src, "\n");

    ut_code_write(src, "#include <%s.h>\n", project->id_base);
    ut_code_write(src, "\n");

    generate_testcase_fwd_decls(src, suites);

    ut_code_write(src, "static bake_test_suite suites[] = {\n");
    ut_code_indent(src);

    generate_suite_data(src, suites);

    ut_code_dedent(src);
    ut_code_write(src, "};\n");
    ut_code_write(src, "\n");
    
    ut_code_write(src, "int main(int argc, char *argv[]) {\n");
    ut_code_indent(src);
    ut_code_write(src, "ut_init(argv[0]);\n");
    ut_code_write(src, "return bake_test_run(\"%s\", argc, argv, suites, %d);\n",
        project->id,
        json_array_get_count(suites));
    ut_code_dedent(src);
    ut_code_write(src, "}\n");

    ut_code_close(src);

    return 0;
}

static
void generate_testcase(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *suite,
    const char *testcase,
    cdiff_file suite_file)
{
    cdiff_file_elemBegin(suite_file, "%s_%s", suite, testcase);

    cdiff_file_headerBegin(suite_file);
    cdiff_file_write(suite_file, "void %s_%s() {", suite, testcase);
    cdiff_file_headerEnd(suite_file);

    if (!cdiff_file_bodyBegin(suite_file)) {
        cdiff_file_write(suite_file, "\n");
        cdiff_file_indent(suite_file);
        cdiff_file_write(suite_file, "// Implement testcase\n");
        cdiff_file_dedent(suite_file);
        cdiff_file_write(suite_file, "}\n");
        cdiff_file_bodyEnd(suite_file);
    }

    cdiff_file_elemEnd(suite_file);
}

static
int generate_suite(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    JSON_Object *suite)
{
    const char *id = json_object_get_string(suite, "id");
    if (!id) {
        fprintf(stderr, "testsuite is missing id attribute");
        return -1;
    }

    JSON_Array *cases = json_object_get_array(suite, "testcases");
    if (!json_array_get_count(cases)) {
        fprintf(stdout, 
            "Testsuite '%s' is empty, add testcases to the 'testcases' array\n",
            id);
        return 0;
    }

    const char *ext = "c";
    if (is_cpp(project)) {
        ext = "cpp";
    }

    char *file = ut_asprintf("%s/src/%s.%s", project->path, id, ext);

    cdiff_file suite_file = cdiff_file_open(file);
    if (!suite_file) {
        fprintf(stderr, "failed to open file %s for test suite %s", 
            file, id);
        
        free(file);
        return -1;
    }

    cdiff_file_write(suite_file, "#include <%s.h>\n", project->id_underscore);

    int has_setup = json_object_get_boolean(suite, "setup");
    if (has_setup && has_setup != -1) {
        generate_testcase(driver, config, project, id, "setup", suite_file);
    }

    int has_teardown = json_object_get_boolean(suite, "teardown");
    if (has_teardown && has_teardown != -1) {
        generate_testcase(driver, config, project, id, "teardown", suite_file);
    }    

    if (cases) {
        uint32_t i, count = json_array_get_count(cases);
        for (i = 0; i < count; i ++) {
            const char *testcase = json_array_get_string(cases, i);
            if (testcase) {
                generate_testcase(
                    driver, config, project, id, testcase, suite_file);
            }
        }
    }

    cdiff_file_close(suite_file);
    free(file);

    return 0;
}

static
void generate(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    JSON_Object *jo = driver->get_json();
    if (jo) {
        JSON_Array *suites = json_object_get_array(jo, "testsuites");
        if (suites) {
            uint32_t i, count = json_array_get_count(suites);
            for (i = 0; i < count; i ++) {
                JSON_Object *suite = json_array_get_object(suites, i);
                if (suite) {
                    if (generate_suite(driver, config, project, suite)) {
                        project->error = true;
                        break;
                    }
                } else {
                    fprintf(stderr, 
                        "unexpected element in testsuites array: expected object");
                    project->error = true;
                    break;
                }
            }

            generate_testmain(driver, config, project, suites);
        } else {
            fprintf(stderr, "no 'testsuites' array in test configuration\n");
            project->error = true;
        }
    } else {
        fprintf(stderr, "missing configuration for test framework\n");
    }
}

static
void init(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    if (project->type != BAKE_APPLICATION) {
        fprintf(stderr, "test projects must be of type application\n");
        project->error = true;
    } else {
        driver->use("bake.util");
        driver->use("bake.test");
    }
}

BAKE_TEST_EXPORT
int bakemain(bake_driver_api *driver) 
{
    ut_init("bake.test");

    /* Generate code based on configuration in project.json */
    driver->generate(generate);

    /* Initialize new projects to add correct dependency */
    driver->init(init);

    return 0;
}
