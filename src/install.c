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
    bool softlink,
    FILE *uninstallFile)
{
    ut_dirstack stack = NULL;
    ut_iter it;

    char *source;
    if (!subdir) {
        source = ut_asprintf("%s/%s", source_path, dir);
    } else {
        source = ut_asprintf("%s/%s", source_path, subdir);
    }

    /* If source path does not exist, nothing needs to be copied. */
    if (!ut_file_test(source)) {
        free(source);
        return 0;
    }

    if (!(stack = ut_dirstack_push(stack, source))) goto error;

    if (ut_dir_iter(ut_dirstack_wd(stack), NULL, &it)) goto error;

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *filepath = ut_asprintf("%s/%s", ut_dirstack_wd(stack), file);

        if (ut_isdir(filepath)) {
            if (ut_os_match(file)) {
                ut_trace("install files for current OS in '%s'", file);
                if (bake_install_dir_for_target(
                    id, source_path, dir, file, target, softlink, uninstallFile))
                {
                    goto error;
                }

                continue;
            } else if (!stricmp(file, "everywhere"))
            {
                /* Always copy all contents in everywhere */
                if (bake_install_dir_for_target(
                    id, source_path, dir, file, target, softlink, uninstallFile))
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
        char *dst = ut_asprintf("%s/%s", target, file);
        ut_path_clean(dst, dst);

        /* Copy file to target */
        if (softlink) {
            if (ut_symlink(filepath, dst)) goto error;
        } else {
            if (ut_cp(filepath, dst)) goto error;
        }

        fprintf(uninstallFile, "%s\n", dst);
        free(dst);
        free(filepath);
    }

    ut_dirstack_pop(stack);

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
    bool softlink,
    FILE *uninstallFile)
{
    char *target;

    if (id) {
        target = ut_envparse("%s/%s/%s",
            config->target,
            dir,
            id);
    } else {
        target = ut_envparse("%s/%s",
            config->target,
            dir);
    }

    if (bake_install_dir_for_target(
        id,
        source_path,
        dir,
        subdir,
        target,
        softlink,
        uninstallFile))
    {
        goto error;
    }

    return 0;
error:
    return -1;
}

static
char* bake_uninstaller_filename(
    bake_config *config,
    bake_project *project)
{
    return ut_envparse(
        "%s/meta/%s/uninstaller.txt",
        config->target, project->id);
}

static
FILE* bake_uninstaller_open(
    bake_config *config,
    bake_project *project,
    const char *mode)
{
    char *filename = bake_uninstaller_filename(config, project);
    FILE *result = ut_file_open(filename, mode);
    free(filename);
    return result;
}

int16_t bake_install_clear(
    bake_config *config,
    bake_project *project,
    bool uninstall)
{
    ut_iter it;

    ut_log_push("uninstall");

    if (project->type != BAKE_TOOL) {
        char *projectDir = ut_asprintf(
            "%s/meta/%s", config->target, project->id);

        /* Uninstall files are stored in the project directory, try uninstalling
         * first by removing all files in uninstaller file. */
        if (ut_file_test(projectDir)) {
            char *filename = bake_uninstaller_filename(config, project);
            if (ut_file_iter(filename, &it)) {
                ut_catch(); /* Catch last error */
                if (uninstall) {
                    ut_warning(
                        "missing uninstaller for project '%s'", project->id);
                }
                free(filename);
                goto skip;
            }

            while (ut_iter_hasNext(&it)) {
                char *line = ut_iter_next(&it);
                if (!line || !line[0]) continue; /* Skip empty lines in file */
                if (line[0] == '/') {
                    if (ut_rm(line)) {
                        ut_warning("failed to uninstall '%s' for '%s'",
                            line,
                            project->id);
                    }
                } else {
                    ut_warning(
                        "ignoring '%s' in uninstaller.txt, path should be absolute",
                        line);
                }
            }

            /* Remove uninstaller */
            if (ut_rm(filename)) {
                ut_warning("failed to remove '%s'", filename);
            }

            /* Free project directory if it's empty (no nested packages) */
            if (ut_dir_isEmpty(projectDir)) ut_rm(projectDir);
            free(projectDir);
        }
    }

skip:
    ut_log_pop();
    return 0;
error:
    ut_log_pop();
    return -1;
}

int16_t bake_install_uninstall(
    bake_config *config,
    bake_project *project)
{
    ut_try( bake_install_clear(config, project, true), NULL);

    char *projectDir = ut_envparse(
        "%s/meta/%s", config->target, project->id);

    if (ut_rm(projectDir)) {
        ut_warning("failed to remove '%s'", projectDir);
    }

    free(projectDir);

    /* In the case of an unsuccessful or interrupted uninstallation, some parts
     * may be left behind. Check for lingering directories, and clean up. */

    /* If include directory exists and is empty, clean up */
    char *inc = ut_asprintf(
        "%s/include/%s", config->target, project->id);
    if (ut_file_test(inc) && ut_dir_isEmpty(inc)) ut_rm(inc);
    free(inc);

    /* If etc directory exists and is empty, clean up */
    char *etc = ut_envparse(
        "%s/etc/%s", config->target, project->id);
    if (ut_file_test(etc) && ut_dir_isEmpty(etc)) ut_rm(etc);
    free(etc);

    return 0;
error:
    return -1;
}

int16_t bake_install_metadata(
    bake_config *config,
    bake_project *project)
{
    if (project->type != BAKE_TOOL) {
        /* Copy project.json and file that points back to source */
        char *project_json = ut_asprintf("%s/project.json", project->path);
        if (ut_file_test(project_json)) {
            char *projectDir = ut_envparse(
                "%s/meta/%s", config->target, project->id);

            ut_try (ut_mkdir(projectDir), NULL);

            /* Copy project file */
            if (ut_cp(project_json, projectDir)) {
                free(projectDir);
                goto error;
            }

            free(project_json);
            char *license = ut_asprintf("%s/LICENSE", project->path);

            /* Copy license file */
            if (ut_file_test(license)) {
                if (ut_cp(license, projectDir)) {
                    ut_throw("failed to copy '%s' to '%s'",
                        license, projectDir);
                    goto error;
                }
            }

            /* Write project source location to package repository */
            FILE *src_location = fopen(strarg("%s/source.txt", projectDir), "w");
            if (!src_location) {
                ut_throw("failed to write to '%s' for '%s'",
                    strarg("%s/source.txt", projectDir),
                    project->id);
                goto error;
            }
            fprintf(src_location, "%s\n", project->path);
            fclose(src_location);

            /* If project contains dependee JSON, write to dependee.json */
            if (project->dependee_json && strlen(project->dependee_json)) {
                FILE *dependee_config =
                    fopen(strarg("%s/dependee.json", projectDir), "w");
                if (!dependee_config) {
                    ut_throw("failed to write to '%s' for '%s'",
                        strarg("%s/dependee.json", projectDir),
                        project->id);
                    goto error;
                }
                fprintf(dependee_config, "%s\n", project->dependee_json);
                fclose(dependee_config);
                ut_trace("#[cyan]write %s/dependee.json", projectDir);
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
        FILE *uninstallFile = bake_uninstaller_open(config, project, "a");
        if (!uninstallFile) {
            goto error;
        }

        /* Install files to project-specific locations in $BAKE_TARGET */
        ut_iter it = ut_ll_iter(project->includes);
        while (ut_iter_hasNext(&it)) {
            if (bake_install_dir(
                config, project->id, project->path, "include", ut_iter_next(&it), true,
                uninstallFile))
            {
                goto error;
            }
        }

        if (bake_install_dir(
            config, project->id, project->path, "etc", NULL, true, uninstallFile))
        {
            goto error;
        }

        if (project->type == BAKE_PACKAGE) {
            if (bake_install_dir(
                config, project->id, project->path, "lib", NULL, true, uninstallFile))
            {
                goto error;
            }
        }

        /* Install files to BAKE_TARGET directly from 'install' folder */
        char *install_path = ut_asprintf("%s/install", project->path);
        if (install_path) {
            if (bake_install_dir(
                config, NULL, install_path, "include", NULL, true, uninstallFile))
            {
                goto error;
            }
            if (bake_install_dir(
                config, NULL, install_path, "lib", NULL, true, uninstallFile))
            {
                goto error;
            }
            if (bake_install_dir(
                config, NULL, install_path, "etc", NULL, true, uninstallFile))
            {
                goto error;
            }
            if (bake_install_dir(
                config, NULL, install_path, "java", NULL, true, uninstallFile))
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
            char *file_path = ut_asprintf("%s/%s", bin_path, file);
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

    char *targetBinary = ut_asprintf("%s/%s", targetDir, project->artefact);

    if (!ut_file_test(targetBinary) || project->freshly_baked) {
        /* Copy binary */
        if (ut_cp(project->artefact_file, targetBinary)) {
            goto error;
        }

        /* Ensure that time on the local system has progressed past the point of the
         * file timestamp. If the build is running in a VM, the clock between the
         * client and host could be out of sync temporarily, which can result in
         * strange behavior. */
        char *installedArtefact = ut_asprintf("%s/%s", targetDir, project->artefact);
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
