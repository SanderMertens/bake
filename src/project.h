/* Copyright (c) 2010-2018 Sander Mertens
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

/** Create new project */
bake_project* bake_project_new(
    const char *path,
    bake_config *cfg);

/* Initialize project that wasn't loaded from a project.json */
int16_t bake_project_init(
    bake_config *config,
    bake_project *project);

/** Cleanup project */
void bake_project_free(
    bake_project *p);

/* Parse configuration of drivers */
int bake_project_parse_driver_config(
    bake_config *config,
    bake_project *project);

/* Init driver */
int bake_project_init_drivers(
    bake_config *config,
    bake_project *project);

/* Parse configuration from dependees */
int16_t bake_project_parse_dependee_config(
    bake_config *config,
    bake_project *project);

/* Check if project dependencies are newer */
int16_t bake_project_check_dependencies(
    bake_config *config,
    bake_project *project);

/* Generate code */
int16_t bake_project_generate(
    bake_config *config,
    bake_project *project);

/* Prebuild step */
int16_t bake_project_prebuild(
    bake_config *config,
    bake_project *project);

/* Postbuild step */
int16_t bake_project_postbuild(
    bake_config *config,
    bake_project *project);

/* Build project */
int16_t bake_project_build(
    bake_config *config,
    bake_project *project);

/* Clean project */
int16_t bake_project_clean(
    bake_config *config,
    bake_project *project);

/* Initialize files for a new bake project */
int16_t bake_project_setup(
    bake_config *config,
    bake_project *project);

/* Initialize files for a new bake project from template */
int16_t bake_project_setup_w_template(
    bake_config *config,
    bake_project *project,
    const char *template_id);    

/* Only clean current platform (used by rebuild) */
int16_t bake_project_clean_current_platform(
    bake_config *config,
    bake_project *project);

/* Initialize files for a new bake project */
int16_t bake_project_should_ignore(
    bake_project *project,
    const char *file);

/* Get driver specific attribute */
bake_attr* bake_project_get_attr(
    bake_project *project,
    const char *driver_id,
    const char *attr);

/* Set string attribute value */
bake_attr* bake_project_set_attr_string(
    bake_config *config,
    bake_project *project,
    const char *driver_id,
    const char *attr,
    const char *value);

/* Set bool attribute value */
bake_attr* bake_project_set_attr_bool(
    bake_config *config,
    bake_project *project,
    const char *driver_id,
    const char *attr,
    bool value);

/* Add value to array attribute */
bake_attr* bake_project_set_attr_array(
    bake_config *config,
    bake_project *project,
    const char *driver_id,
    const char *attr,
    const char *value);

/* Transform project type to string */
const char* bake_project_type_str(
    bake_project_type type);
