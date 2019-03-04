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

#include <bake>

#include <include/test.h>

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
void generate_testcase(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project,
    const char *testcase,
    cdiff_file suite_file)
{
    cdiff_file_elemBegin(suite_file, testcase);

    cdiff_file_headerBegin(suite_file);
    cdiff_file_write(suite_file, "void %s() {", testcase);
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

    cdiff_file_write(suite_file, "#include <include/test.h>\n");

    JSON_Array *cases = json_object_get_array(suite, "testcases");
    if (cases) {
        uint32_t i, count = json_array_get_count(cases);
        for (i = 0; i < count; i ++) {
            const char *testcase = json_array_get_string(cases, i);
            if (testcase) {
                generate_testcase(
                    driver, config, project, testcase, suite_file);
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

}

UT_EXPORT 
int bakemain(bake_driver_api *driver) 
{
    ut_init("bake.test");

    /* Generate code based on configuration in project.json */
    driver->generate(generate);

    /* Initialize new projects to add correct dependency */
    driver->init(init);

    return 0;
}
