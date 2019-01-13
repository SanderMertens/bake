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

#include "../include/util.h"

static ut_ll fileHandlers = NULL;
static ut_ll loadedAdmin = NULL;
static ut_ll libraries = NULL;

/* Static variables set during initialization that contain paths to packages */
static char *UT_LOAD_TARGET_PATH, *UT_LOAD_HOME_PATH;
static char *UT_LOAD_TARGET_META_PATH, *UT_LOAD_HOME_META_PATH;
static bool UT_LOAD_TARGET_OTHER_THAN_HOME = false;

/* Lock protecting the package administration */
extern ut_mutex_s UT_LOAD_LOCK;

struct ut_loaded {
    char* id; /* package id or file */

    char *env; /* Environment in which the package is installed */
    char *lib; /* Path to library (if available) */
    char *static_lib; /* Path to static library (if available) */
    char *app; /* Path to executable (if available) */
    char *bin; /* Path to binary (if available) */
    char *etc; /* Path to project etc (if available) */
    char *include; /* Path to project include (if available) */
    char *project; /* Path to project lib. Always available if valid project */
    bool tried_binary; /* Set to true if already tried loading the bin path */
    bool tried_locating; /* Set to true if already tried locating package */

    /* Members used by package loader to indicate current loading status */
    ut_thread loading; /* current thread loading package */
    int16_t loaded; /* -1 error, 0 not loaded, 1 loaded */

    ut_dl library; /* pointer to library */
};

struct ut_fileHandler {
    char* ext;
    ut_load_cb load;
    void* userData;
};

static
int ut_load_fromDl(
    ut_dl dl,
    const char *fileName,
    int argc,
    char *argv[]);

/* Lookup loaded library by name */
static
struct ut_loaded* ut_loaded_find(
    const char* name)
{
    if (loadedAdmin) {
        ut_iter iter = ut_ll_iter(loadedAdmin);
        struct ut_loaded *lib;
        const char *nameptr = name;
        if (nameptr[0] == PATH_SEPARATOR_C) nameptr ++;
        if (nameptr[0] == '.') nameptr ++;

        while (ut_iter_hasNext(&iter)) {
            lib = ut_iter_next(&iter);
            const char *idptr = lib->id;
            if (idptr[0] == PATH_SEPARATOR_C) idptr ++;
            if (idptr[0] == '.') idptr ++;
            if (!idcmp(nameptr, idptr)) {
                return lib;
            }
        }
    }

    return NULL;
}

/* Add file */
static
struct ut_loaded* ut_loaded_add(
    const char* library)
{
    struct ut_loaded *lib = ut_calloc(sizeof(struct ut_loaded));
    lib->id = ut_strdup(library);
    lib->loading = ut_thread_self();
    if (!loadedAdmin) {
        loadedAdmin = ut_ll_new();
    }
    ut_ll_insert(loadedAdmin, lib);
    return lib;
}

/* Lookup file handler action */
static
int ut_lookupExtWalk(
    struct ut_fileHandler* h,
    struct ut_fileHandler** data)
{
    if (!strcmp(h->ext, (*data)->ext)) {
        *data = h;
        return 0;
    }
    return 1;
}

/* Lookup file handler */
static
struct ut_fileHandler* ut_lookupExt(
    char* ext)
{
    struct ut_fileHandler dummy, *dummy_p;

    dummy.ext = ext;
    dummy_p = &dummy;

    /* Walk handlers */
    if (fileHandlers) {
        ut_ll_walk(
            fileHandlers, (ut_elementWalk_cb)ut_lookupExtWalk, &dummy_p);
    }

    if (dummy_p == &dummy) {
        dummy_p = NULL;
    }

    return dummy_p;
}

/* Load from a dynamic library */
static
int ut_load_fromDl(
    ut_dl dl,
    const char *fileName,
    int argc,
    char *argv[])
{
    int (*proc)(int argc, char* argv[]);

    ut_assert(fileName != NULL, "NULL passed to ut_load_fromDl");

    ut_debug("invoke cortomain of '%s' with %d arguments", fileName, argc);

    if (!argv) {
        argv = (char*[]){(char*)fileName, NULL};
        argc = 1;
    }

    int ret = 0;

    proc = (int(*)(int,char*[]))ut_dl_proc(dl, "cortoinit");
    if (proc) {
        if ((ret = proc(argc, argv))) {
            if (!ut_raised()) {
                ut_throw("cortoinit of '%s' returned %d", fileName, ret);
            }
            goto error;
        }
    }

    proc = (int(*)(int,char*[]))ut_dl_proc(dl, "cortomain");
    if (proc) {
        if ((ret = proc(argc, argv))) {
            if (!ut_raised()) {
                ut_throw("cortomain of '%s' returned %d", fileName, ret);
            }
            goto error;
        }
    }

    /* Add library to libraries list */
    if (ut_mutex_lock (&UT_LOAD_LOCK)) {
        ut_throw(NULL);
        goto error;
    }
    if (!libraries || !ut_ll_hasObject(libraries, dl)) {
        if (!libraries) {
            libraries = ut_ll_new();
        }

        ut_ll_insert(libraries, dl);
        ut_debug("loaded '%s'", fileName);
    }
    if (ut_mutex_unlock (&UT_LOAD_LOCK)) {
        ut_throw(NULL);
        goto error;
    }

    return 0;
error:
    return -1;
}

/*
 * Load a Corto library
 * Receives the absolute path to the lib<name>.so file.
 */
static
int ut_load_library(
    char* fileName,
    ut_dl *dl_out,
    int argc,
    char* argv[])
{
    ut_dl dl = NULL;

    ut_assert(fileName != NULL, "NULL passed to ut_load_library");
    ut_catch();

    dl = ut_dl_open(fileName);

    if (!dl) {
        ut_throw("%s: %s", fileName, ut_dl_error());
        goto error;
    }

    if (ut_load_fromDl(dl, fileName, argc, argv)) {
        goto error;
    }

    if (dl_out) {
        *dl_out = dl;
    }

    return 0;
error:
    if (dl) ut_dl_close(dl);
    return -1;
}

/*
 * An adapter on top of ut_load_library to fit the ut_load_cb signature.
 */
int ut_load_libraryAction(
    char* file,
    int argc,
    char* argv[],
    void *data)
{
    UT_UNUSED(data);
    return ut_load_library(file, NULL, argc, argv);
}

static
int16_t ut_test_package(
    const char *env,
    const char *package,
    time_t *t_out)
{
    int16_t result = 0;
    char *path;
    path = ut_asprintf("%s%cmeta%c%s%cproject.json", env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, package, PATH_SEPARATOR_C);

    if ((result = ut_file_test(path)) == 1) {
        ut_debug("found '%s'", path);
        *t_out = ut_lastmodified(path);
    } else {
        if (result != -1) {
            ut_debug("file '%s' not found", path);
        }
    }

    free(path);

    return result;
}

static
int16_t ut_locate_package(
    const char* package,
    const char **env)
{
    time_t t_target = 0, t_home = 0;
    int16_t ret = 0;

    ut_assert(UT_LOAD_TARGET_PATH != NULL, "UT_LOAD_TARGET_PATH is not set");
    ut_assert(UT_LOAD_HOME_PATH != NULL, "UT_LOAD_HOME_PATH is not set");

    ut_log_push(strarg("locate:%s", package));

    /* Look for project in BAKE_TARGET first */
    ut_debug("try to find '%s' in $BAKE_TARGET ('%s')", package, UT_LOAD_TARGET_PATH);
    if ((ret = ut_test_package(UT_LOAD_TARGET_PATH, package, &t_target)) != -1) {
        if (ret == 1) {
            *env = UT_LOAD_TARGET_PATH;
        }

        /* If no errors and BAKE_TARGET != BAKE_HOME look in BAKE_HOME */
        if (UT_LOAD_TARGET_OTHER_THAN_HOME) {
            ut_debug("try to find '%s' in $BAKE_HOME ('%s')", package, UT_LOAD_HOME_PATH);
            if ((ut_test_package(UT_LOAD_HOME_PATH, package, &t_home) == 1)) {
                if (!ret || t_target < t_home) {
                    *env = UT_LOAD_HOME_PATH;
                }
            }
        } else {
            ut_debug("skip looking in $BAKE_HOME (same as $BAKE_TARGET)");
        }
    }

    ut_log_pop();
    return ret;
}

static
int16_t ut_locate_binary(
    const char *id,
    struct ut_loaded *loaded)
{
    int16_t ret = 0;

    char *pkg_underscore = ut_strdup(id);
    char *ptr, ch;
    for (ptr = pkg_underscore; (ch = *ptr); ptr ++) {
        if (ch == PATH_SEPARATOR_C || ch == '.') {
            *ptr = '_';
        }
    }

    char *bin = NULL;

#ifdef UT_MACOS
    /* Test for .dylib library */
    bin = ut_asprintf("%s/lib/lib%s.dylib", loaded->env, pkg_underscore);
    if ((ret = ut_file_test(bin)) == 1) {
        /* Library found */
        loaded->lib = bin;
        loaded->bin = bin;
    } else {
        if (ret != 0) {
            ut_throw("could not access file '%s'", bin);
            free(bin);
            goto error;
        } else {
            free(bin);
        }
    }
#endif

    /* Test for .so library */
    if (ret == 0) {
        bin = ut_asprintf("%s%clib%clib%s.so", loaded->env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, pkg_underscore);
        if ((ret = ut_file_test(bin)) == 1) {
            /* Library found */
            loaded->lib = bin;
            loaded->bin = bin;
        } else {
            if (ret != 0) {
                ut_throw("could not access file '%s'", bin);
                free(bin);
                goto error;
            } else {
                free(bin);
            }
        }
    }

    /* Test for .a library */
    if (ret == 0) {
        bin = ut_asprintf("%s/lib/lib%s.a", loaded->env, pkg_underscore);
        if ((ret = ut_file_test(bin)) == 1) {
            /* Library found */
            loaded->static_lib = bin;
            loaded->bin = bin;
        } else {
            if (ret != 0) {
                ut_throw("could not access file '%s'", bin);
                free(bin);
                goto error;
            } else {
                free(bin);
            }
        }
    }

    /* Test for executable */
    if (ret == 0) {
        bin = ut_asprintf("%s%cbin%c%s", loaded->env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, pkg_underscore);
        if ((ret = ut_file_test(bin)) == 1) {
            /* Executable found */
            loaded->app = bin;
            loaded->bin = bin;
        } else {
            if (ret != 0) {
                ut_throw("could not access file '%s'", bin);
                free(bin);
                goto error;
            } else {
                free(bin);
            }
        }
    }

    free(pkg_underscore);

    /* Prevent looking for the binary again */
    loaded->tried_binary = true;

    return 0;
error:
    return -1;
}

const char* ut_locate(
    const char* package,
    ut_dl *dl_out,
    ut_locate_kind kind)
{
    char *result = NULL;
    const char *env = NULL;
    struct ut_loaded *loaded = NULL;

    if (!package[0]) {
        ut_throw("invalid package identifier");
        goto error;
    }

    ut_try ( ut_mutex_lock(&UT_LOAD_LOCK), NULL);

    /* If package has been loaded already, don't resolve it again */
    loaded = ut_loaded_find(package);
    if (loaded && loaded->tried_locating) {
        if (!loaded->env) {
            ut_debug("locating '%s' failed before", package);
            goto error;
        }

        env = loaded->env;
    } else {
        /* If package has not been found, try locating it */
        if (ut_locate_package(package, &env) == -1) {
            /* Error happened other than not being able to find the package */
            ut_throw(NULL);
            goto error;
        }
    }

    if (!loaded) {
        loaded = ut_loaded_add(package);
        loaded->loading = 0; /* locate is not going to load package */
    }

    /* Only try to locate packages once */
    loaded->tried_locating = true;

    /* If package is not in load admin but has been located, add to admin */
    if (!loaded->env && env) {
        ut_strset(&loaded->env, env);
        loaded->project = ut_asprintf("%s%cmeta%c%s", env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, package);
    }

    /* If loaded hasn't been loaded by now, package isn't found */
    if (!loaded->env) {
        ut_trace("could not locate '%s'", package);
        goto error;
    }

    /* If env has been found, derive location */
    if (env) {
        switch(kind) {
        case UT_LOCATE_ENV:
            result = loaded->env; /* always set */
            break;
        case UT_LOCATE_PROJECT:
            result = loaded->project; /* always set */
            break;
        case UT_LOCATE_ETC:
            if (!loaded->etc) {
                loaded->etc = ut_asprintf("%s%cetc%c%s", loaded->env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, package);
            }
            result = loaded->etc;
            break;
        case UT_LOCATE_INCLUDE:
            if (!loaded->include) {
                loaded->include = ut_asprintf("%s%cinclude%c%s.dir", loaded->env, PATH_SEPARATOR_C, PATH_SEPARATOR_C, package);
            }
            result = loaded->include;
            break;
        case UT_LOCATE_LIB:
        case UT_LOCATE_STATIC:
        case UT_LOCATE_APP:
        case UT_LOCATE_BIN:
            if (!loaded->tried_binary) {
                if (ut_locate_binary(package, loaded)) {
                    goto error;
                }
            }
            if (kind == UT_LOCATE_LIB) result = loaded->lib;
            if (kind == UT_LOCATE_STATIC) result = loaded->static_lib;
            if (kind == UT_LOCATE_APP) result = loaded->app;
            if (kind == UT_LOCATE_BIN) result = loaded->bin;
            break;
        }
    }

    /* If locating a library, library is requested and lookup is successful,
     * load library within lock (can save additional lookups/locks) */
    if (dl_out && kind == UT_LOCATE_LIB && loaded->lib) {
        if (!loaded->library) {
            loaded->library = ut_dl_open(loaded->lib);
            if (!loaded->library) {
                ut_throw("failed to open library '%s': %s",
                    loaded->lib, ut_dl_error());
                goto error;
            }
        }
        *dl_out = loaded->library;
    } else if (dl_out) {
        /* Library was not found */
    }

    ut_try (
        ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

    return result;
error:
    if (ut_mutex_unlock(&UT_LOAD_LOCK)) {
        ut_throw(NULL);
    }
    return NULL;
}

void ut_locate_reset(
    const char *package)
{
    struct ut_loaded *loaded = NULL;

    ut_try ( ut_mutex_lock(&UT_LOAD_LOCK), NULL);

    loaded = ut_loaded_find(package);
    if (loaded) {
        if (!loaded->loading && !loaded->library) {
            free(loaded->env);
            free(loaded->bin);
            free(loaded->etc);
            free(loaded->include);
            free(loaded->project);

            loaded->env = NULL;
            loaded->lib = NULL;
            loaded->static_lib = NULL;
            loaded->app = NULL;
            loaded->bin = NULL;
            loaded->etc = NULL;
            loaded->include = NULL;
            loaded->project = NULL;

            loaded->tried_binary = false;
            loaded->tried_locating = false;
            loaded->loaded = 0;
        }
    }

    ut_try ( ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

    return;
error:
    ut_raise();
}

static
struct ut_fileHandler* ut_load_filehandler(
    const char *file)
{
    /* Get extension from filename */
    char ext[16];

    if (ut_file_test(file) == 1) {
        /* If file is found, it will be loaded directly. Lookup file handler by
         * extension. */
        if (!ut_file_extension((char*)file, ext)) {
            goto error;
        }
    } else {
        /* If file is not found, assume this is a logical id. Logical ids can be
         * package identifiers that are stored in the package repository. */
        ext[0] = '\0';
    }

    /* Lookup corresponding handler for file */
    struct ut_fileHandler *h = ut_lookupExt(ext);

    /* If filehandler is not found, look it up in driver/ext */
    if (!h) {
        ut_id extPackage;
        sprintf(extPackage, "driver%cext%c%s", PATH_SEPARATOR_C, PATH_SEPARATOR_C, ext);

        ut_try(
            ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

        /* Try to load the extension package */
        if (ut_use(extPackage, 0, NULL)) {
            ut_throw(
                "unable to load file '%s'",
                file,
                ext);
            goto error;
        }
        ut_try (
            ut_mutex_lock(&UT_LOAD_LOCK), NULL);

        /* Extension package should have registered the extension in the
         * cortomain function, so try loading again. */
        h = ut_lookupExt(ext);
        if (!h) {
            ut_throw(
                "package 'driver%cext%c%s' loaded but extension is not registered",
                PATH_SEPARATOR_C, PATH_SEPARATOR_C, ext);
            ut_try(
                ut_mutex_unlock(&UT_LOAD_LOCK), NULL);
            goto error;
        }
    }
    return h;
error:
    return NULL;
}

/* Load a file of a supported type (file path or in package repo) */
int ut_load_intern(
    const char* file,
    int argc,
    char* argv[],
    bool ignore_recursive,
    bool always_load)
{
    struct ut_fileHandler* h;
    int result = -1;
    bool other_thread_loading = false;
    bool loaded_by_me = false;

    ut_log_push(strarg("load:%s", file));

    ut_try(
        ut_mutex_lock(&UT_LOAD_LOCK), NULL);

    /* Check if file is added to admin */
    struct ut_loaded *loaded_admin = ut_loaded_find(file);

    if (loaded_admin) {
        if (loaded_admin->loading == ut_thread_self()) {
            /* Illegal recursive load of file */
            goto recursive;
        } else {
            other_thread_loading = true;
        }
    }

    if (!loaded_admin) {
        loaded_admin = ut_loaded_add(file);
    }

    /* If other thread is loading file, wait until it finishes. This can happen
     * when the other thread unlocked to allow for running the file handler. */
    if (other_thread_loading) {
        while (loaded_admin->loading) {
            /* Need to unlock, as other thread will try to relock after the file
             * handler is executed. */
            ut_try(
                ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

            while (loaded_admin->loading) {
                ut_sleep(0, 100000000);
            }

            /* Relock, so we can safely inspect & modify the admin again. */
            ut_try(
                ut_mutex_lock(&UT_LOAD_LOCK), NULL);

            /* Keep looping until we have the lock and there is no other thread
             * currently loading this file */
        }
    }

    /* Only load when file was not loaded, except when always_load is set */
    if (loaded_admin->loaded == 0 || always_load) {
        /* Load handler for file type */
        h = ut_load_filehandler(file);
        if (!h) {
            ut_throw(NULL);
            goto error;
        }

        /* This thread is going to load the file */
        loaded_admin->loading = ut_thread_self();

        /* Unlock, so file handler can load other files without deadlocking */
        ut_try(
            ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

        /* Load file */
        result = h->load((char*)file, argc, argv, h->userData);

        /* Relock admin to update */
        ut_try(
            ut_mutex_lock(&UT_LOAD_LOCK), NULL);

        /* Set loaded to 1 if success, or -1 if failed */
        loaded_admin->loaded = result ? -1 : 1;

        /* Thread is no longer loading the file */
        loaded_admin->loading = 0;

        loaded_by_me = true;
    } else {
        result = loaded_admin->loaded == 1 ? 0 : -1;
    }

    ut_try(
        ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

    if (!result) {
        if (loaded_by_me) {
            ut_ok("loaded '%s'", file);
        } else {
            ut_debug("already loaded '%s'", file);
        }
    }

    ut_log_pop();

    return result;
recursive:
    if (!ignore_recursive) {
        ut_strbuf detail = UT_STRBUF_INIT;
        ut_throw("illegal recursive load of file '%s'", loaded_admin->id);
        ut_strbuf_appendstr(&detail, "error occurred while loading:\n");
        ut_iter iter = ut_ll_iter(loadedAdmin);
        while (ut_iter_hasNext(&iter)) {
            struct ut_loaded *lib = ut_iter_next(&iter);
            if (lib->loading) {
                ut_strbuf_append(
                    &detail,
                    "    - #[cyan]%s#[normal] #[magenta]=>#[normal] #[white]%s\n",
                    lib->id, lib->bin ? lib->bin : "");
            }
        }
        char *str = ut_strbuf_get(&detail);
        ut_throw_detail(str);
        free(str);
    }
    ut_try(
        ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

    if (ignore_recursive) {
        ut_log_pop();
        return 0;
    }
error:
    ut_log_pop();
    return -1;
}

void* ut_load_sym(
    char *package,
    ut_dl *dl_out,
    char *symbol)
{
    void *result = NULL;

    if (!*dl_out) {
        const char *lib = ut_locate(package, dl_out, UT_LOCATE_LIB);
        if (!*dl_out) {
            if (lib) {
                *dl_out = ut_dl_open(lib);
            }
        }
        if (!*dl_out) {
            ut_throw(NULL);
            goto error;
        }
    }

    if (*dl_out) {
        result = ut_dl_sym(*dl_out, symbol);
        if (!result) {
            char *err = (char*)ut_dl_error();
            if (err) {
                ut_throw(err);
            } else {
                ut_throw("symbol lookup failed for '%s'", symbol);
            }
        }
    } else {
        ut_throw("failed to load '%s'", package);
    }

    return result;
error:
    return NULL;
}

void(*ut_load_proc(
    char *package,
    ut_dl *dl_out,
    char *symbol))(void)
{
    return (void(*)(void))(uintptr_t)ut_load_sym(package, dl_out, symbol);
}

/* Load file with no extension */
int ut_file_loader(
    char* package,
    int argc,
    char* argv[],
    void* ctx)
{
    UT_UNUSED(ctx);
    const char* fileName;
    int result;
    ut_dl dl = NULL;

    fileName = ut_locate(package, &dl, UT_LOCATE_LIB);
    if (!fileName) {
        ut_throw("failed to locate package '%s' as library", package);
        return -1;
    }

    ut_assert(dl != NULL, "package '%s' located but dl_out is NULL", package);

    result = ut_load_fromDl(dl, fileName, argc, argv);

    if (result) {
        ut_throw(NULL);
    }

    return result;
}

void ut_loaderOnExit(
    void* ctx)
{
    struct ut_fileHandler* h;
    ut_dl dl;
    ut_iter iter;

    UT_UNUSED(ctx);

    /* Free loaded administration. Always happens from mainthread, so no lock
     * required. */

    if (loadedAdmin) {
        iter = ut_ll_iter(loadedAdmin);
         while(ut_iter_hasNext(&iter)) {
             struct ut_loaded *loaded = ut_iter_next(&iter);
             free(loaded->id);
             if (loaded->env) free(loaded->env);
             if (loaded->lib) free(loaded->lib);
             if (loaded->app) free(loaded->app);
             if (loaded->etc) free(loaded->etc);
             if (loaded->include) free(loaded->include);
             if (loaded->project) free(loaded->project);
             free(loaded);
         }
         ut_ll_free(loadedAdmin);
    }

    /* Free handlers */
    while ((h = ut_ll_takeFirst(fileHandlers))) {
        free(h);
    }
    ut_ll_free(fileHandlers);

    /* Free libraries */
    if (libraries) {
        while ((dl = ut_ll_takeFirst(libraries))) {
            ut_dl_close(dl);
        }
        ut_ll_free(libraries);
    }
}

/* Load a package */
int ut_use(
    const char* str,
    int argc,
    char* argv[])
{
    return ut_load_intern(str, argc, argv, false, false);
}

/* Run a package */
int ut_run(
    const char* str,
    int argc,
    char* argv[])
{
    return ut_load_intern(str, argc, argv, false, true);
}

/* Register a filetype */
int ut_load_register(
    char* ext,
    ut_load_cb handler,
    void* userData)
{
    struct ut_fileHandler* h;

    if (ext[0] == '.') {
        ext ++;
    }

    /* Check if extension is already registered */
    ut_try(
        ut_mutex_lock(&UT_LOAD_LOCK), NULL);

    if ((h = ut_lookupExt(ext))) {
        if (h->load != handler) {
            ut_error(
                "ut_load_register: extension '%s' is already registered with another loader.",
                ext);
            abort();
            goto error;
        }
    } else {
        h = malloc(sizeof(struct ut_fileHandler));
        h->ext = ext;
        h->load = handler;
        h->userData = userData;

        if (!fileHandlers) {
            fileHandlers = ut_ll_new();
        }

        ut_trace("registered file extension '%s'", ext);
        ut_ll_append(fileHandlers, h);
    }

    ut_try(
        ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

    return 0;
error:
    return -1;
}

/* Initialize paths necessary for loader */
int16_t ut_load_init(
    const char *target,
    const char *home,
    const char *config)
{
    char *cwd = ut_strdup(ut_cwd());

    if (!target) target = ut_getenv("BAKE_TARGET");
    if (!home) home = ut_getenv("BAKE_HOME");
    if (!config) config = ut_getenv("BAKE_CONFIG");

    if (!target) {
        UT_LOAD_TARGET_PATH = ut_strdup(cwd);
    } else {
        UT_LOAD_TARGET_PATH = ut_asprintf("%s%c%s-%s",
          target, PATH_SEPARATOR_C, UT_PLATFORM_STRING, config);

        UT_LOAD_TARGET_META_PATH =
            ut_asprintf("%s%cmeta", UT_LOAD_TARGET_PATH, PATH_SEPARATOR_C);
    }

    if (!home) {
        UT_LOAD_HOME_PATH = ut_strdup(cwd);
    } else {
        UT_LOAD_HOME_PATH = ut_asprintf("%s", home);

        UT_LOAD_HOME_META_PATH =
            ut_asprintf("%s%cmeta", UT_LOAD_HOME_PATH, PATH_SEPARATOR_C);
    }

    if (strcmp(UT_LOAD_TARGET_PATH, UT_LOAD_HOME_PATH)) {
        UT_LOAD_TARGET_OTHER_THAN_HOME = true;
    }

    free(cwd);

    return 0;
}

const char* ut_load_homePath(void) {
    return UT_LOAD_HOME_PATH;
}

const char* ut_load_targetPath(void) {
    return UT_LOAD_TARGET_PATH;
}

const char* ut_load_homeMetaPath(void) {
    return UT_LOAD_HOME_META_PATH;
}

const char* ut_load_targetMetaPath(void) {
    return UT_LOAD_TARGET_META_PATH;
}


/* Initialize paths necessary for loader */
void ut_load_deinit(void)
{
    free(UT_LOAD_TARGET_PATH);
    free(UT_LOAD_HOME_PATH);
}
