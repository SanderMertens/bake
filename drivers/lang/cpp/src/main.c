
#include <bake>

static
void setup_project(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project)
{
    /* Get short project id */
    const char *id = project->id;
    bake_project_type kind = project->type;
    const char *short_id = project->id_short;

    /* Create directories */
    ut_mkdir("src");
    ut_mkdir("include");

    /* Create main source file */
    char *source_filename = ut_asprintf("src%cmain.cpp", PATH_SEPARATOR_C);
    FILE *f = fopen(source_filename, "w");

    fprintf(f,
        "#include <include/%s.h>\n"
        "\n"
        "int main(int argc, char *argv[]) {\n"
        "    return 0;\n"
        "}\n",
        short_id
    );

    fclose(f);
    free(source_filename);

    /* Create upper-case id for defines in header file */
    char *id_upper = strdup(id);
    strupper(id_upper);
    char *ptr, ch;
    for (ptr = id_upper; (ch = *ptr); ptr ++) {
        if (ch == '/' || ch == '.') {
            ptr[0] = '_';
        }
    }

    /* Create main header file */
    char *header_filename = ut_asprintf("include/%s.h", short_id);
    f = fopen(header_filename, "w");

    fprintf(f,
        "#ifndef %s_H\n"
        "#define %s_H\n\n"
        "/* This generated file contains includes for project dependencies */\n"
        "#include \"bake_config.h\"\n\n",
        id_upper,
        id_upper);

    fprintf(f, "\n" "#endif\n" "\n");

    fclose(f);
}

int bakemain(bake_driver_api *driver) {

    driver->import("lang.c");

    /* Override callback for setting up a project */
    driver->setup(setup_project);

    return 0;
}
