
/* Copyright (c) 2010-2017 Sander Mertens
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
#include <sys/stat.h>
#include <errno.h>

static int retcode;

typedef struct ut_fileMonitor {
    char *file;
    char *lib;
    time_t mtime;
} ut_fileMonitor;

static
ut_fileMonitor* monitor_new(
    const char *file,
    const char *lib)
{
    ut_fileMonitor *mon = malloc(sizeof(ut_fileMonitor));
    mon->file = ut_strdup(file);
    mon->lib = lib ? ut_strdup(lib) : NULL;
    mon->mtime = 0;

    return mon;
}

static
void monitor_free(
    ut_fileMonitor *mon)
{
    free(mon->file);
    free(mon->lib);
    free(mon);
}

static
void add_changed(
    ut_ll *libs,
    char *lib)
{
    if (*libs && lib) {
        ut_iter iter = ut_ll_iter(*libs);
        while (ut_iter_hasNext(&iter)) {
            char *l = ut_iter_next(&iter);
            if (!strcmp(l, lib)) {
                return;
            }
        }
    }

    if (!*libs) {
        *libs = ut_ll_new();
    }
    ut_ll_append(*libs, lib);
}

static
ut_ll get_modified(
    ut_ll files,
    ut_ll changed)
{
    int32_t i = 0;
    ut_ll libs = NULL;

    if (changed) {
        ut_ll_free(changed);
        changed = NULL;
    }

    if (files) {
        ut_iter iter = ut_ll_iter(files);
        while (ut_iter_hasNext(&iter)) {
            struct stat attr;
            ut_fileMonitor *mon = ut_iter_next(&iter);

            if (stat(mon->file, &attr) < 0) {
                ut_error("failed to stat '%s' (%s)\n",
                mon->file, strerror(errno));
            }

            if (mon->mtime) {
                if (mon->mtime != attr.st_mtime) {
                    ut_log("#[grey]detected change in file '%s'\n", mon->file);
                    add_changed(&libs, mon->lib);
                } else {
                    ut_debug("#[grey] no change in file '%s'",
                        mon->file);
                }
            } else {
                ut_debug("#[grey] ignoring file '%s'", mon->file);
            }
            mon->mtime = attr.st_mtime;

            i++;
        }
    }

    return libs;
}

static
int compare_monitor(
    void *o1,
    void *o2)
{
    ut_fileMonitor
        *m1 = o1,
        *m2 = o2;

    if (!strcmp(m1->file, m2->file)) {
        return 0;
    }
    return 1;
}

static
bool filter_file(
    const char *file)
{
    char *ext = strrchr(file, '.');

    if (!ut_isdir(file) &&
        strncmp(file, "test", 4) &&
        strncmp(file, "bin", 3) &&
        strcmp(file, "include/bake_config.h") &&
        !(file[0] == '.') &&
        ext &&
        strcmp(ext, ".so"))
    {
        return true;
    }

    return false;
}

static
ut_ll gather_files(
    const char *project_dir,
    const char *app_bin,
    ut_ll old_files,
    bool *files_removed)
{
    ut_ll result = ut_ll_new();
    ut_iter it;

    if (ut_dir_iter(project_dir, "//", &it))  {
        goto error;
    }

    /* Find files that are not binaries, directories or build artefacts */
    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        if (filter_file(file)) {
            ut_fileMonitor *m = monitor_new(
                strarg("%s"UT_OS_PS"%s", project_dir, file), app_bin);
            ut_ll_append(result, m);
        }
    }

    /* Update new filelist with timestamps from old list */
    if (old_files) {
        ut_fileMonitor *m;
        while ((m = ut_ll_takeFirst(old_files))) {
            ut_fileMonitor *m_new = ut_ll_find(
                result, compare_monitor, m);
            if (m_new) {
                m_new->mtime = m->mtime;
            } else {
                if (files_removed) {
                    *files_removed = true;
                }
            }
            monitor_free(m);
        }

        ut_ll_free(old_files);
    }

    return result;
error:
    ut_ll_free(result);
    return NULL;
}

static
ut_ll wait_for_changes(
    ut_proc pid,
    const char *project_dir,
    const char *app_bin,
    ut_ll changed)
{
    int32_t i = 0;

    /* Collect initial set of files in project directory */
    ut_ll files = gather_files(project_dir, app_bin, NULL, NULL);

    /* Get initial timestamps for all files */
    get_modified(files, NULL);

    if (changed) {
        ut_ll_free(changed);
        changed = NULL;
    }

    do {
        ut_sleep(0, 50000000);

        /* Check if process is still running every 50ms */
        if (pid) {
            if ((retcode = ut_proc_check(pid, NULL))) {
                break;
            }
        }

        i++;

        /* Check files every second */
        if (!(i % 20)) {
            bool files_removed = false;

            /* Refresh files, new files will show up with timestamp 0 */
            files = gather_files(
                project_dir, app_bin, files, &files_removed);

            /* Check for changes, new files will show up as changed */
            changed = get_modified(files, changed);

            /* If files have been removed but no files have changed, create an
             * empty list, so it will still trigger a rebuild */
            if (!changed && files_removed) {
                changed = ut_ll_new();
            }
        } else {
            continue;
        }
    }while (!changed || (pid && retcode));

    return changed;
}

static
int build_project(
    const char *path)
{
    int8_t procResult = 0;

    /* Build */
    ut_proc pid = ut_proc_run(BAKE_EXEC, (const char*[]){
        BAKE_EXEC,
        (char*)path,
        "-r",
         NULL
    });

    if (!pid) {
        ut_throw("failed to invoke bake %s -r", path);
        goto error;
    }

    if (ut_proc_wait(pid, &procResult) || procResult) {
        ut_throw("failed to build '%s'", path);

        /* Ensure that the binary folder is gone */
        ut_rm(strarg("%s"UT_OS_PS"bin", path));
        goto error;
    }

    return 0;
error:
    return -1;
}

static
ut_proc run_exec(
    const char *exec,
    const char *app_id,
    bool is_package,
    int argc,
    const char *argv[])
{
    if (!is_package) {
        const char *local_argv[1024] = {(char*)exec};
        int i = 1;

        if (argc) {
            if (argv) {
                for (i = 0; i < argc; i ++) {
                    local_argv[i] = argv[i - 1];
                }
            }
        }

        local_argv[i] = NULL;

        ut_proc pid = ut_proc_run(exec, local_argv);

        bake_message(UT_LOG, "run", "#[green]application#[reset] %s [%u] '%s'", app_id, pid, exec);

        return pid;
    } else {
        /* TODO */
        return 0;
    }
}

static
int16_t run_interactive(
    const char *project_dir,
    const char *app_bin,
    const char *app_id,
    bool is_package,
    int argc,
    const char *argv[])
{
    ut_proc pid = 0, last_pid = 0;
    ut_ll changed = NULL;
    uint32_t retries = 0;
    int32_t rebuild = 0;

    while (true) {
        if (!retries || changed) {
            if (changed) {
                printf("\n");
            }

            /* Build the project */
            build_project(project_dir);
            rebuild++;
        }

        if (pid && rebuild) {
            /* Send interrupt signal to process */
            if (ut_proc_kill(pid, UT_SIGINT)) {
                /* Wait until process has exited */
                ut_proc_wait(pid, NULL);
            }
            rebuild = 0;
            pid = 0;
        }

        /* Test whether the app exists, then start it */
        if (ut_file_test(app_bin)) {
            if (retries && !pid) {
                bake_message(UT_LOG, "restart", "#[green]application#[reset] %s (%dx)", app_id, retries);
            }

            if (!pid) {
                /* Run process, ensure process binary is first argument */
                {
                    pid = run_exec(app_bin, app_id, is_package, argc, argv);
                    last_pid = pid;
                }
            }

            /* Wait until either source changes, or executable finishes */
            changed = wait_for_changes(
                pid, project_dir, app_bin, changed);

            /* Set pid to 0 if process has exited */
            if (retcode) {
                pid = 0;
            }
        } else {
            ut_error(
                "build failed! (press Ctrl-C to exit"
                " or change files to rebuild)\n");

            /* Wait for changed before trying again */
            changed = wait_for_changes(
                0, project_dir, app_bin, changed);
        }

        /* If the process segfaults, wait for changes and rebuild */
        if (retcode) {
            if ((retcode == 11) || (retcode == 6)) {
                ut_error("segmentation fault, fix your code!");
            } else {
                bake_message(UT_LOG, "done", "#[green]application#[reset] %s [%d] '%s'", app_id, last_pid, app_bin);
                bake_message(UT_LOG, "", "press Ctrl-C to exit or change files to restart", app_id, last_pid, app_bin);
            }

            changed = wait_for_changes(
                0, project_dir, app_bin, changed);
            retcode = 0;
            pid = 0;
        }

        retries++;
    }

    if (retcode != -1) {
        ut_error("process stopped with error (%d)", retcode);
    }

    return 0;
}

static
bool is_valid_project(
    const char *project)
{
    /* Test if directory exists */
    if (ut_file_test(project) != 1) {
        ut_throw("project directory '%s' does not exist", project);
        goto error;
    }

    if (!ut_isdir(project)) {
        ut_throw("file '%s' is not a directory", project);
        goto error;
    }

    char *project_json = ut_asprintf("%s"UT_OS_PS"project.json", project);
    if (ut_file_test(project_json) != 1) {
        ut_throw("cannot find or load file '%s'", project_json);
        goto error;
    }

    return true;
error:
    return false;
}

static
char *name_from_id(
    const char *id)
{
    if (id[0] == UT_OS_PS[0]) {
        id ++;
    }

    char *result = ut_strdup(id);
    char *ptr, ch;

    for (ptr = result; (ch = *ptr); ptr ++) {
        if (ch == '.') {
            *ptr = '_';
        }
    }

    return result;
}

static
void str_strip(
    char *str)
{
    int len = strlen(str);
    char *ptr = &str[len - 1];

    while (isspace(*ptr)) {
        *ptr = '\0';
        ptr --;
    }
}

int bake_run(
    bake_config *config,
    const char *app_id,
    bool interactive,
    int argc,
    const char *argv[])
{
    const char *app_name = NULL; /* Last element of application id */
    const char *app_bin = NULL; /* Application executable */
    bool is_package = false;
    const char *package_path = NULL; /* App location in bake environment */
    char *project_dir = NULL; /* Project directory containing sources */

    if (app_id && strcmp(app_id, ".")) {
        app_name = name_from_id(app_id);

        /* First check if passed argument is a valid directory */
        if (is_valid_project(app_id)) {
            project_dir = (char*)app_id;
        } else {
            ut_catch();
        }

        /* If project is not found, lookup in package repositories */
        if (!project_dir) {
            package_path = ut_locate(app_id, NULL, UT_LOCATE_PROJECT);
            if (!package_path) {
                ut_throw("failed to find application '%s'", app_id);
                goto error;
            }

            /* Check if the package repository contains a link back to the
             * location of the project. */
            char *source_file = ut_asprintf("%s"UT_OS_PS"source.txt", package_path);
            if (ut_file_test(source_file) == 1) {
                project_dir = ut_file_load(source_file);
                if (!project_dir) {
                    ut_throw("file '%s' does not contain project path", source_file);
                    goto error;
                }

                str_strip(project_dir); /* Remove trailing whitespace */

                if (!is_valid_project(project_dir)) {
                    /* Directory pointed to is not a valid bake project. Don't
                     * give up yet, maybe the project got moved. */
                    ut_warning(
                        "package '%s' points to project folder '%s'"
                        " which is not a valid bake project. Attempting"
                        " to run existing binary, which may be outdated.",
                        app_id, project_dir);
                    project_dir = NULL;
                }
            } else {
                ut_warning("could not find a source directory for '%s'", app_id);
            }
        }
        /* Test if current directory is a valid project */
    } else if (is_valid_project(".")) {
        project_dir = ".";
    } else {
        ut_throw("current directory is not a valid bake project");
        goto error;
    }

    if (project_dir) {
        bake_project *project = bake_project_new(project_dir, config);
        if (!project) {
            ut_throw("failed to load project in directory '%s'", project_dir);
            goto error;
        }

        ut_try( bake_project_init(config, project), NULL);

        ut_try( bake_project_parse_driver_config(config, project), NULL);

        app_id = project->id;
        app_name = project->artefact;
        is_package = project->type == BAKE_PACKAGE;

        /* If project is found, point to executable in project bin */
        app_bin = ut_asprintf("%s"UT_OS_PS"bin"UT_OS_PS"%s-%s"UT_OS_PS"%s",
            project_dir, UT_PLATFORM_STRING, config->configuration, app_name);
    } else {
        /* If project directory is not found, locate the binary in the
         * package repository. This only allows for running the
         * application, not for interactive building */
        app_bin = ut_locate(app_id, NULL, UT_LOCATE_BIN);
        if (!app_bin) {
            /* We have no project dir and no executable. No idea how to
             * build this project! */
            ut_throw("executable not found for '%s'", app_id);
            goto error;
        }

        if (ut_locate(app_id, NULL, UT_LOCATE_LIB)) {
            is_package = true;
        }
    }

    if (interactive) {
        if (!project_dir) {
            ut_warning(
              "don't know location of sourcefiles, interactive mode disabled");
            interactive = false;
        }
    }

    ut_ok("starting app '%s'", app_id);
    ut_ok("  executable = '%s'", app_bin);
    ut_ok("  project path = '%s'", project_dir);
    ut_ok("  project kind = '%s'", is_package ? "package" : "application");
    ut_ok("  interactive = '%s'", interactive ? "true" : "false");

    if (interactive) {
        /* Run process & monitor source for changes */
        if (run_interactive(project_dir, app_bin, app_id, is_package, argc, argv)) {
            goto error;
        }
    } else {
        /* Just run process */
        ut_proc pid;
        ut_trace("starting process '%s'", app_bin);

        if (project_dir) {
            ut_try( build_project(project_dir), "build failed, cannot run");
        }

        if (argc) {
            pid = run_exec(app_bin, app_id, is_package, argc, argv);
        } else {
            pid = run_exec(
              app_bin, app_id, is_package, 1, (const char*[]){
                  (char*)app_bin, NULL});
        }
        if (!pid) {
            ut_throw("failed to start process '%s'", app_bin);
            goto error;
        }

        ut_trace("waiting for process '%s'", app_bin);
        int8_t result = 0, sig = 0;
        if ((sig = ut_proc_wait(pid, &result)) || result) {
            if (sig > 0) {
                ut_throw("process crashed (%d)", sig);
                ut_raise();

                ut_strbuf cmd_args = UT_STRBUF_INIT;
                int i;
                for (i = 1; i < argc; i ++) {
                    ut_strbuf_appendstr(&cmd_args, argv[i]);
                }

                char *args = ut_strbuf_get(&cmd_args);

                printf("\n");
                printf("to debug your application, do:\n");
                ut_log("  export $(bake env)\n");
                ut_log("  %s %s\n", app_bin, args ? args : "");
                printf("\n");

                free(args);
            }

            goto error;
        }

        bake_message(UT_LOG, "done", "#[green]application#[reset] %s [%u] '%s'", 
            app_id, pid, app_bin);
    }

    return 0;
error:
    return -1;
}
