#include <bake.h>
#include <bake_amalgamate.h>

BAKE_AMALGAMATE_API 
int bakemain(bake_driver_api *driver);

// If longer, bad
#define MAX_LINE_LENGTH (256) 

static
const char *skip_ws(
    const char *ptr)
{
    while (isspace(*ptr)) {
        ptr ++;
    }
    return ptr;
}

static
int compare_string(
    void *ctx, 
    const void *f1, 
    const void *f2) 
{
    return strcmp(f1, f2);
}

/* Utility for combining file & path */
static
char *combine_path(
    const char *path,
    const char *file)
{
    ut_strbuf path_buf = UT_STRBUF_INIT;
    ut_strbuf_append(&path_buf, "%s/%s", path, file);
    return ut_strbuf_get(&path_buf);
}

/* Get file from include statement */
static
char* parse_include_file(
    const char *line, 
    bool *relative) 
{
    const char *file_start = &line[8];
    file_start = skip_ws(file_start);

    char end = '>';
    if (file_start[0] == '"') {
        *relative = true;
        end = '"';
    }

    file_start ++;
    char *file = ut_strdup(file_start);
    char *file_end = strchr(file, end);
    if (!file_end) {
        ut_error("invalid include '%s'", line);
        goto error;
    }

    *file_end = '\0';

    return file;
error:
    return NULL;
}

/* Amalgamate source file */
static
int amalgamate(
    FILE *out, 
    const char *include_path,
    bool is_include,
    const char *const_file,
    const char *src_file,
    int32_t src_line,
    ut_rb files_parsed,
    time_t *last_modified) 
{
    char *file = ut_strdup(const_file);
    ut_path_clean(file, file);
    if (ut_rb_find(files_parsed, file)) {
        ut_debug("amalgamate: skip   '%s'  (from '%s:%d')", file, 
            src_file, src_line);
        free(file);
        return 0;
    }

    ut_debug("amalgamate: insert '%s' (from '%s:%d')", file,
        src_file, src_line);

    ut_rb_set(files_parsed, file, file);

    /* Get current path from filename (for relative includes) */
    char *cur_path = ut_strdup(file);
    char *last_elem = strrchr(cur_path, '/');
    if (last_elem) {
        *last_elem = '\0';
    }

    /* Buffer used for reading lines */
    char line[MAX_LINE_LENGTH];
    int32_t line_count = 0;

    /* Open file for reading */
    FILE* in = fopen(file, "r");
    if (!in) {
        ut_error("cannot read file '%s'", file);
        goto error;
    }

    time_t modified = ut_lastmodified(file);
    if (modified > last_modified[0]) {
        last_modified[0] = modified;
    }

    while (ut_file_readln(in, line, MAX_LINE_LENGTH) != NULL) {
        line_count ++;

        if (line[0] == '#') {
            if (!strncmp(line, "#include", 8)) {
                bool relative = false;
                char *include = parse_include_file(line, &relative);

                if (!relative) {
                    /* If this is an absolute path, this either refers to a
                     * system include file or to a file in the include folder.
                     * If we are amalgamating source files, we should include
                     * neither. If we are amalgamating include files, we should
                     * only include when the file is in our include folder */
                    char *path = combine_path(include_path, include);
                    if (ut_file_test(path) == 1) {                            
                        /* Only amalgamate if file exists */
                        ut_try(
                            amalgamate(out, include_path, is_include, path, 
                                file, line_count, files_parsed, last_modified), 
                            NULL);
                    } else {
                        /* If file cannot be found in project, include */
                        fprintf(out, "%s", line);
                    }
                    free(path);
                } else {
                    /* If this is a relative path, we should always include it,
                     * if the file can be found. System headers and headers from
                     * the include path can be included with include "". If we
                     * are generating the include file, we have to also try to
                     * find the file in the include path. If we are generating
                     * the source, we need to check if the file existst in the
                     * source path, as if it doesn't, it is a system header we
                     * need to include. */
                    char *path = combine_path(cur_path, include);
                    if (ut_file_test(path) != 1) {
                        /* If we don't find the file in the relative path, look
                         * for the file in the include path, but only if we are
                         * generating the include file. */
                        free(path);
                        path = combine_path(include_path, include);
                        if (ut_file_test(path) != 1) {
                            /* If file cannot be found in project, include */
                            fprintf(out, "%s", line);                           
                            free(path);
                            path = NULL;
                        } else if (!is_include) {
                            /* If we are not generating the include file, we
                             * generally don't want to include files from the
                             * include path. However, it is theoretically 
                             * possible that a file was not included by the main
                             * include file, but is by source files. In that
                             * case it should be amalgamated in the source. This
                             * will not result in duplicate code, as the header
                             * will already have been flagged as included. */
                        }
                    }

                    if (path) {
                        /* Amalgamate this file */
                        ut_try(
                            amalgamate(out, include_path, is_include, path, 
                                file, line_count, files_parsed, last_modified), 
                                NULL);
                        free(path);                        
                    }
                }

                free(include);
                
                continue;
            }
        }

        fprintf(out, "%s", line);
    }

    fprintf(out, "\n"); /* Support for empty files */

    fclose(in);

    return 0;
error:
    return -1;
}

static
void generate(
    bake_driver_api *driver,
    bake_config *config,
    bake_project *project_obj)
{
    if (project_obj->recursive) {
        if (!project_obj->generate_path) {
            return;
        }
    }

    ut_rb files_parsed = ut_rb_new(compare_string, NULL);
    const char *project = project_obj->id_underscore;
    char *project_upper = ut_strdup(project_obj->id_underscore);
    strupper(project_upper);

    const char *project_path = project_obj->path;
    if (!project_path) {
        ut_error("cannot find source for project '%s'", project);
        goto error;
    }

    const char *target_path = project_path;
    if (project_obj->generate_path) {
        target_path = project_obj->generate_path;
    }

    /* Create output file strings & check when they were last modified */
    char *include_file_out = ut_asprintf("%s/%s.h", target_path, project);
    char *include_file_tmp = ut_asprintf("%s/%s.h.tmp", target_path, project);
    time_t include_modified = 0;
    if (ut_file_test(include_file_out) == 1) {
        include_modified = ut_lastmodified(include_file_out);
    }

    char *src_file_out = ut_asprintf("%s/%s.c", target_path, project);
    char *src_file_tmp = ut_asprintf("%s/%s.c.tmp", target_path, project);
    time_t src_modified = 0;
    if (ut_file_test(src_file_out) == 1) {
        src_modified = ut_lastmodified(src_file_out);
    }

    time_t project_modified = 0;

    /* -- Amalgamate include files -- */
    char *include_path = combine_path(project_path, "include");
    char *include_file = ut_asprintf("%s/%s.h", include_path, project);

    if (ut_file_test(include_file) != 1) {
        ut_error("cannot find include file '%s'", include_file);
        goto error;
    }

    /* Create amalgamated header file */
    FILE *include_out = fopen(include_file_tmp, "w");
    if (!include_out) {
        ut_error("cannot open output file '%s'", include_file_out);
        goto error;
    }

    /* If file is embedded, the code should behave like a static library */
    fprintf(include_out, "// Comment out this line when using as DLL\n");
    fprintf(include_out, "#define %s_STATIC\n", project_obj->id_underscore);
    ut_try(amalgamate(include_out, include_path, true, include_file, 
        "(main header)", 0, files_parsed, &project_modified), NULL);
    fclose(include_out);

    /* -- Amalgamate source files -- */
    char *src_path = combine_path(project_path, "src");

    /* Create amalgamated source file */
    FILE *src_out = fopen(src_file_tmp, "w");
    if (!src_out) {
        ut_error("cannot open output file '%s'", include_file_out);
        goto error;
    }

    /* If specified, include main header file */
    fprintf(src_out, "#ifndef %s_IMPL\n", project_upper);
    fprintf(src_out, "#include \"%s.h\"\n", project);
    fprintf(src_out, "#endif\n");

    ut_iter it;
    ut_try(ut_dir_iter(src_path, "//*.c,*.cpp", &it), NULL);

    /* Recursively iterate sources, append to source file */
    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *file_path = combine_path(src_path, file);
        ut_try(amalgamate(src_out, include_path, false, file_path, 
            "(main source)", 0, files_parsed, &project_modified), NULL);
        free(file_path);
    } 
    fclose(src_out);

    if (project_modified > src_modified || project_modified > include_modified){
        ut_rename(src_file_tmp, src_file_out);
        ut_rename(include_file_tmp, include_file_out);
    } else {
        ut_rm(src_file_tmp);
        ut_rm(include_file_tmp);
    }

    ut_rb_free(files_parsed);

    free(include_file_out);
    free(include_file_tmp);
    free(src_file_out);
    free(src_file_tmp);
    free(src_path);
    free(include_path);

    return;
error:
    project_obj->error = true;
}

int bakemain(bake_driver_api *driver) {
    ut_init("bake.amalgamate");
    driver->generate(generate);
    return 0;
}
