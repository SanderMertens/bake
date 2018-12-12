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

#include "bake.h"

int bake_do_build(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *project)
{
    /* Step 2: export metadata to environment to make project discoverable */
    ut_log_push("install_metadata");
    if (project->public)
        ut_try (bake_install_metadata(config, project), NULL);
    ut_log_pop();

    /* Step 3: parse driver configuration in project JSON */
    ut_log_push("load_drivers");
    ut_try (bake_project_parse_driver_config(config, project), NULL);
    ut_log_pop();

    /* Step 4: parse dependee configuration */
    ut_log_push("load_dependees");
    ut_try (bake_project_parse_dependee_config(config, project), NULL);
    ut_log_pop();

    /* Step 5: now that project config is fully loaded, check dependencies */
    ut_log_push("validate_dependencies");
    ut_try (bake_project_check_dependencies(config, project), NULL);
    ut_log_pop();

    /* Step 6: invoke code generators, if any */
    ut_log_push("generate_code");
    ut_try (bake_project_generate(config, project), NULL);
    ut_log_pop();

    /* Step 7: clear environment of old project files */
    ut_log_push("clear");
    if (project->public)
        ut_try (bake_install_clear(config, project, false), NULL);
    ut_log_pop();

    /* Step 8: export project files to environment */
    ut_log_push("install_prebuild");
    if (project->public)
        ut_try (bake_install_prebuild(config, project), NULL);
    ut_log_pop();

    /* Step 9: build generated projects, in case code generation created any */
    ut_log_push("build_generated");
    ut_try (bake_project_build_generated(config, project), NULL);
    ut_log_pop();

    /* Step 10: build project */
    ut_log_push("build");
    if (project->artefact)
        ut_try (bake_project_build(config, project), NULL);
    ut_log_pop();

    /* Step 11: install binary to environment */
    ut_log_push("install_postbuild");
    if (project->public && project->artefact)
        ut_try (bake_install_postbuild(config, project), NULL);
    ut_log_pop();

    return 0;
error:
    return -1;
}

int bake_do_clean(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *project)
{
    return bake_project_clean(config, project);
}

int bake_do_rebuild(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *project)
{
    ut_try( bake_project_clean_current_platform(config, project), NULL);

    ut_try( bake_do_build(config, crawler, project), NULL);

    return 0;
error:
    return -1;
}

int bake_do_install(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *project)
{
    if (!project->public) return 0;

    /* Step 1: export metadata to environment to make project discoverable */
    ut_try (bake_install_metadata(config, project), NULL);

    /* Step 2: export project files to environment */
    ut_try (bake_install_prebuild(config, project), NULL);

    /* Step 3: install binary to environment */
    if (project->artefact)
        ut_try (bake_install_postbuild(config, project), NULL);

    return 0;
error:
    return -1;
}

int bake_do_uninstall(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *project)
{
    ut_try (bake_install_uninstall(config, project), NULL);

    return 0;
error:
    return -1;
}

int bake_do_foreach(
    bake_config *config,
    bake_crawler *crawler,
    bake_project *project)
{
    return 0;
}
