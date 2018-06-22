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
    char *dir,
    char *subdir,
    char *target,
    bool softlink,
    FILE *uninstallFile)
{
    corto_dirstack stack = NULL;
    corto_iter it;

    char *source;
    if (!subdir) {
        source = strdup(dir);
    } else {
        source = strdup(subdir);
    }

    /* If source path does not exist, nothing needs to be copied. */
    if (!corto_file_test(source)) {
        free(source);
        return 0;
    }

    if (!(stack = corto_dirstack_push(stack, source))) goto error;

    if (corto_dir_iter(".", NULL, &it)) goto error;

    while (corto_iter_hasNext(&it)) {
        char *file = corto_iter_next(&it);
        if (corto_isdir(file)) {
            if (!strnicmp(file, strlen("linux-"), "linux-") ||
                !strnicmp(file, strlen("darwin-"), "darwin-") ||
                !strnicmp(file, strlen("windows-"), "windows-"))
            {
                if (corto_os_match(file)) {
                    if (bake_install_dir_for_target(
                        id, dir, file, target, softlink, uninstallFile))
                    {
                        goto error;
                    }
                } else {
                    /* If directory contains platform-specific content but does not
                     * match current platform, skip */
                }
                continue;
            } else if (!stricmp(file, "everywhere"))
            {
                /* Always copy all contents in everywhere */
                if (bake_install_dir_for_target(
                    id, dir, file, target, softlink, uninstallFile))
                {
                    goto error;
                }
                continue;
            }
        }

        /* Construct target path */
        char *dst = corto_asprintf("%s/%s", target, file);
        corto_path_clean(dst, dst);

        /* Copy file to target */
        if (softlink) {
            if (corto_symlink(file, dst)) goto error;
        } else {
            if (corto_cp(file, dst)) goto error;
        }

        fprintf(uninstallFile, "%s\n", dst);
        free(dst);
    }

    if (corto_dirstack_pop(stack)) goto error;

    free(source);

    return 0;
error:
    return -1;
}

static
int16_t bake_install_dir(
    bake_config *config,
    char *id,
    char *dir,
    char *subdir,
    bool softlink,
    FILE *uninstallFile)
{
    char *target;
    if (id) {
        target = corto_envparse("%s/%s/%s",
            config->rootpath,
            dir,
            id);
    } else {
        target = corto_envparse("%s/%s",
            config->rootpath,
            dir);
    }

    if (bake_install_dir_for_target(
        id,
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
    return corto_envparse(
        "%s/meta/%s/uninstaller.txt",
        config->rootpath, project->id);
}

static
FILE* bake_uninstaller_open(
    bake_config *config,
    bake_project *project,
    const char *mode)
{
    char *filename = bake_uninstaller_filename(config, project);
    FILE *result = corto_file_open(filename, mode);
    free(filename);

    return result;
}

int16_t bake_uninstall(
    bake_config *config,
    bake_project *project)
{
    corto_iter it;

    corto_log_push("uninstall");
    corto_trace("begin");

    if (project->kind != BAKE_TOOL) {
        char *projectDir = corto_asprintf(
            "%s/meta/%s", config->rootpath, project->id);

        /* Uninstall files are stored in the project directory, try uninstalling
         * first by removing all files in uninstaller file. */
        if (corto_file_test(projectDir)) {
            char *filename = bake_uninstaller_filename(config, project);
            if (corto_file_iter(filename, &it)) {
                corto_catch(); /* Catch last error */
                corto_warning("missing uninstaller for project '%s'", project->id);
                free(filename);
                goto skip;
            }

            while (corto_iter_hasNext(&it)) {
                char *line = corto_iter_next(&it);
                if (!line || !line[0]) continue; /* Skip empty lines in file */
                if (line[0] == '/') {
                    if (corto_rm(line)) {
                        corto_warning("failed to uninstall '%s' for '%s'",
                            line,
                            project->id);
                    }
                } else {
                    corto_warning(
                        "ignoring '%s' in uninstaller.txt, path should be absolute",
                        line);
                }
            }

            /* Remove uninstaller */
            if (corto_rm(filename)) {
                corto_warning("failed to remove '%s'", filename);
            }

            /* Free project directory if it's empty (no nested packages) */
            if (corto_dir_isEmpty(projectDir)) corto_rm(projectDir);
            free(projectDir);
        }

        /* In the case of an unsuccessful or interrupted uninstallation, some parts
         * may be left behind. Check for lingering directories, and clean up. */

        /* If include directory exists and is empty, clean up */
        char *inc = corto_asprintf(
            "%s/include/%s", config->rootpath, project->id);
        if (corto_file_test(inc) && corto_dir_isEmpty(inc)) corto_rm(inc);
        free(inc);

        /* If etc directory exists and is empty, clean up */
        char *etc = corto_envparse(
            "%s/etc/%s", config->rootpath, project->id);
        if (corto_file_test(etc) && corto_dir_isEmpty(etc)) corto_rm(etc);
        free(etc);
    }

skip:
    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
}

int16_t bake_install(
    bake_config *config,
    bake_project *project)
{
    if (project->kind != BAKE_TOOL) {
        FILE *uninstallFile = bake_uninstaller_open(config, project, "w");
        if (!uninstallFile) {
            goto error;
        }

        /* Copy project.json and file that points back to source */
        if (corto_file_test("project.json")) {
            char *projectDir = corto_envparse(
                "%s/meta/%s", config->rootpath, project->id);

            corto_try (corto_mkdir(projectDir), NULL);

            /* Copy project file */
            if (corto_cp("project.json", projectDir)) {
                free(projectDir);
                goto error;
            }

            /* Copy license file */
            if (corto_file_test("LICENSE")) {
                if (corto_cp("LICENSE", projectDir)) {
                    corto_throw("failed to copy 'LICENSE' to '%s'",
                        projectDir);
                    goto error;
                }
            }

            /* Write project source location to package repository */
            FILE *src_location = fopen(strarg("%s/source.txt", projectDir), "w");
            if (!src_location) {
                corto_throw("failed to write to '%s' for '%s'",
                    strarg("%s/source.txt", projectDir),
                    project->id);
                goto error;
            }
            fprintf(src_location, "%s\n", corto_cwd());
            fclose(src_location);

            /* If project contains dependee JSON, write to dependee.json */
            if (project->dependee_json && strlen(project->dependee_json)) {
                FILE *dependee_config =
                    fopen(strarg("%s/dependee.json", projectDir), "w");
                if (!dependee_config) {
                    corto_throw("failed to write to '%s' for '%s'",
                        strarg("%s/dependee.json", projectDir),
                        project->id);
                    goto error;
                }
                fprintf(dependee_config, "%s\n", project->dependee_json);
                fclose(dependee_config);
                fprintf(uninstallFile, "%s/dependee.json\n", projectDir);
            }

            fprintf(uninstallFile, "%s/project.json\n", projectDir);
            fprintf(uninstallFile, "%s/source.txt\n", projectDir);
            free(projectDir);
        }
        fclose(uninstallFile);
    }

    return 0;
error:
    return -1;
}

int16_t bake_pre(
    bake_config *config,
    bake_project *project)
{
    corto_log_push("pre");
    corto_trace("begin");

    if (project->kind != BAKE_TOOL) {
        FILE *uninstallFile = bake_uninstaller_open(config, project, "a");
        if (!uninstallFile) {
            goto error;
        }

        /* Install files to project-specific locations in package hierarchy */
        if (project->public) {
            corto_iter it = corto_ll_iter(project->includes);
            while (corto_iter_hasNext(&it)) {
                if (bake_install_dir(
                    config,
                    project->id,
                    "include",
                    corto_iter_next(&it),
                    true,
                    uninstallFile))
                {
                    goto error;
                }
            }
        }

        if (bake_install_dir(
            config,
            project->id,
            "etc",
            NULL,
            true,
            uninstallFile))
        {
            goto error;
        }

        if (project->kind == BAKE_PACKAGE) {
            if (bake_install_dir(
                config,
                project->id,
                "lib",
                NULL,
                true,
                uninstallFile))
            {
                goto error;
            }
        }

        /* Install files to BAKE_TARGET directly from 'install' folder */
        if (corto_file_test("install")) {
            if (corto_chdir("install")) {
                goto error;
            }
            if (bake_install_dir(
                config,
                NULL,
                "include",
                NULL,
                true,
                uninstallFile))
            {
                goto error;
            }
            if (bake_install_dir(
                config,
                NULL,
                "lib",
                NULL,
                true,
                uninstallFile))
            {
                goto error;
            }
            if (bake_install_dir(
                config,
                NULL,
                "etc",
                NULL,
                true,
                uninstallFile))
            {
                goto error;
            }
            if (bake_install_dir(
                config,
                NULL,
                "java",
                NULL,
                true,
                uninstallFile))
            {
                goto error;
            }
            if (corto_chdir("..")) {
                goto error;
            }
        }

        fclose(uninstallFile);
    }

    corto_log_pop();
    return 0;
error:
    corto_log_pop();
    return -1;
}

int16_t bake_post(
    bake_project *project,
    char *artefact)
{
    corto_log_push("post");
    corto_trace("begin");

    char *kind, *subdir, *targetDir = NULL;
    bool copy = false;

    targetDir = bake_project_binaryPath(project);
    if (!targetDir) {
        goto error;
    }

    char *artefact_full = corto_asprintf("bin/%s-%s/%s",
        CORTO_PLATFORM_STRING, project->cfg->id, artefact);

    if (!corto_file_test(artefact_full)) {

        /* If artefact cannot be found, try just the artefact name itself */
        if (corto_file_test(artefact)) {
            free(artefact_full);
            artefact_full = corto_strdup(artefact);
        } else {
            corto_throw("cannot find artefact '%s'", artefact_full);
            goto error;
        }
    }

    if (corto_isdir(artefact_full)) {
        corto_throw("specified artefact '%s' is a directory, expecting regular file", artefact_full);
        goto error;
    }

    if (corto_mkdir(targetDir)) {
        goto error;
    }

    char *targetBinary = corto_asprintf("%s/%s", targetDir, artefact);
    if (!corto_file_test(targetBinary) || project->freshly_baked) {
        /* Copy binary */
        if (corto_cp(artefact_full, targetBinary)) {
            goto error;
        }

        /* Ensure that time on the local system has progressed past the point of the
         * file timestamp. If the build is running in a VM, the clock between the
         * client and host could be out of sync temporarily, which can result in
         * strange behavior. */
        char *installedArtefact = corto_asprintf("%s/%s", targetDir, artefact);
        time_t t_artefact = corto_lastmodified(installedArtefact);
        time_t t = time(NULL);
        int i = 0;
        while (t_artefact > time(NULL) && i < 10) {
            corto_sleep(0, 100000000); /* sleep 100msec */
            i ++;
        }
        if (i == 10) {
            corto_warning(
                "clock drift of >1sec between the OS clock and the filesystem detected");
        }
        free(installedArtefact);

        corto_ok("installed '%s/%s'", targetDir, artefact);
    }

    free(targetBinary);
    free(artefact_full);

    corto_log_pop();
    return 0;
error:
    if (targetDir) free(targetDir);
    corto_log_pop();
    return -1;
}
