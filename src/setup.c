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
    int sig = ut_proc_cmd(cmd, &ret);
    if (sig || ret) {
        ut_error("'%s' (%s %d)", cmd, sig ? "sig" : "result", sig ? sig : ret);
        return -1;
    }

    return 0;
}

int16_t bake_create_script(void)
{
    FILE *f = fopen ("/tmp/bake", "w");
    if (!f) {
        ut_error("cannot open '/tmp/bake': %s", strerror(errno));
        goto error;
    }

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
    fprintf(f, "        make -C build-linux\n");
    fprintf(f, "    elif [ \"$UNAME\" = \"Darwin\" ]; then\n");
    fprintf(f, "        make -C build-darwin\n");
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
    fprintf(f, "    exec $HOME/bake/bake $@\n");
    fprintf(f, "fi\n");
    fclose(f);

    /* Make executable for everyone */
    if (ut_setperm("/tmp/bake", 0755)) {
        ut_raise();
        ut_log("failed to set permissions of bake script\n");
    }

    /* Copy file to global location, may ask for password */
    ut_log("#[yellow]ATTENTION#[reset] -- copying script to '" GLOBAL_PATH "', setup may request password\n");
    ut_try( cmd("sudo cp /tmp/bake " GLOBAL_PATH "/bake"),
      "Failed to instal bake script. If you do not have the privileges to install to\n"
      GLOBAL_PATH ", you can install bake just to your home directory by doing:\n"
      "   bake setup --local-setup\n");
    ut_log("#[green]OK#[reset] bake script installed to '" GLOBAL_PATH "'\n");

    return 0;
error:
    return -1;
}

int16_t bake_setup(
    bool local)
{
    ut_try( ut_mkdir("~/bake"), NULL);

    ut_log("Bake setup, installing to $BAKE_HOME ('%s')\n", ut_getenv("BAKE_HOME"));

    if (!local) {
        ut_try(bake_create_script(), "failed to create global bake script");
    }

    ut_try(ut_cp("./bake", "~/bake/bake"),
        "failed to copy bake executable to user bake environment");
    ut_log("#[green]OK#[reset] bake executable copied to $BAKE_HOME\n");

    ut_try(cmd("bake install --id bake --type application --includes include --build-to-home"),
        "failed to install bake include files");
    ut_log("#[green]OK#[reset] bake include files installed to $BAKE_HOME\n");

    ut_try(cmd("bake install --id bake.util --type package --includes util/include --build-to-home"),
        "failed to install bake util include files");
    ut_log("#[green]OK#[reset] bake utility include files installed to $BAKE_HOME\n");

    ut_try(cmd(strarg("make -C util/build-%s clean all", UT_OS_STRING)),
        "failed to build utility library");
    ut_log("#[green]OK#[reset] bake utility library built\n");

    if (!strcmp(UT_OS_STRING, "darwin")) {
        ut_try (ut_rename("util/libbake_util.dylib", "util/libbake_util.so"),
            "failed to rename bake util library");
    }

    ut_try(cmd(
      "bake install util --id bake.util --artefact libbake_util.so --build-to-home"),
        "failed to install bake utility library");
    ut_log("#[green]OK#[reset] bake utility library installed to $BAKE_HOME\n");

    ut_try(cmd(strarg("make -C drivers/lang/c/build-%s clean all", UT_OS_STRING)),
        "failed to build C driver");
    ut_log("#[green]OK#[reset] bake C driver built\n");

    if (!strcmp(UT_OS_STRING, "darwin")) {
        ut_try (ut_rename("drivers/lang/c/libbake_lang_c.dylib", "drivers/lang/c/libbake_lang_c.so"),
            "failed to rename bake C driver library");
    }

    ut_try(cmd(
      "bake install drivers/lang/c --id bake.lang.c --artefact libbake_lang_c.so --build-to-home"),
        "failed to install bake C driver");
    ut_log("#[green]OK#[reset] bake C driver installed to $BAKE_HOME\n");

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

    printf("     ~ installation complete ~\n\n");

    return 0;
error:
    return -1;
}
