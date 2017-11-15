#include "bake.h"

char *repositories[] = {
    "https://github.com/cortoproject/base",
    "https://github.com/cortoproject/bake",
    "https://github.com/cortoproject/driver-bake-c",
    NULL
};

static
int16_t bake_setup_cmd(char *msg, char *cmd) {
    int8_t ret;
    int sig = corto_proc_cmd(cmd, &ret);
    if (sig || ret) {
        corto_print("🔥 %s\n", msg);
        return -1;
    } else {
        corto_print("#[green]√#[normal] #[grey]%s#[normal]\n", msg);
        return 0;
    }
}

static
int16_t bake_verify_repos(void) {
    int i, missing = 0;
    char *repo;

    for (i = 0; (repo = repositories[i]); i ++) {
        char *lastElem = strrchr(repo, '/');
        if (!lastElem) {
            corto_throw("invalid repository identifier '%s'", repo);
            goto error;
        }
        lastElem ++;
        if (corto_file_test(lastElem)) {
            corto_print("#[green]√#[normal] #[grey]%s#[normal]\n", repo);
        } else {
            corto_print("#[red]x#[normal] #[grey]%s#[normal]\n", repo);
            missing ++;
        }
    }

    return missing;
error:
    return -1;
}

static
int16_t bake_clone_repos(void) {
    int i, missing = 0;
    char *repo;

    for (i = 0; (repo = repositories[i]); i ++) {
        char *lastElem = strrchr(repo, '/');
        if (!lastElem) {
            corto_throw("invalid repository identifier '%s'", repo);
            goto error;
        }
        lastElem ++;
        if (!corto_file_test(lastElem)) {
            if (bake_setup_cmd(
                strarg("clone '%s'", repo),
                strarg("git clone %s", repo)))
            {
                goto error;
            }
        }
    }

    return missing;
error:
    return -1;
}

int16_t bake_setup(void) {
    corto_log_verbositySet(CORTO_ERROR);
    corto_print("🍰 #[bold]welcome to the bake setup!#[normal]\n");
    corto_print("🍰 checking availability of repositories...\n");

    /* If current working directory contains bake executable, move one directory up */
    if (corto_file_test("bake") && !corto_isdir("bake")) {
        if (corto_chdir("..")) {
            corto_throw(NULL);
            goto error;
        }
    }

    if (!bake_verify_repos()) {
        corto_print("🍰 all repositories are here, installing bake...\n");
    } else {
        corto_print("🍰 cloning missing repositories...\n");
        if (bake_clone_repos()) {
            goto error;
        }
    }

    if (bake_setup_cmd(
        "copy bake executable",
        "bake/bake copy bake --id bake --kind tool --artefact bake"))
    { goto error; }

    if (bake_setup_cmd(
        "copy base include files",
        "bake copy base --id corto --includes include/corto"))
    { goto error; }

    if (bake_setup_cmd(
        "copy bake include files",
        "bake copy bake --id bake --kind application  --includes include/bake"))
    { goto error; }

    if (bake_setup_cmd(
        "build driver/bake/c",
        "make -C driver-bake-c/build"))
    { goto error; }

    if (bake_setup_cmd(
        "copy driver/bake/c binary to package repository",
        "bake copy driver-bake-c --id driver/bake/c --artefact libc.so --skip-preinstall"))
    { goto error; }

    corto_print("🎂 done!\n");
    return 0;
error:
    corto_print("🔥 failed to install bake :(\n");
    return -1;
}
