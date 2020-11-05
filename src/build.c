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

#include "bake.h"

/* At this stage, the current project configuration has been parsed, and drivers
 * have been loaded */
int bake_do_pre_discovery(
    bake_config *config,
    bake_project *project)
{
    if (project->type == BAKE_TEMPLATE) {
        ut_log_push("install-template");
        ut_try( bake_install_template(config, project), NULL);
        ut_log_pop();
    } else {
        /* Step 1: export metadata to environment to make project discoverable */
        ut_log_push("install-metadata");
        if (project->public)
            ut_try (bake_install_metadata(config, project), NULL);
        ut_log_pop();

        /* Step 2: parse driver configuration in project JSON */
        ut_log_push("load-driver-config");
        ut_try (bake_project_parse_driver_config(config, project), NULL);
        ut_log_pop();

        /* Step 5: load bundle configuration */
        ut_log_push("load-bundle");
        ut_try (bake_project_load_bundle(config, project, NULL), NULL);
        ut_log_pop();         
    }

    return 0;
error:
    ut_log_pop();
    return -1;
}

/* At this stage, all projects have been discovered */
int bake_do_post_discovery(
    bake_config *config,
    bake_project *project)
{
    if (project->type == BAKE_TEMPLATE) {
        return 0;
    }

    /* Step 3: initialize drivers */
    ut_log_push("init-drivers");
    ut_try (bake_project_init_drivers(config, project), NULL);
    ut_log_pop();   

    /* Step 4: parse dependee configuration */
    ut_log_push("load-dependees");
    ut_try (bake_project_parse_dependee_config(config, project), NULL);
    ut_log_pop();

    return 0;
error:
    ut_log_pop();
    return -1;
}

/* At this stage, the project configuration is fully loaded (including dependee
 * configuration), and all dependencies are built or found in the bake env. */
static
int bake_do_build_intern(
    bake_config *config,
    bake_project *project,
    bool rebuild)
{
    if (project->type == BAKE_TEMPLATE) {
        return 0;
    }

    /* If generating assembly code, always build the code */
    if (config->assembly) {
        rebuild = true;
    }

    /* If any of the steps invoke bake, they may invoke the bake script, which
     * can reset the LD_LIBRARY_PATH environment variable. Setting this variable
     * to false will cause bake to fork itself again after the environment is
     * set correctly. */
    ut_setenv("BAKE_CHILD", "FALSE");

    /* Step 5: if rebuilding, clean project cache for current platform/config */
    if (rebuild) {
        ut_log_push("clean-cache");

        ut_try( bake_project_clean_current_platform(config, project), NULL);

        /* If project sets keep-binary to true, make sure to only rebuild when
         * the user is explicitly rebuilding this project. That way, even when
         * rebuilding a tree of projects, keep-binary projects won't get
         * rebuilt. This feature is useful if a tree contains projects that take
         * a long time to build, and for projects that want to version control
         * binaries. */
        if (project->keep_binary) {
            /* Only clean if just one project was discovered */
            if (bake_crawler_count() == 1) {

                /* Only remove the artefact for the current platform */
                ut_try( ut_rm(project->artefact_path), NULL);
            }
        }

        ut_log_pop();
    }

    /* Step 6: now that project config is fully loaded, check dependencies */
    ut_log_push("validate-dependencies");
    ut_try (bake_project_check_dependencies(config, project), NULL);
    ut_log_pop();

    /* Step 7: invoke code generators, if any */
    ut_log_push("generate");
    ut_try (bake_project_generate(config, project), NULL);
    ut_log_pop();

    /* Step 8: clear environment of old project files */
    ut_log_push("clear");
    if (project->public && project->type != BAKE_TOOL)
        ut_try (bake_install_clear(config, project, project->id, false), NULL);
    ut_log_pop();

    /* Step 9: export project files to environment */
    ut_log_push("install-prebuild");
    if (project->public && project->type != BAKE_TOOL)
        ut_try (bake_install_prebuild(config, project), NULL);
    ut_log_pop();

    /* Step 10: prebuild step */
    ut_log_push("prebuild");
    ut_try (bake_project_prebuild(config, project), NULL);
    ut_log_pop();

    /* Step 11: build project */
    ut_log_push("build");
    if (project->artefact)
        ut_try (bake_project_build(config, project), NULL);
    ut_log_pop();

    /* Step 12: postbuild step */
    ut_log_push("postbuild");
    if (!config->assembly)
        ut_try (bake_project_postbuild(config, project), NULL);
    ut_log_pop();

    /* Step 13: install binary to environment */
    ut_log_push("install-postbuild");
    if (project->public && project->artefact && !config->assembly)
        ut_try (bake_install_postbuild(config, project), NULL);
    ut_log_pop();

    /* Reset environment variable */
    ut_setenv("BAKE_CHILD", "TRUE");

    return 0;
error:
    ut_log_pop();
    return -1;
}

int bake_do_build(
    bake_config *config,
    bake_project *project)
{
    if (project->type == BAKE_TEMPLATE) {
        return 0;
    }

    return bake_do_build_intern(config, project, false);
}

int bake_do_clean(
    bake_config *config,
    bake_project *project)
{
    if (project->type == BAKE_TEMPLATE) {
        return 0;
    }

    return bake_project_clean(config, project);
}

int bake_do_rebuild(
    bake_config *config,
    bake_project *project)
{
    if (project->type == BAKE_TEMPLATE) {
        return 0;
    }

    return bake_do_build_intern(config, project, true);
}

int bake_do_install(
    bake_config *config,
    bake_project *project)
{
    if (!project->public) return 0;

    if (project->type == BAKE_TEMPLATE) {
        ut_try( bake_install_template(config, project), NULL);
    } else {
        /* Step 1: export metadata to environment to make project discoverable */
        ut_try (bake_install_metadata(config, project), NULL);

        /* Step 2: export project files to environment */
        ut_try (bake_install_prebuild(config, project), NULL);

        /* Step 3: install binary to environment */
        if (project->artefact)
            ut_try (bake_install_postbuild(config, project), NULL);
    }

    return 0;
error:
    return -1;
}
