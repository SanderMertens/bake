#include "bake.h"
#include "parson.h"

char *repositories[] = {
    "https://github.com/cortoproject/platform",
    "https://github.com/cortoproject/bake",
    "https://github.com/cortoproject/driver-bake-c",
    "https://github.com/cortoproject/corto",
    NULL
};

static
int16_t bake_setup_cmd(
    char *msg,
    char *cmd)
{
    int8_t ret;
    int sig = corto_proc_cmd(cmd, &ret);
    if (sig || ret) {
        corto_log("#[red]x#[normal] %s\n", msg);
        corto_log(
          "  #[red]%s #[normal](%s %d)\n",
          cmd, sig?"sig":"result", sig?sig:ret);
        return -1;
    } else {
        corto_log("#[green]√#[normal] #[grey]%s#[normal]\n", msg);
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
            corto_log("#[green]√#[normal] #[grey]%s#[normal]\n", repo);
        } else {
            corto_log("#[red]x#[normal] #[grey]%s#[normal]\n", repo);
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

/* Utility to safely add JSON object */
static
JSON_Object* bake_setup_addJsonObject(
    JSON_Object *parent,
    const char *name)
{
    JSON_Object *result = NULL;
    if (json_object_has_value(parent, name)) {
        result = json_object_get_object(parent, name);
        if (!result) {
            corto_log("#[red]x#[normal] invalid JSON: expected '%s' to be a JSON object\n",
                name);
            goto error;
        }
    } else {
        json_object_set_value(parent, name, json_value_init_object());
        result = json_object_get_object(parent, name);
    }
    return result;
error:
    return NULL;
}

static
int16_t bake_setup_addVersion(
    const char *file,
    const char *majorMinor)
{
    JSON_Value *json = json_parse_file(file);
    if (!json) {
        corto_log("#[red]x#[normal] cannot load '%s'\n", file);
        goto error;
    }

    JSON_Object *jsonObj = json_value_get_object(json);
    if (!jsonObj) {
        corto_log("#[red]x#[normal] invalid JSON: expected object in '%s'\n", file);
        goto error;
    }

    JSON_Object *envObj = bake_setup_addJsonObject(jsonObj, "environment");
    if (!envObj) {
        goto error;
    }

    JSON_Object *defaultObj = bake_setup_addJsonObject(envObj, "default");
    if (!envObj) {
        goto error;
    }

    /* Add configuration with existing version */
    const char *existingVersion =
        json_object_get_string(defaultObj, "BAKE_VERSION");
    if (existingVersion && strcmp(majorMinor, existingVersion)) {
        /* Create a new configuration section with existing version */
        JSON_Object *oldVersion = bake_setup_addJsonObject(
            envObj, strarg("v%s", existingVersion));
        if (!oldVersion) {
            goto error;
        }
        const char *str;
        if ((str = json_object_get_string(defaultObj, "BAKE_HOME"))) {
            json_object_set_string(oldVersion, "BAKE_HOME", str);
        }
        if ((str = json_object_get_string(defaultObj, "BAKE_TARGET"))) {
            json_object_set_string(oldVersion, "BAKE_TARGET", str);
        }
        json_object_set_string(oldVersion, "BAKE_VERSION", existingVersion);
        corto_log(
            "#[green]√#[normal] #[grey]create new environment configuration for version '%s'#[normal]\n",
            existingVersion);
    }

    /* Set version of new default section */
    json_object_set_string(defaultObj, "BAKE_VERSION", majorMinor);

    /* Write updated file */
    char *str = json_serialize_to_string_pretty(json);
    char *noEsc = strreplace(str, "\\/", "/");
    free(str);
    FILE *f = fopen(".bake", "w");
    fprintf(f, "%s", noEsc);
    fclose(f);
    free(noEsc);
    json_value_free(json);

    return 0;
error:
    return -1;
}

static
int16_t bake_setup_writeConfig(
    const char *majorMinor)
{
    char *file = corto_envparse("~/.bake/config.json");
    if (!corto_file_test(file)) {
        if (corto_mkdir("~/.bake")) {
            corto_log("#[red]x#[normal] cannot write to '%s': #[red]%s#[normal]\n",
                file, strerror(errno));
            goto error;
        }

        FILE *f = fopen(file, "w");
        if (!f) {
            corto_log("#[red]x#[normal] cannot write to '%s': #[red]%s#[normal]\n",
                file, strerror(errno));
            goto error;
        }

        fprintf(f, "{\n");
        fprintf(f, "    \"configuration\":{\n");
        fprintf(f, "        \"debug\":{\n");
        fprintf(f, "            \"symbols\":true,\n");
        fprintf(f, "            \"debug\":true,\n");
        fprintf(f, "            \"optimizations\":false,\n");
        fprintf(f, "            \"coverage\":false,\n");
        fprintf(f, "            \"strict\":false\n");
        fprintf(f, "        },\n");
        fprintf(f, "        \"release\":{\n");
        fprintf(f, "            \"symbols\":false,\n");
        fprintf(f, "            \"debug\":false,\n");
        fprintf(f, "            \"optimizations\":true,\n");
        fprintf(f, "            \"coverage\":false,\n");
        fprintf(f, "            \"strict\":false\n");
        fprintf(f, "        }\n");
        fprintf(f, "    },\n");
        fprintf(f, "    \"environment\":{\n");
        fprintf(f, "        \"default\":{\n");
        fprintf(f, "            \"BAKE_HOME\":\"~/corto\",\n");
        fprintf(f, "            \"BAKE_TARGET\":\"~/corto\",\n");
        fprintf(f, "            \"BAKE_VERSION\":\"%s\"\n", majorMinor);
        fprintf(f, "        }\n");
        fprintf(f, "    }\n");
        fprintf(f, "}\n");
        fclose(f);

        corto_log(
            "#[green]√#[normal] #[grey]writing '%s'#[normal]\n", file);
    } else {
        corto_log(
            "#[green]√#[normal] #[grey]using existing '%s'#[normal]\n", file);

        if (bake_setup_addVersion(file, majorMinor)) {
            goto error;
        }
    }

    free(file);

    return 0;
error:
    return -1;
}

static
char* bake_setup_getCortoVersion(void)
{
    /* Detect corto version */
    JSON_Value *json = json_parse_file("corto/project.json");
    if (!json) {
        corto_log("#[red]x#[normal] cannot load 'corto/project.json'\n");
        goto error;
    }

    JSON_Object *jsonObj = json_value_get_object(json);
    if (!jsonObj) {
        corto_log("#[red]x#[normal] invalid JSON in 'corto/project.json', expected object\n");
        goto error;
    }

    JSON_Object *value = json_object_get_object(jsonObj, "value");
    if (!value) {
        corto_log("#[red]x#[normal] invalid JSON in 'corto/project.json', missing 'value'\n");
        goto error;
    }

    const char *version = json_object_get_string(value, "version");
    if (!value) {
        corto_log("#[red]x#[normal] invalid JSON in 'corto/project.json', missing 'version'\n");
        goto error;
    }

    char *majorMinor = corto_strdup(version);
    char *dot = strrchr(majorMinor, '.');
    if (!dot) {
        corto_log(
            "#[red]x#[normal] invalid version '%s' (expected major.minor.patch)\n", version);
        goto error;
    } else {
        dot[0] = '\0'; /* Strip patch */
    }
    dot = strchr(majorMinor, '.');
    if (!dot) {
        corto_log(
            "#[red]x#[normal] invalid version '%s' (expected major.minor.patch)\n", version);
        goto error;
    }

    corto_log(
        "#[green]√#[normal] #[grey]discovered corto version %s#[normal]\n", majorMinor);

    json_value_free(json);

    return majorMinor;
error:
    if (json) {
        json_value_free(json);
    }
    return NULL;
}

static
int16_t bake_setup_configure()
{
    char *majorMinor = bake_setup_getCortoVersion();
    if (!majorMinor) {
        goto error;
    }

    if (corto_setenv("BAKE_VERSION", majorMinor)) {
        corto_throw("failed to set $BAKE_VERSION");
        goto error;
    }

    if (corto_setenv("BAKE_PLATFORM", CORTO_PLATFORM_STRING)) {
        corto_throw("failed to set $BAKE_PLATFORM");
        goto error;
    }

    if (corto_setenv("BAKE_CONFIG", "debug")) {
        corto_throw("failed to set $BAKE_CONFIG");
        goto error;
    }

    /* Write default configuration */
    if (bake_setup_writeConfig(majorMinor)) {
        goto error;
    }

    free(majorMinor);

    return 0;
error:
    return -1;
}

/* Linx/Darwin only: create script in location accessible for all users */
int16_t bake_setup_globalScript(void)
{
    FILE *f = fopen ("/usr/local/bin/bake", "w");
    if (!f) {
        corto_log(
      "#[red]x#[normal] cannot open '/usr/local/bin/bake': #[red]%s#[normal]\n",
          strerror(errno));
        corto_log(
            "     try running as 'sudo bake setup', or 'bake setup --local'\n");
        goto error;
    }

    fprintf(f, "exec $HOME/.bake/bake $@\n");
    fclose(f);

    /* Make executable for everyone */
    if (corto_setperm("/usr/local/bin/bake", 0755)) {
        corto_raise();
        corto_log(
      "#[red]x#[normal] failed to set permissions of '/usr/local/bin/bake'\n");
    }

    return 0;
error:
    return -1;
}

int16_t bake_setup(const char *exec, bool local) {
    corto_log_verbositySet(CORTO_ERROR);
    corto_setenv("BAKE_BUILDING", "");

    corto_log("* setting up bake environment\n");

    /* First try to install global scripts, as this is prone to failing when the
     * setup does not have the right permissions. */
    if (!local) {
        corto_log(
      "* writing scripts to '/usr/local', #[yellow]may prompt for password!\n");

        /* Write bake script */
        if (bake_setup_cmd(
            "write bake script to /usr/local",
            strarg("sudo %s install-script", exec)))
        {
            goto error;
        }

        /* Write corto script */
        if (bake_setup_cmd(
            "write corto script to /usr/local",
            strarg("sudo %s install-tool corto", exec)))
        {
            goto error;
        }
    }

    /* If current working directory contains bake executable, move one directory up */
    if (corto_file_test("bake") && !corto_isdir("bake")) {
        if (corto_chdir("..")) {
            corto_throw(NULL);
            goto error;
        }
    }

    corto_log("* checking availability of repositories...\n");

    if (!bake_verify_repos()) {
        corto_log("* all repositories are here, installing bake...\n");
    } else {
        corto_log("* cloning missing repositories...\n");
        if (bake_clone_repos()) {
            goto error;
        }
    }

    if (bake_setup_configure()) {
        goto error;
    }

    /* Copy bake executable to ~/.bake */
    if (corto_mkdir("~/.bake")) {
        corto_raise();
        corto_log(
            "#[red]x#[normal] install bake executable to ~/.bake\n");
        goto error;
    }

    if (corto_cp("bake/bake", "~/.bake/bake")) {
        corto_raise();
        corto_log(
            "#[red]x#[normal] install bake executable to ~/.bake\n");
        goto error;
    }
    corto_log(
        "#[green]√#[normal] #[grey]install bake executable to ~/.bake#[normal]\n");

    if (bake_setup_cmd(
        "install platform include files",
        "bake install platform --id corto --kind package --includes include/corto"))
    { goto error; }

    if (bake_setup_cmd(
        "install bake include files",
        "bake install bake --id bake --kind application --includes include/bake"))
    { goto error; }

    if (bake_setup_cmd(
        "build driver/bake/c",
        strarg("make -C driver-bake-c/build-%s", CORTO_OS_STRING)))
    { goto error; }

    if (!strcmp(CORTO_OS_STRING, "darwin")) {
        if (bake_setup_cmd(
            "rename libdriver_bake_c.dylib to libdriver_bake_c.so",
            "mv driver-bake-c/libdriver_bake_c.dylib driver-bake-c/libdriver_bake_c.so"))
        { goto error; }
    }
    if (bake_setup_cmd(
        "install driver/bake/c binary to package repository",
        "bake install driver-bake-c --id driver/bake/c --kind package --artefact libdriver_bake_c.so"))
    { goto error; }

    corto_log("done!\n");
    return 0;
error:
    corto_log("#[red]x#[normal] failed to install bake :(\n");
    return -1;
}
