/* Copyright (c) 2010-2018 the corto developers
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

static
int16_t bake_install_dir_for_target(
    char *id,
    char *source_path,
    char *dir,
    char *subdir,
    char *target,
    bool softlink)
{
    ut_dirstack stack = NULL;
    ut_iter it;

    char *source;
    if (!subdir) {
        source = ut_asprintf("%s%c%s", source_path, PATH_SEPARATOR_C, dir);
    } else {
        source = ut_asprintf("%s%c%s", source_path, PATH_SEPARATOR_C, subdir);
    }

    /* If source path does not exist, nothing needs to be copied. */
    if (!ut_file_test(source)) {
        free(source);
        return 0;
    }

    if (ut_dir_iter(source, NULL, &it)) goto error;

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *filepath = ut_asprintf("%s%c%s", source, PATH_SEPARATOR_C, file);

        if (ut_isdir(filepath)) {
            if (ut_os_match(file)) {
                ut_trace("install files for current OS in '%s'", file);
                if (bake_install_dir_for_target(
                    id, source_path, dir, file, target, softlink))
                {
                    goto error;
                }

                continue;
            } else if (!stricmp(file, "everywhere"))
            {
                /* Always copy all contents in everywhere */
                if (bake_install_dir_for_target(
                    id, source_path, dir, file, target, softlink))
                {
                    goto error;
                }
                continue;
            } else if (!stricmp(file, "private")) {
                /* Never copy files in private */
                continue;
            }
        }

        /* Construct target path */
        char *dst = ut_asprintf("%s%c%s", target, PATH_SEPARATOR_C, file);
        ut_path_clean(dst, dst);

        /* Copy file to target */
        if (softlink) {
            if (ut_symlink(filepath, dst)) goto error;
        } else {
            if (ut_cp(filepath, dst)) goto error;
        }

        free(dst);
        free(filepath);
    }

    free(source);

    return 0;
error:
    return -1;
}

static
int16_t bake_install_dir(
    bake_config *config,
    char *id,
    char *source_path,
    char *dir,
    char *subdir,
    bool softlink)
{
    char *target;

    if (id) {
        target = ut_envparse("%s%c%s%c%s",
            config->target, PATH_SEPARATOR_C,
            dir, PATH_SEPARATOR_C,
            id);
    } else {
        target = ut_envparse("%s%c%s",
            config->target, PATH_SEPARATOR_C,
            dir);
    }

    if (bake_install_dir_for_target(
        id,
        source_path,
        dir,
        subdir,
        target,
        softlink))
    {
        goto error;
    }

    return 0;
error:
    return -1;
}

int16_t bake_uninstall_from_env(
    const char *env,
    bake_project *project,
    bool uninstall)
{
    if (uninstall) {
        ut_try( ut_rm(strarg("%s%cmeta%c%s", env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->id)), NULL);

        if (project->type == BAKE_PACKAGE) {
            ut_try( ut_rm(strarg("%s%clib%c%s", env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->artefact)), NULL);
        } else if (project->type == BAKE_APPLICATION) {
            ut_try( ut_rm(strarg("%s%cbin%c%s", env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->artefact)), NULL);
        } else if (project->type == BAKE_TOOL) {
            ut_try( ut_rm(strarg("%s%c%s", env, PATH_SEPARATOR_C, project->artefact)), NULL);
        }
    }

    ut_try( ut_rm(strarg("%s%cetc%c%s", env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->id)), NULL);
    ut_try( ut_rm(strarg("%s%cinclude%c%s.dir", env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->id)), NULL);

    return 0;
error:
    return -1;
}

int16_t bake_install_clear(
    bake_config *config,
    bake_project *project,
    bool uninstall)
{
    ut_log_push("uninstall");
    ut_try(bake_uninstall_from_env(config->home, project, uninstall), NULL);
    ut_try(bake_uninstall_from_env(config->target, project, uninstall), NULL);

    char *link_name = ut_asprintf("%s%cinclude%c%s", config->target, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->id);
    ut_try( ut_rm(link_name), NULL);
    free(link_name);

    ut_log_pop();
    return 0;
error:
    ut_log_pop();
    return -1;
}

int16_t bake_install_uninstall(
    bake_config *config,
    const char *project_id)
{
    const char *project_dir = ut_locate(project_id, NULL, UT_LOCATE_PROJECT);
    if (!project_dir) {
        ut_throw("project '%s' not found", project_id);
        goto error;
    }

    bake_project *project = bake_project_new(project_dir, config);
    if (!project) {
        goto error;
    }

    ut_try( bake_install_clear(config, project, true), NULL);

#ifndef _WIN32
    ut_log("#[grey]uninstalled #[normal]'%s'\n", project->id);
#else
    ut_log("uninstalled '%s'\n", project->id);
#endif
error:
    return -1;
}

int16_t bake_install_metadata(
    bake_config *config,
    bake_project *project)
{
    if (project->type != BAKE_TOOL) {
        /* Copy project.json and file that points back to source */
        char *project_json = ut_asprintf("%s%cproject.json", project->path, PATH_SEPARATOR_C);
        if (ut_file_test(project_json)) {
            char *projectDir = ut_envparse(
                "%s%cmeta%c%s", config->target, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->id);

            ut_try (ut_mkdir(projectDir), NULL);

            /* Copy project file */
            if (ut_cp(project_json, projectDir)) {
                free(projectDir);
                goto error;
            }

            free(project_json);
            char *license = ut_asprintf("%s%cLICENSE", project->path, PATH_SEPARATOR_C);

            /* Copy license file */
            if (ut_file_test(license)) {
                if (ut_cp(license, projectDir)) {
                    ut_throw("failed to copy '%s' to '%s'",
                        license, projectDir);
                    goto error;
                }
            }

            /* Write project source location to package repository */
            FILE *src_location = fopen(strarg("%s%csource.txt", projectDir, PATH_SEPARATOR_C), "w");
            if (!src_location) {
                ut_throw("failed to write to '%s' for '%s'",
                    strarg("%s%csource.txt", projectDir, PATH_SEPARATOR_C),
                    project->id);
                goto error;
            }

            fprintf(src_location, "%s\n", project->fullpath);
            fclose(src_location);

            /* If project contains dependee JSON, write to dependee.json */
            if (project->dependee_json && strlen(project->dependee_json)) {
                FILE *dependee_config =
                    fopen(strarg("%s%cdependee.json", projectDir, PATH_SEPARATOR_C), "w");
                if (!dependee_config) {
                    ut_throw("failed to write to '%s' for '%s'",
                        strarg("%s%cdependee.json", projectDir, PATH_SEPARATOR_C),
                        project->id);
                    goto error;
                }
                fprintf(dependee_config, "%s\n", project->dependee_json);
                fclose(dependee_config);
                ut_trace("#[cyan]write %s%cdependee.json", projectDir, PATH_SEPARATOR_C);
            }
            free(projectDir);
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_install_prebuild(
    bake_config *config,
    bake_project *project)
{
    if (project->type != BAKE_TOOL) {

        /* Install files to project-specific locations in $BAKE_TARGET */
        ut_iter it = ut_ll_iter(project->includes);
        while (ut_iter_hasNext(&it)) {
            char *tmp_id = ut_asprintf("%s.dir", project->id);
            if (bake_install_dir(
                config,
                tmp_id,
                project->path,
                "include",
                ut_iter_next(&it),
                true))
            {
                free(tmp_id);
                goto error;
            }
            free(tmp_id);
        }

        /* Create softlink to main header file so projects can include packages
         * using their logical name */
        char *header_name = ut_asprintf("%s%cinclude%c%s.dir%c%s.h",
            config->target, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->id, PATH_SEPARATOR_C, project->id_short);
        if (ut_file_test(header_name) == 1) {
            char *link_name = ut_asprintf("%s%cinclude%c%s",
                config->target, PATH_SEPARATOR_C, PATH_SEPARATOR_C, project->id);
            FILE *f = fopen(link_name, "w");
            fprintf(f, "#include \"%s.dir/%s.h\"\n", project->id, project->id_short);
            fclose(f);
            free(link_name);
        }
        free(header_name);

        if (bake_install_dir(
            config, project->id, project->path, "etc", NULL, true))
        {
            goto error;
        }

        if (project->type == BAKE_PACKAGE) {
            if (bake_install_dir(
                config, project->id, project->path, "lib", NULL, true))
            {
                goto error;
            }
        }

        /* Install files to BAKE_TARGET directly from 'install' folder */
        char *install_path = ut_asprintf("%s%cinstall", project->path, PATH_SEPARATOR_C);
        if (install_path) {
            if (bake_install_dir(
                config, NULL, install_path, "include", NULL, true))
            {
                goto error;
            }
            if (bake_install_dir(
                config, NULL, install_path, "lib", NULL, true))
            {
                goto error;
            }
            if (bake_install_dir(
                config, NULL, install_path, "etc", NULL, true))
            {
                goto error;
            }
            if (bake_install_dir(
                config, NULL, install_path, "java", NULL, true))
            {
                goto error;
            }
        }
        free(install_path);
    }

    return 0;
error:
    return -1;
}

static
int16_t bake_post_install_bin(
    bake_config *config,
    bake_project *project,
    const char *targetDir)
{
    char *bin_path = project->artefact_path;

    if (ut_file_test(bin_path) == 1) {
        ut_iter it;
        ut_try( ut_dir_iter(bin_path, NULL, &it), NULL);

        while (ut_iter_hasNext(&it)) {
            char *file = ut_iter_next(&it);
            char *file_path = ut_asprintf("%s%c%s", bin_path, PATH_SEPARATOR_C, file);
            ut_try( ut_cp(file_path, targetDir), NULL);
            free(file_path);
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_install_postbuild(
    bake_config *config,
    bake_project *project)
{
    char *kind, *subdir, *targetDir = NULL;
    bool copy = false;

    if (project->type == BAKE_PACKAGE) {
        targetDir = config->target_lib;
    } else {
        targetDir = config->target_bin;
    }

    if (!ut_file_test(project->artefact_file)) {
        ut_throw("cannot find artefact '%s'", project->artefact_file);
        goto error;
    }

    if (ut_isdir(project->artefact_file)) {
        ut_throw(
            "specified artefact '%s' is a directory, expecting regular file",
            project->artefact_file);
        goto error;
    }

    if (ut_mkdir(targetDir)) {
        goto error;
    }

    char *targetBinary = ut_asprintf("%s%c%s", targetDir, PATH_SEPARATOR_C, project->artefact);

    if (!ut_file_test(targetBinary) || project->changed || !project->language) {
        /* Copy binary */
        if (ut_cp(project->artefact_file, targetBinary)) {
            goto error;
        }

        /* Ensure that time on the local system has progressed past the point of the
         * file timestamp. If the build is running in a VM, the clock between the
         * client and host could be out of sync temporarily, which can result in
         * strange behavior. */
        char *installedArtefact = ut_asprintf("%s%c%s", targetDir, PATH_SEPARATOR_C, project->artefact);
        time_t t_artefact = ut_lastmodified(installedArtefact);
        time_t t = time(NULL);
        int i = 0;
        while (t_artefact > time(NULL) && i < 10) {
            ut_sleep(0, 100000000); /* sleep 100msec */
            i ++;
        }
        if (i == 10) {
            ut_warning(
                "clock drift of >1sec between the OS clock and the filesystem detected");
        }
        free(installedArtefact);
    }

    free(targetBinary);

    return 0;
error:
    if (targetDir) free(targetDir);
    return -1;
}
