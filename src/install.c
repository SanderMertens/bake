/* Copyright (c) 2010-2017 the corto developers
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

#include <include/bake.h>

static
int16_t bake_install_dir(
    char *id,
    char *projectPath,
    char *dir,
    char *platform,
    bool softlink)
{
    corto_dirstack stack = NULL;
    corto_iter it;

    char *source;
    if (!platform) {
        source = corto_asprintf("%s/%s", 
            projectPath, 
            dir);
    } else {
        source = strdup(platform);
    }

    /* If source path does not exist, nothing needs to be copied. */
    if (!corto_file_test(source)) {
        free(source);
        return 0;
    }

    char *target = corto_envparse("$CORTO_TARGET/%s/corto/$CORTO_VERSION/%s", dir, id);
    if (!target) {
        goto error;
    }

    if (!(stack = corto_dirstack_push(stack, source))) goto error;

    if (corto_dir_iter(".", &it)) goto error;

    while (corto_iter_hasNext(&it)) {
        char *file = corto_iter_next(&it);

        if (corto_isdir(file)) {
            if (!strnicmp(file, strlen("linux-"), "linux-") ||
                !strnicmp(file, strlen("darwin-"), "darwin-") ||
                !strnicmp(file, strlen("windows-"), "windows-"))
            {
                if (corto_os_match(file)) {
                    if (bake_install_dir(id, projectPath, dir, file, softlink)) {
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
                if (bake_install_dir(id, projectPath, dir, file, softlink)) {
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
        free(dst);
    }

    if (corto_dirstack_pop(stack)) goto error;

    free(target);
    free(source);

    return 0;
error:
    return -1;
}


int16_t bake_preinstall(
    bake_project *project)
{

    if (!project->local) {
        if (bake_install_dir(project->id, project->path, "include", NULL, true)) {
            goto error;
        }
    }

    if (bake_install_dir(project->id, project->path, "etc", NULL, true)) {
        goto error;
    }    

    if (project->kind == BAKE_LIBRARY) {
        if (bake_install_dir(project->id, project->path, "lib", NULL, true)) {
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

int16_t bake_postinstall(
    char *id, 
    char *artefact)
{
    return 0;
}

int16_t bake_uninstall(
    char *id)
{
    return 0;
}