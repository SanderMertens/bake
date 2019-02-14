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

/* Information about current target */
char *UT_ARCH;
char *UT_OS;
char *UT_PLATFORM;
char *UT_CONFIG;

/* Paths pointing to bake environment and current target */
char *UT_HOME_PATH;
char *UT_TARGET_PATH;
char *UT_PLATFORM_PATH;
char *UT_META_PATH;
char *UT_INCLUDE_PATH;
char *UT_ETC_PATH;
char *UT_SRC_PATH;
char *UT_TEMPLATE_PATH;
char *UT_BIN_PATH;
char *UT_LIB_PATH;
char *UT_JAVA_PATH;
char *UT_HOME_LIB_PATH;
char *UT_HOME_BIN_PATH;

/* File extensions for binaries for current target */
char *UT_SHARED_LIB_EXT;
char *UT_STATIC_LIB_EXT;
char *UT_EXECUTABLE_EXT;
char *UT_BIN_EXT;
char *UT_LIB_EXT;
char *UT_STATIC_LIB_EXT;
char *UT_LIB_PREFIX;

/* Lock protecting the package administration */
extern ut_mutex_s UT_LOAD_LOCK;

struct ut_loaded {
    char* id; /* package id or file */

    char *lib; /* Path to library (if available) */
    char *static_lib; /* Path to static library (if available) */
    char *app; /* Path to executable (if available) */
    char *bin; /* Path to binary (if available) */
    char *etc; /* Path to project etc (if available) */
    char *src; /* Path to project source (if available) */
    char *dev; /* Path to project in development (if available) */
    char *include; /* Path to project include (if available) */
    char *template; /* Path to template */
    char *meta; /* Path to metadata. Always available if valid project */
    char *repo; /* Repository name (replace . with -) */
    bool tried_binary; /* Set to true if tried loading the bin path */
    bool tried_locating; /* Set to true if tried locating package */
    bool tried_src; /* Set to true if tried locating source */

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
        if (nameptr[0] == UT_OS_PS[0]) nameptr ++;
        if (nameptr[0] == '.') nameptr ++;

        while (ut_iter_hasNext(&iter)) {
            lib = ut_iter_next(&iter);
            const char *idptr = lib->id;
            if (idptr[0] == UT_OS_PS[0]) idptr ++;
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

    /* support for this will be dropped */

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

/* Load a library
 * Receives the absolute path to the lib<name>.so (or equivalent) file. */
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

/* An adapter on top of ut_load_library to fit the ut_load_cb signature. */
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
int16_t ut_locate_binary(
    const char *id,
    const char *config,
    struct ut_loaded *loaded,
    int *found)
{
    char *id_bin = ut_strdup(id);
    char *ptr, ch;
    for (ptr = id_bin; (ch = *ptr); ptr ++) {
        if (ch == '.') {
            *ptr = '_';
        }
    }

    int16_t ret = 0;
    bool clean_path = false;

    char *bin = NULL;
    char *lib_path = UT_LIB_PATH;
    char *bin_path = UT_BIN_PATH;

    if (ut_project_is_buildtool(id)) {
        lib_path = UT_HOME_LIB_PATH;
        bin_path = UT_HOME_BIN_PATH;
    } else if (strcmp(config, UT_CONFIG)) {
        lib_path = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS"lib", UT_PLATFORM_PATH, config);
        bin_path = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS"bin", UT_PLATFORM_PATH, config);
        clean_path = true;
    }

    /* Test for dynamic library */
    bin = ut_asprintf("%s"UT_OS_PS "%s%s%s", lib_path, UT_LIB_PREFIX, id_bin, UT_SHARED_LIB_EXT);
    if ((ret = ut_file_test(bin)) == 1) {
        /* Library found */
        if (loaded) {
            loaded->lib = bin;
            loaded->bin = bin;
        }
        if (found) *found = true;
    } else {
        if (ret != 0) {
            ut_throw("could not access file '%s'", bin);
            free(bin);
            goto error;
        } else {
            free(bin);
        }
    }

    /* Test for static library */
    if (ret == 0) {
        bin = ut_asprintf("%s"UT_OS_PS "%s%s%s", lib_path, UT_LIB_PREFIX, id_bin, UT_STATIC_LIB_EXT);
        if ((ret = ut_file_test(bin)) == 1) {
            /* Library found */
            if (loaded) {
                loaded->static_lib = bin;
                loaded->bin = bin;
            }
            if (found) *found = true;
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
        bin = ut_asprintf("%s"UT_OS_PS"%s%s", bin_path, id_bin, UT_EXECUTABLE_EXT);
        if ((ret = ut_file_test(bin)) == 1) {
            /* Executable found */
            if (loaded) {
                loaded->app = bin;
                loaded->bin = bin;
            }
            if (found) *found = true;
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

    free(id_bin);

    if (clean_path) {
        free(lib_path);
        free(bin_path);
    }

    return 0;
error:
    if (clean_path) {
        free(lib_path);
        free(bin_path);
    }
    return -1;
}

static
int16_t ut_locate_src(
    const char *id,
    struct ut_loaded *loaded)
{
    if (loaded->tried_src) {
        return 0;
    }

    char *src = ut_asprintf("%s"UT_OS_PS"%s", UT_SRC_PATH, loaded->repo);
    if (ut_file_test(src) == 1) {
        loaded->src = src;
    } else {
        ut_trace("sources for '%s' not found in '%s'", id, src);
        free(src);
    }

    src = ut_asprintf("%s"UT_OS_PS"source.txt", loaded->meta);
    if (ut_file_test(src) == 1) {
        char *path = ut_file_load(src);

        /* Trim newline */
        size_t len = strlen(path);
        if (path[len - 1] == '\n') {
            path[len - 1] = '\0';
        }

        if (ut_file_test(path) == 1) {
            loaded->dev = path;
        } else {
            free(path);
        }
    }

    loaded->tried_src = true;

    return 0;
}

/* Locate various paths for projects in the bake environment */
const char* ut_locate(
    const char* id,
    ut_dl *dl_out,
    ut_locate_kind kind)
{
    char *result = NULL;
    const char *env = NULL;
    struct ut_loaded *loaded = NULL;

    if (!id || !id[0]) {
        ut_throw("invalid (empty) project identifier");
        goto error;
    }

    ut_try ( ut_mutex_lock(&UT_LOAD_LOCK), NULL);

    /* If package has been loaded already, don't resolve it again */
    loaded = ut_loaded_find(id);
    if (!loaded) {
        loaded = ut_loaded_add(id);
        loaded->loading = 0; /* locate is not going to load project */
    }

    if (loaded && loaded->tried_locating) {
        /* Templates don't occupy the same namespace as ordinary projects */
        if (kind != UT_LOCATE_TEMPLATE) {
            if (!loaded->meta) {
                ut_debug("locating project '%s' failed before", id);
                goto error;
            }
        } else {
            if (!loaded->template) {
                ut_debug("locating template '%s' failed before", id);
                goto error;
            }
        }
    }

    /* Test whether project or template project exists */
    if (kind == UT_LOCATE_TEMPLATE) {
        if (!loaded->template) {
            char *tmpl_path = ut_asprintf("%s"UT_OS_PS"%s", UT_TEMPLATE_PATH, id);
            if (ut_file_test(tmpl_path) == 1) {
                loaded->template = tmpl_path;
            } else {
                ut_trace("template path '%s' not found", tmpl_path);
                free(tmpl_path);
                goto error;
            }
        }
    } else {
        if (!loaded->meta) {
            char *meta_path = ut_asprintf("%s"UT_OS_PS"%s", UT_META_PATH, id);
            if (ut_file_test(meta_path) == 1) {
                loaded->meta = meta_path;

                /* Prepare repository identifier (replaces . with -) */
                loaded->repo = ut_strdup(id);
                char *ptr, ch;
                for (ptr = loaded->repo; (ch = *ptr); ptr ++) {
                    if (ch == '.') {
                        *ptr = '-';
                    }
                }
            } else {
                ut_trace("template path '%s' not found", meta_path);
                free(meta_path);
                goto error;
            }
        }
    }

    /* Only try to locate packages once (until reset is called) */
    loaded->tried_locating = true; 

    /* If env has been found, derive location */
    switch(kind) {
    case UT_LOCATE_PROJECT:
        result = loaded->meta;
        break;
    case UT_LOCATE_TEMPLATE:
        result = loaded->template;
        break;        
    case UT_LOCATE_ETC:
        if (!loaded->etc) {
            loaded->etc = ut_asprintf("%s"UT_OS_PS"%s", UT_ETC_PATH, id);
        }
        result = loaded->etc;
        break;
    case UT_LOCATE_INCLUDE:
        if (!loaded->include) {
            loaded->include = ut_asprintf("%s"UT_OS_PS"%s.dir", UT_INCLUDE_PATH, id);
        }
        result = loaded->include;
        break;
    case UT_LOCATE_SOURCE:
        if (!loaded->tried_src) {
            ut_try (ut_locate_src(id, loaded), NULL);
            loaded->tried_src = true;
        }
        result = loaded->src;
        break;
    case UT_LOCATE_DEVSRC:
        if (!loaded->tried_src) {
            ut_try (ut_locate_src(id, loaded), NULL);
            loaded->tried_src = true;
        }
        result = loaded->dev;
        break;
    case UT_LOCATE_LIB:
    case UT_LOCATE_STATIC:
    case UT_LOCATE_APP:
    case UT_LOCATE_BIN:
        if (!loaded->tried_binary) {
            ut_try (ut_locate_binary(id, UT_CONFIG, loaded, NULL), NULL);
            loaded->tried_binary = true;
        }
        if (kind == UT_LOCATE_LIB) result = loaded->lib;
        if (kind == UT_LOCATE_STATIC) result = loaded->static_lib;
        if (kind == UT_LOCATE_APP) result = loaded->app;
        if (kind == UT_LOCATE_BIN) result = loaded->bin;
        break;
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

    ut_try ( ut_mutex_unlock(&UT_LOAD_LOCK), NULL);

    return result;
error:
    if (ut_mutex_unlock(&UT_LOAD_LOCK)) {
        ut_throw(NULL);
    }
    return NULL;
}

bool ut_project_is_buildtool(
    const char *id)
{
    return !strncmp(id, "bake.", 5);
}

bool ut_project_in_config(
    const char *id,
    const char *config)
{
    int found = 0;
    ut_locate_binary(id, config, NULL, &found);
    return found;
}

void ut_locate_reset(
    const char *package)
{
    struct ut_loaded *loaded = NULL;

    ut_try ( ut_mutex_lock(&UT_LOAD_LOCK), NULL);

    loaded = ut_loaded_find(package);
    if (loaded) {
        if (!loaded->loading && !loaded->library) {
            free(loaded->bin);
            free(loaded->etc);
            free(loaded->src);
            free(loaded->dev);
            free(loaded->include);
            free(loaded->meta);
            free(loaded->template);

            loaded->bin = NULL;
            loaded->etc = NULL;
            loaded->src = NULL;
            loaded->dev = NULL;
            loaded->include = NULL;
            loaded->meta = NULL;
            loaded->template = NULL;
            loaded->app = NULL;
            loaded->lib = NULL;
            loaded->static_lib = NULL;

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
        char extPackage[512];
        sprintf(extPackage, "driver"UT_OS_PS"ext"UT_OS_PS"%s", ext);

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
                "package 'driver"UT_OS_PS"ext"UT_OS_PS"%s' loaded but extension is not registered",
                ext);
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
    bool used_dlopen = false;

    if (!*dl_out) {
        const char *lib = ut_locate(package, dl_out, UT_LOCATE_LIB);
        if (!*dl_out) {
            if (lib) {
                *dl_out = ut_dl_open(lib);
                used_dlopen = true;
            }
        }
        if (!*dl_out) {
            if (used_dlopen) {
                char *err = (char*)ut_dl_error();
                ut_throw("failed to load library '%s': %s", lib, err);
            } else {
                ut_throw("failed to locate binary for package '%s'", package);
            }
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
             if (loaded->lib) free(loaded->lib);
             if (loaded->app) free(loaded->app);
             if (loaded->etc) free(loaded->etc);
             if (loaded->include) free(loaded->include);
             if (loaded->meta) free(loaded->meta);
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

static
void ut_load_(
    const char *env,
    const char *value)
{
    char *existing = ut_getenv(env);
    if (!existing || !strlen(existing)) {
        ut_setenv(env, value);
    } else {
        ut_setenv(env, strarg("%s:%s", value, existing));
    }
}

/* Initialize paths necessary for loader */
int16_t ut_load_init(
    const char *home,
    const char *arch,
    const char *os,
    const char *config)
{
    if (!home) {
        home = ut_getenv("BAKE_HOME");
        if (!home) {
            home = "~"UT_OS_PS"bake";
        }
    }

    if (!arch) {
        arch = ut_getenv("BAKE_ARCHITECTURE");
        if (!arch) {
            arch = UT_CPU_STRING;
        }
    }
    
    if (!os) {
        os = ut_getenv("BAKE_OS");
        if (!os) {
            os = UT_OS_STRING;
        }
    }

    if (!config) {
        config = ut_getenv("BAKE_CONFIG");
        if (!config) {
            config = "debug";
        }
    }

    UT_ARCH = ut_strdup(arch);
    UT_OS = ut_strdup(os);
    UT_PLATFORM = ut_asprintf("%s-%s", arch, os);
    UT_CONFIG = ut_strdup(config);

    UT_HOME_PATH = ut_envparse("%s", home);
    UT_TARGET_PATH = ut_asprintf("%s"UT_OS_PS"%s"UT_OS_PS"%s", UT_HOME_PATH, UT_PLATFORM, config);
    UT_PLATFORM_PATH = ut_asprintf("%s"UT_OS_PS"%s", UT_HOME_PATH, UT_PLATFORM);
    UT_META_PATH = ut_asprintf("%s"UT_OS_PS"meta", UT_HOME_PATH);
    UT_SRC_PATH = ut_asprintf("%s"UT_OS_PS"src", UT_HOME_PATH);
    UT_INCLUDE_PATH = ut_asprintf("%s"UT_OS_PS"include", UT_HOME_PATH);
    UT_ETC_PATH = ut_asprintf("%s"UT_OS_PS"etc", UT_HOME_PATH);
    UT_TEMPLATE_PATH = ut_asprintf("%s"UT_OS_PS"templates", UT_HOME_PATH);
    UT_BIN_PATH = ut_asprintf("%s"UT_OS_PS"bin", UT_TARGET_PATH);
    UT_LIB_PATH = ut_asprintf("%s"UT_OS_PS"lib", UT_TARGET_PATH);
    UT_JAVA_PATH = ut_asprintf("%s"UT_OS_PS"java", UT_HOME_PATH);
    UT_HOME_LIB_PATH = ut_asprintf("%s"UT_OS_PS"lib", UT_HOME_PATH);
    UT_HOME_BIN_PATH = ut_asprintf("%s"UT_OS_PS"bin", UT_HOME_PATH);

    /* For now, default to current platform */
    UT_SHARED_LIB_EXT = UT_OS_LIB_EXT;
    UT_STATIC_LIB_EXT = UT_OS_STATIC_LIB_EXT;
    UT_EXECUTABLE_EXT = UT_OS_BIN_EXT;
    UT_LIB_PREFIX = UT_OS_LIB_PREFIX;
    UT_BIN_EXT = UT_OS_BIN_EXT;
    UT_LIB_EXT = UT_OS_LIB_EXT;
    UT_STATIC_LIB_EXT = UT_OS_STATIC_LIB_EXT;
    UT_LIB_PREFIX = UT_OS_LIB_PREFIX;

    /* Set environment variables */
    ut_setenv("BAKE_HOME", UT_HOME_PATH);
    ut_setenv("BAKE_TARGET", UT_TARGET_PATH);
    ut_setenv("BAKE_CONFIG", config);
    ut_setenv("BAKE_ARCH", UT_ARCH);
    ut_setenv("BAKE_OS", UT_OS);
    ut_setenv("BAKE_PLATFORM", UT_PLATFORM);

    /* Set system environment variables */
    ut_appendenv(UT_ENV_BINPATH, "~"UT_OS_PS"bake");
    ut_appendenv(UT_ENV_BINPATH, "%s", UT_BIN_PATH);
    ut_appendenv(UT_ENV_BINPATH, ".");
    ut_appendenv(UT_ENV_LIBPATH, "%s", UT_LIB_PATH);
    ut_appendenv(UT_ENV_LIBPATH, "%s", UT_HOME_LIB_PATH);
    ut_appendenv("CLASSPATH", "%s", UT_JAVA_PATH);

#ifdef UT_ENV_DYLIBPATH
    ut_appendenv(UT_ENV_DYLIBPATH, "%s", UT_LIB_PATH);
    ut_appendenv(UT_ENV_DYLIBPATH, "%s", UT_HOME_LIB_PATH);
#endif

    return 0;
}

/* Initialize paths necessary for loader */
void ut_load_deinit(void)
{
    free(UT_HOME_PATH);
    free(UT_TARGET_PATH);
    free(UT_PLATFORM_PATH);
    free(UT_META_PATH);
    free(UT_SRC_PATH);
    free(UT_INCLUDE_PATH);
    free(UT_TEMPLATE_PATH);
    free(UT_BIN_PATH);
    free(UT_LIB_PATH);

    free(UT_ARCH);
    free(UT_OS);
    free(UT_PLATFORM);
    free(UT_CONFIG);
}
