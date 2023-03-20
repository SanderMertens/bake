/* Copyright (c) 2010-2019 the corto developers
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
    const char *id,
    const char *source_path,
    const char *dir,
    const char *subdir,
    const char *target,
    bool softlink)
{
    ut_dirstack stack = NULL;
    ut_iter it;

    char *source;
    if (!subdir) {
        source = ut_asprintf("%s"UT_OS_PS"%s", source_path, dir);
    } else {
        source = ut_asprintf("%s"UT_OS_PS"%s", source_path, subdir);
    }

    /* If source path does not exist, nothing needs to be copied. */
    if (!ut_file_test(source)) {
        free(source);
        return 0;
    }

    if (ut_dir_iter(source, NULL, &it)) goto error;

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *filepath = ut_asprintf("%s"UT_OS_PS"%s", source, file);

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
        char *dst = ut_asprintf("%s"UT_OS_PS"%s", target, file);
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
    const char *env,
    const char *id,
    const char *source_path,
    const char *dir,
    const char *subdir,
    bool softlink)
{
    char *target;

    if (id) {
        target = ut_envparse("%s"UT_OS_PS"%s"UT_OS_PS"%s", env, dir, id);
    } else {
        target = ut_envparse("%s"UT_OS_PS"%s", env, dir);
    }

    if (bake_install_dir_for_target(
        id, source_path, dir, subdir, target, softlink))
    {
        goto error;
    }

    return 0;
error:
    return -1;
}

int16_t bake_uninstall_from_cfg(
    bake_config *config,
    const char *cfg,
    bake_project *project,
    bool uninstall)
{
    /* Try removing all possible artefacts, in case project type changed */
    ut_rm( strarg("%s"UT_OS_PS"%s%s%s", config->lib, UT_LIB_PREFIX, project->id_underscore, UT_SHARED_LIB_EXT));
    ut_rm( strarg("%s"UT_OS_PS"%s%s%s", config->lib, UT_LIB_PREFIX, project->id_underscore, UT_STATIC_LIB_EXT));
    ut_rm( strarg("%s"UT_OS_PS"%s%s", config->bin, project->id_underscore, UT_EXECUTABLE_EXT));
    ut_rm( strarg("%s"UT_OS_PS"%s%s", config->target, project->id_underscore, UT_EXECUTABLE_EXT));

    return 0;
error:
    return -1;
}

int16_t bake_install_clear(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    bool uninstall)
{
    ut_log_push("uninstall");

    if (uninstall) {
        /* Clean metadata from BAKE_HOME */
        if (ut_rm(strarg("%s"UT_OS_PS"%s", UT_META_PATH, project_id))) {
            /* Raise error, but don't abort. Try to cleanup as much as possible. */
            ut_raise();
        }

        /* Only delete binaries when uninstalling */
        if (ut_file_test(UT_PLATFORM_PATH)) {
            ut_iter it;
            if (ut_dir_iter(UT_PLATFORM_PATH, NULL, &it)) {
                ut_raise(); /* ditto */
            } else {
                while (ut_iter_hasNext(&it)) {
                    char *cfg = ut_iter_next(&it);
                    bake_uninstall_from_cfg(config, cfg, project, uninstall);
                }
            }
        }
    }

    /* Only clean from current configuration */
    ut_rm(strarg("%s"UT_OS_PS"%s", UT_ETC_PATH, project_id));
    ut_rm(strarg("%s"UT_OS_PS"%s.dir", UT_INCLUDE_PATH, project_id));

    /* Remove link to main include file */
    char *link_name = ut_asprintf("%s"UT_OS_PS"%s", UT_INCLUDE_PATH, project_id);
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
    if (!project_id) {
        ut_throw("no project id specified for uninstaller");
        goto error;
    }

    const char *project_dir = ut_locate(project_id, NULL, UT_LOCATE_PROJECT);
    if (!project_dir) {
        ut_throw("project '%s' not found", project_id);

        if (ut_locate(project_id, NULL, UT_LOCATE_TEMPLATE)) {
            ut_info("Did you mean:");
            ut_info("  bake uninstall %s --template\n", project_id);
        }

        goto error;
    }

    bake_project *project = bake_project_new(project_dir, config);
    if (!project) {
        /* Silence error. Cleanup as much as possible */
        ut_catch();
    }

    if (bake_install_clear(config, project, project_id, true)) {
        ut_raise();
    }

    ut_log("#[grey]uninstalled #[normal]'%s'\n", project->id);
error:
    return -1;
}

int16_t bake_install_uninstall_template(
    bake_config *config,
    const char *project_id)
{
    if (!project_id) {
        ut_throw("no project id specified for uninstaller");
        goto error;
    }

    const char *project_dir = ut_locate(project_id, NULL, UT_LOCATE_TEMPLATE);
    if (!project_dir) {
        ut_throw("project '%s' not found", project_id);
        goto error;
    }

    ut_try( ut_rm(project_dir), NULL);

    ut_log("#[grey]uninstalled template #[normal]'%s'\n", project_id);
error:
    return -1;
}

int16_t bake_install_metadata(
    bake_config *config,
    bake_project *project)
{
    if (project->type != BAKE_TOOL) {
        /* Copy project.json and file that points back to source */
        char *project_json = ut_asprintf("%s"UT_OS_PS"project.json", project->path);
        if (ut_file_test(project_json)) {
            char *projectDir = ut_envparse(
                "%s"UT_OS_PS"%s", UT_META_PATH, project->id);

            ut_try (ut_mkdir(projectDir), NULL);

            /* Copy project file */
            if (ut_cp(project_json, projectDir)) {
                free(projectDir);
                goto error;
            }

            free(project_json);
            char *license = ut_asprintf("%s"UT_OS_PS"LICENSE", project->path);

            /* Copy license file */
            if (ut_file_test(license)) {
                if (ut_cp(license, projectDir)) {
                    ut_throw("failed to copy '%s' to '%s'",
                        license, projectDir);
                    goto error;
                }
            }

            /* Write project source location to package repository */
            FILE *src_location = fopen(strarg("%s"UT_OS_PS"source.txt", projectDir), "w");
            if (!src_location) {
                ut_throw("failed to write to '%s' for '%s'",
                    strarg("%s"UT_OS_PS"source.txt", projectDir),
                    project->id);
                goto error;
            }

            fprintf(src_location, "%s\n", project->fullpath);
            fclose(src_location);

            /* If project contains dependee JSON, write to dependee.json */
            if (project->dependee_json && strlen(project->dependee_json)) {
                FILE *dependee_config =
                    fopen(strarg("%s"UT_OS_PS"dependee.json", projectDir), "w");
                if (!dependee_config) {
                    ut_throw("failed to write to '%s' for '%s'",
                        strarg("%s"UT_OS_PS"dependee.json", projectDir),
                        project->id);
                    goto error;
                }
                fprintf(dependee_config, "%s\n", project->dependee_json);
                fclose(dependee_config);
                ut_trace("#[cyan]write %s"UT_OS_PS"dependee.json", projectDir);
            }
            free(projectDir);
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_check_includes(
    bake_project *project,
    const char *path)
{
    ut_iter it;
    int16_t result = 0;
    
    if (ut_dir_iter(path, NULL, &it)) {
        ut_catch();
        return 0;
    }

    char *short_header = ut_asprintf("%s.h", project->id_base);
    char *header = ut_asprintf("%s.h", project->id_underscore);

    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *file_path = ut_asprintf("%s/%s", path, file);

        /* Clear bake_config.h include root (left by old bake install) */
        if (!strcmp(file, "bake_config.h")) {            
            ut_rm(file_path);
        }

        /* Rename header with project base name to header with full path */
        else if (!strcmp(file, short_header)) {
            if (strcmp(header, short_header)) {
                char *header_path = ut_asprintf("%s/include/%s", project->path, header);
                ut_rename(file_path, header_path);
                free(header_path);
            }
        }
        
        /* If a directory is found, check if it has the right name */
        else if (ut_isdir(file_path)) {
            if (file[0] != '.' && strcmp(file, project->id_dash)) {
                ut_error(
                    "project '%s' has directory with illegal name ('include/%s'), expected 'include/%s'",
                    project->id, file, project->id_dash);
                result = -1;
            }
        }

        /* If filename is different from main header, throw error */
        else if (file[0] != '.' && strcmp(file, header)) {
            ut_error("project '%s' has file with illegal name '%s' in include directory, expected '%s.h'",
                project->id, file, project->id_underscore);
            result = -1;
        }

        free(file_path);
    }

    free(header);

    return result;
error:
    return -1;
}

int16_t bake_install_prebuild(
    bake_config *config,
    bake_project *project)
{
    if (project->type != BAKE_TOOL) {

        char *own_includes = ut_asprintf("%s/include", project->path);

        if (bake_check_includes(project, own_includes)) {
            free(own_includes);
            goto error;
        }

        free(own_includes);

        /* Install files to project-specific locations in $BAKE_TARGET */
        ut_iter it = ut_ll_iter(project->includes);
        while (ut_iter_hasNext(&it)) {
            char *include_path = ut_iter_next(&it);

            if (bake_install_dir(
                config->home,
                ".",
                project->path,
                "include",
                include_path,
                true))
            {
                goto error;
            }
        }

        if (bake_install_dir(
            config->target, project->id, project->path, "etc", NULL, true))
        {
            goto error;
        }

        if (project->type == BAKE_PACKAGE) {
            if (bake_install_dir(
                config->target, project->id, project->path, "lib", NULL, true))
            {
                goto error;
            }
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_install_template(
    bake_config *cfg,
    bake_project *project)
{
    ut_try( ut_mkdir(UT_TEMPLATE_PATH), NULL);

    char *project_template = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS"%s", 
        UT_TEMPLATE_PATH, project->id, project->language);

    ut_try( ut_rm(project_template), NULL);
    ut_try( ut_symlink(project->path, project_template), NULL);

    free(project_template);

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

    if (project->public) {
        if (project->type == BAKE_PACKAGE) {
            if (project->bake_extension) {
                targetDir = UT_HOME_LIB_PATH;
            } else {
                targetDir = UT_LIB_PATH;
            }
        } else {
            targetDir = UT_BIN_PATH;
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

        char *targetBinary = ut_asprintf("%s"UT_OS_PS"%s", targetDir, project->artefact);

        if (!ut_file_test(targetBinary) || project->changed || !project->language) {

            /* Copy all files in project bin path to bake environment */
            ut_iter it;
            ut_try( ut_dir_iter(project->artefact_path, NULL, &it), NULL);

            while (ut_iter_hasNext(&it)) {
                char *file = ut_iter_next(&it);
                char *src = ut_asprintf("%s"UT_OS_PS"%s", project->artefact_path, file);
                char *dst = ut_asprintf("%s"UT_OS_PS"%s", targetDir, file);
                ut_try (ut_cp(src, dst), 
                    "failed to install binary '%s' to bake environment", src);

                time_t t_artefact = ut_lastmodified(dst);
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

                free(src);
                free(dst);
            }
        }

        free(targetBinary);
    }

    /* If artefact is a webasm file, also copy it to etc directory */
    char *js_ext = strrchr(project->artefact, '.');
    if (js_ext && !strcmp(js_ext, ".js")) {
        if (ut_mkdir("etc")) {
            goto error;
        }

        ut_cp(project->artefact_file, "etc");

        char *wasm_file = malloc(strlen(project->artefact_file) + 1 + 2);
        strcpy(wasm_file, project->artefact_file);
    
        char *wasm_ext = strrchr(wasm_file, '.');
        strcpy(wasm_ext, ".wasm");

        ut_cp(wasm_file, "etc");
        free(wasm_file);
    }

    return 0;
error:
    return -1;
}
