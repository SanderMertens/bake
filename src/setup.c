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
    FILE *f = fopen ("/usr/local/bin/bake", "w");
    if (!f) {
        ut_error("cannot open '/usr/local/bin/bake': %s", strerror(errno));
        ut_log("  try running as 'sudo bake setup', or 'bake setup --local-setup'\n");
        goto error;
    }

    fprintf(f, "UNAME=$(uname)\n\n");

    fprintf(f, "clone_bake() {\n");
    fprintf(f, "    if [ ! -d \"bake\" ]; then\n");
    fprintf(f, "        echo \"Cloning bake repository...\"\n");
    fprintf(f, "        git clone -q \"https://github.com/SanderMertens/bake.git\"\n");
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
    fprintf(f, "    pwd\n");
    fprintf(f, "    ./bake --setup-local\n");
    fprintf(f, "}\n\n");

    fprintf(f, "if [ \"$1\" = \"upgrade\" ]; then\n");
    fprintf(f, "    mkdir -p $HOME/bake/src\n");
    fprintf(f, "    cd $HOME/bake/src\n");
    fprintf(f, "    clone_bake\n");
    fprintf(f, "    cd bake\n");
    fprintf(f, "    build_bake\n");
    fprintf(f, "    install_bake\n");
    fprintf(f, "else\n");
    fprintf(f, "    exec $HOME/bake/bake $@\n");
    fprintf(f, "fi\n");
    fclose(f);

    /* Make executable for everyone */
    if (ut_setperm("/usr/local/bin/bake", 0755)) {
        ut_raise();
        ut_log(
       "#[red]x#[normal] failed to set permissions of '/usr/local/bin/bake'\n");
    }

    return 0;
error:
    return -1;
}

int16_t bake_setup(
    bool local)
{
    ut_try( ut_mkdir("~/bake"), NULL);

    if (!local) {
        ut_try(bake_create_script(), "failed to create global bake script");
    }

    ut_try(ut_cp("./bake", "~/bake/bake"),
        "failed to copy bake executable to user bake environment");

    ut_try(cmd("bake install --id bake --type application --includes include --build-to-home"),
        "failed to install bake include files");

    ut_try(cmd("bake install --id bake.util --type package --includes util/include --build-to-home"),
        "failed to install bake util include files");

    ut_try(cmd(strarg("make -C drivers/lang/c/build-%s", UT_OS_STRING)),
        "failed to build C driver");

    if (!strcmp(UT_OS_STRING, "darwin")) {
        ut_try (ut_rename("drivers/lang/c/libbake_lang_c.dylib", "drivers/lang/c/libbake_lang_c.so"),
            "failed to rename bake C driver library");
    }

    ut_try(cmd(
      "bake install drivers/lang/c --id bake.lang.c --artefact libbake_lang_c.so --build-to-home"),
        "failed to install bake C driver");

    return 0;
error:
    return -1;
}
