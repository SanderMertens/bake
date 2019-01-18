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

#include "bake.h"

#define GLOBAL_PATH "/usr/local/bin"

static
int16_t cmd(
   char *cmd)
{
    int8_t ret;
    int sig = ut_proc_cmd_stderr_only(cmd, &ret);
    if (sig || ret) {
        ut_error("'%s' (%s %d)", cmd, sig ? "sig" : "result", sig ? sig : ret);
        return -1;
    }

    return 0;
}

int16_t bake_create_script(void)
{
    char *script_path = ut_envparse("$HOME/bake/bakes.sh");
    if (!script_path) {
        ut_error("missing $HOME environment variable");
        goto error;
    }

    FILE *f = fopen (script_path, "w");
    if (!f) {
        ut_error("cannot open '%s': %s", script_path, strerror(errno));
        goto error;
    }

    fprintf(f, "#!/bin/bash\n\n");

    fprintf(f, "UNAME=$(uname)\n\n");

    fprintf(f, "clone_bake() {\n");
    fprintf(f, "    if [ ! -d \"bake\" ]; then\n");
    fprintf(f, "        echo \"Cloning bake repository...\"\n");
    fprintf(f, "        git clone -q \"https://github.com/SanderMertens/bake.git\"\n");
    fprintf(f, "        cd \"bake\"\n");
    fprintf(f, "    else\n");
    fprintf(f, "        cd \"bake\"\n");
    fprintf(f, "        echo \"Reset bake repository...\"\n");
    fprintf(f, "        git fetch -q origin\n");
    fprintf(f, "        git reset -q --hard origin/master\n");
    fprintf(f, "        git clean -q -xdf\n");
    fprintf(f, "    fi\n");
    fprintf(f, "}\n\n");

    fprintf(f, "build_bake() {\n");
    fprintf(f, "    if [ \"$UNAME\" = \"Linux\" ]; then\n");
    fprintf(f, "        make -C build-Linux\n");
    fprintf(f, "    elif [ \"$UNAME\" = \"Darwin\" ]; then\n");
    fprintf(f, "        make -C build-Darwin\n");
    fprintf(f, "    fi\n");
    fprintf(f, "}\n\n");

    fprintf(f, "install_bake() {\n");
    fprintf(f, "    ./bake setup --local-setup\n");
    fprintf(f, "}\n\n");

    fprintf(f, "if [ \"$1\" = \"upgrade\" ]; then\n");
    fprintf(f, "    mkdir -p $HOME/bake/src\n");
    fprintf(f, "    cd $HOME/bake/src\n");
    fprintf(f, "    clone_bake\n");
    fprintf(f, "    build_bake\n");
    fprintf(f, "    install_bake\n");
    fprintf(f, "else\n");
    fprintf(f, "    export `$HOME/bake/bake env`\n");
    fprintf(f, "    exec $HOME/bake/bake \"$@\"\n");
    fprintf(f, "fi\n");
    fclose(f);

    /* Make executable for everyone */
    if (ut_setperm(script_path, 0755)) {
        ut_raise();
        ut_log("failed to set permissions of %s script\n", script_path);
    }

    /* Copy file to global location, may ask for password */
    ut_log("#[yellow]ATTENTION#[reset] copying script to '" GLOBAL_PATH "', setup may request password\n");

    char *cp_cmd = ut_asprintf("sudo cp %s %s/bake", script_path, GLOBAL_PATH);

    if(cmd(cp_cmd)) {
        printf("\n");
        ut_warning(
            "Failed to instal bake script to %s. Setup will continue, but\n"
            "      before you can use bake, you now first need to run:\n"
            "        export $(~/bake/bake env)\n", GLOBAL_PATH
        );
    } else {
        ut_log("#[green]OK#[reset]   install bake script to '" GLOBAL_PATH "'\n");
    }

    free(script_path);
    free(cp_cmd);

    return 0;
error:
    return -1;
}

int16_t bake_build_make_project(
    const char *path,
    const char *id,
    const char *artefact)
{
    char *install_cmd = ut_asprintf(
      "bake install %s --id %s --package --includes include --build-to-home",
      path, id);
    ut_try(cmd(install_cmd), "failed to install '%s' include files", id);
    free(install_cmd);
    ut_log("#[green]OK#[reset]   install include files for '%s'\n", id);

    ut_log("...  build '%s'", id);
    fflush(stdout);
    ut_try(cmd(strarg("make -C %s/build-%s clean all", path, UT_OS_STRING)),
      "failed to build '%s'", id);
    ut_log("#[green]OK#[reset]   build '%s'\n", id);

    char *bin_path = ut_asprintf("%s/bin/%s-debug", path, UT_PLATFORM_STRING);
    ut_try(ut_mkdir(bin_path), "failed to create bin path for %s", id);

    /* On macOS, remove rpath that premake automatically adds */
    /*#ifdef UT_MACOS
      install_cmd = ut_asprintf(
          "install_name_tool -change @rpath/libbake_util.dylib libbake_util.dylib %s/lib%s%s",
          path, artefact, UT_OS_LIB_EXT);
      ut_try( cmd(install_cmd), NULL);
    #endif
    */

    ut_try (ut_rename(
      strarg("%s/lib%s" UT_OS_LIB_EXT, path, artefact),
      strarg("%s/lib%s" UT_OS_LIB_EXT, bin_path, artefact)),
        "failed to move '%s' to project bin path", id);

    free(bin_path);

    install_cmd = ut_asprintf(
      "bake install %s --id %s --artefact %s --build-to-home --package",
      path, id, artefact);
    ut_try(cmd(install_cmd), "failed to install bake %s library", id);
    ut_log("#[green]OK#[reset]   install '%s' to $BAKE_HOME\n", id);
    free(install_cmd);

    return 0;
error:
    return -1;
}

int16_t bake_setup(
    const char *bake_cmd,
    bool local)
{
    char *dir = ut_strdup(bake_cmd);
    char *last_elem = strrchr(dir, '/');
    if (last_elem) {
        *last_elem = '\0';
        ut_chdir(dir);
    }

    /* Temporary fix to ensure bake shell script is updated on upgrade */
    local = false;

    char *cur_env = ut_envparse("~/bake");
    char *cur_dir = ut_strdup(ut_cwd());

    if (!strcmp(cur_env, cur_dir)) {
        char *tmp_dir = ut_envparse("~/bake_tmp");
        char *target_dir = ut_envparse("~/bake/src/bake");
        ut_try(ut_rm(tmp_dir), NULL);
        ut_try(ut_rename(cur_dir, tmp_dir), NULL);
        ut_try(ut_mkdir("~/bake/src"), NULL);
        ut_try(ut_rename(tmp_dir, target_dir), NULL);
        ut_try(ut_chdir(target_dir), NULL);
        free(target_dir);
        free(tmp_dir);
    }

    free(cur_dir);

    ut_try( ut_mkdir(cur_env), NULL);
    free (cur_env);

    ut_log("Bake setup, installing to $BAKE_HOME ('%s')\n", ut_getenv("BAKE_HOME"));

    if (!local) {
        ut_try(bake_create_script(),
          "failed to create global bake script");
    }

    ut_try(ut_cp("./bake", "~/bake/bake"),
        "failed to copy bake executable to user bake environment");
    ut_log("#[green]OK#[reset]   copy bake executable to $BAKE_HOME\n");

    ut_try(cmd("bake install --id bake --includes include --build-to-home"),
        "failed to install bake include files");
    ut_log("#[green]OK#[reset]   install bake include files to $BAKE_HOME\n");

    ut_try( bake_build_make_project("util", "bake.util", "bake_util"), NULL);

    ut_try( bake_build_make_project("drivers/lang/c", "bake.lang.c", "bake_lang_c"), NULL);

    ut_try( bake_build_make_project("drivers/lang/cpp", "bake.lang.cpp", "bake_lang_cpp"), NULL);

    ut_try(cmd("bake libraries --build-to-home"), NULL);
    ut_log("#[green]OK#[reset]   Installed library configuration packages\n");

    /*

     ______   ______   ______   __   __       ______   ______  ______
    /\  __ \ /\  ___\ /\  ___\ /\ \ /\ \     /\  __ \ /\  == \/\__  _\
    \ \  __ \\ \___  \\ \ \____\ \ \\ \ \    \ \  __ \\ \  __<\/_/\ \/
     \ \_\ \_\\/\_____\\ \_____\\ \_\\ \_\    \ \_\ \_\\ \_\ \_\ \ \_\
      \/_/\/_/ \/_____/ \/_____/ \/_/ \/_/     \/_/\/_/ \/_/ /_/  \/_/

     */

    ut_log(
      "#[white]\n"
      "#[normal]    #[cyan]___      ___      ___      ___ \n"
      "#[normal]   /\\#[cyan]  \\    #[normal]/\\#[cyan]  \\    #[normal]/\\#[cyan]__\\    #[normal]/\\  #[cyan]\\ \n"
      "#[normal]  /  \\#[cyan]  \\  #[normal]/  \\#[cyan]  \\  #[normal]/ / #[cyan]_/_  #[normal]/  \\  #[cyan]\\ \n"
      "#[normal] /  \\ \\#[cyan]__\\#[normal]/  \\ \\#[cyan]__\\#[normal]/  -\"\\#[cyan]__\\#[normal]/  \\ \\#[cyan]__\\ \n"
      "#[normal] \\ \\  /#[cyan]  /#[normal]\\/\\  /#[cyan]  /#[normal]\\; ;-\"#[cyan],-\"#[normal]\\ \\ \\/  #[cyan]/ \n"
      "#[normal]  \\  /#[cyan]  /   #[normal]/ /  #[cyan]/  #[normal]| |  #[cyan]|   #[normal]\\ \\/  #[cyan]/ \n"
      "#[normal]   \\/#[cyan]__/    #[normal]\\/#[cyan]__/    #[normal]\\|#[cyan]__|    #[normal]\\/#[cyan]__/ \n\n");

    printf("\n       Installation complete \n\n");

    return 0;
error:
    return -1;
}
