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
    const char *file_start = &line[7];
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
    const char *project_id,
    FILE *out, 
    const char *include_path,
    bool is_include,
    const char *const_file,
    const char *src_file,
    int32_t src_line,
    ut_rb files_parsed,
    time_t *last_modified,
    bool *main_included) 
{
    char *file = strreplace(const_file, "/", UT_OS_PS);
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
    char *last_elem = strrchr(cur_path, UT_OS_PS[0]);
    if (last_elem) {
        *last_elem = '\0';
        last_elem ++;
    } else {
        last_elem = cur_path;
    }

    /* Check if current file is a bake_config.h. If it is, replace the <> 
     * includes it contains with "" */
    bool bake_config_h = !strcmp(last_elem, "bake_config.h");

    /* Buffer used for reading lines */
    char line[MAX_LINE_LENGTH];
    int32_t line_count = 0;

    /* Open file for reading */
    FILE* in = ut_file_open(file, "r");
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
            const char *pp_start = skip_ws(line + 1);

            if (!strncmp(pp_start, "include", 7)) {
                if (!is_include && !main_included[0]) {
                    /* If this is the first include of the source file, add
                     * include statement for main header */
                    fprintf(out, "#include \"%s.h\"\n", project_id);
                    main_included[0] = true;
                }

                bool relative = false;
                char *include = parse_include_file(pp_start, &relative);

                if (!relative) {
                    /* If this is in the bake_config.h file, replace the 
                     * statement with one that uses "". Assume that the file
                     * exists, as bake may still be generating it. */
                    if (bake_config_h) {
                        fprintf(out, "#include \"%s\"\n", include);
                    } else {
                        /* If this is an absolute path, this either refers to a
                        * system include file or to a file in the include folder.
                        * If we are amalgamating source files, we should include
                        * neither. If we are amalgamating include files, we should
                        * only include when the file is in our include folder */
                        char *path = combine_path(include_path, include);
                        if (ut_file_test(path) == 1) {                            
                            /* Only amalgamate if file exists */
                            ut_try(
                                amalgamate(project_id, out, include_path, is_include, path, 
                                    file, line_count, files_parsed, last_modified, main_included), 
                                NULL);
                        } else {
                            /* If file cannot be found in project, include */
                            fprintf(out, "%s", line);
                        }
                        free(path);
                    }
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
                            amalgamate(project_id, out, include_path, is_include, path, 
                                file, line_count, files_parsed, last_modified, main_included), 
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

    ut_file_close(in);

    return 0;
error:
    return -1;
}

/* Find project main source file, if there is any */
static
char *find_main_src_file(
    bake_project *project,
    const char *src_path)
{
    /* Try main.cxx */
    char *main_src_file = ut_asprintf(
        "%s/main.%s", src_path, project->language);
    if (ut_file_test(main_src_file) != 1) {
        free(main_src_file);
        main_src_file = NULL;
    }

    /* Try project_full_name.cxx */
    if (main_src_file == NULL) {
        main_src_file = ut_asprintf(
            "%s/%s.%s", src_path, project->id_underscore, 
            project->language);
    }
    if (ut_file_test(main_src_file) != 1) {
        free(main_src_file);
        main_src_file = NULL;
    }

    /* Try name.cxx */
    if (main_src_file == NULL) {
        main_src_file = ut_asprintf(
            "%s/%s.%s", src_path, project->id_base, 
            project->language);
    }
    if (ut_file_test(main_src_file) != 1) {
        free(main_src_file);
        main_src_file = NULL;
    }

    return main_src_file;
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

    /* In case project contains Objective C files */
    char *m_file_out = ut_asprintf("%s/%s_objc.m", target_path, project);
    char *m_file_tmp = ut_asprintf("%s/%s_objc.m.tmp", target_path, project);
    time_t m_modified = 0;
    if (ut_file_test(m_file_out) == 1) {
        m_modified = ut_lastmodified(m_file_out);
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
    ut_try(amalgamate(project, include_out, include_path, true, include_file, 
        "(main header)", 0, files_parsed, &project_modified, 0), NULL);
    fclose(include_out);

    /* -- Amalgamate source files -- */
    char *src_path = combine_path(project_path, "src");

    /* Create amalgamated source file */
    FILE *src_out = fopen(src_file_tmp, "w");
    if (!src_out) {
        ut_error("cannot open output file '%s'", include_file_out);
        goto error;
    }

    /* The main header is included in place of the first #include statement
     * found. This keeps any macro's defined before including a file intact */
    bool main_included = false;

    /* Try to find a file with the name "main.lang" or the name of the project.
     * If a project contains a file with that name, process it first. This gives
     * the project an opportunity to control how headers are included by its own
     * files, as each header is only appended once. */
    char *main_src_file = find_main_src_file(project_obj, src_path);
    if (main_src_file) {
        ut_try(amalgamate(project, src_out, include_path, false, main_src_file, 
            "(main source)", 0, files_parsed, &project_modified, 
            &main_included), NULL);
    }

    /* Recursively iterate sources, append to source file */
    ut_iter it;
    ut_try(ut_dir_iter(src_path, "//*.c,*.cpp", &it), NULL);
    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *file_path = combine_path(src_path, file);

        if (!main_src_file || strcmp(file_path, main_src_file)) {
            ut_try(amalgamate(project, src_out, include_path, false, file_path, 
                "(main source)", 0, files_parsed, &project_modified, 
                &main_included), NULL);
        }

        free(file_path);
    } 
    fclose(src_out);
    free(main_src_file);

    /* Objective C output is only created when necessary */
    FILE *m_out = NULL;

    /* Start with fresh parsed files list */
    ut_rb objc_files_parsed = NULL;

    /* Recursively iterate objective-C sources, append to source file */
    ut_try(ut_dir_iter(src_path, "//*.m", &it), NULL);
    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *file_path = combine_path(src_path, file);

        if (!m_out) {
            m_out = fopen(m_file_tmp, "w");
            if (!m_out) {
                ut_error("cannot open output file '%s'", m_file_tmp);
                goto error;
            }

            objc_files_parsed = ut_rb_new(compare_string, NULL);
        }

        ut_try(amalgamate(project, m_out, include_path, false, file_path, 
            "(main obj-C source)", 0, objc_files_parsed, &project_modified, 
            &main_included), NULL);
    }
    if (m_out) {
        fclose(m_out);
    }

    /* Timestamps were checked while amalgamating. Only replace files with new
     * result if we found that inputs were newer. This ensures we won't end up
     * rebuilding amalgamated file on every build. */
    if (project_modified > src_modified || project_modified > include_modified){
        ut_rename(src_file_tmp, src_file_out);
        ut_rename(include_file_tmp, include_file_out);
    } else {
        ut_rm(src_file_tmp);
        ut_rm(include_file_tmp);
    }

    if (m_out && project_modified > m_modified) {
        ut_rename(m_file_tmp, m_file_out);
    } else {
        ut_rm(m_file_tmp);
    }

    free(include_file_out);
    free(include_file_tmp);
    free(src_file_out);
    free(src_file_tmp);
    free(src_path);
    free(include_path);

    ut_rb_free(files_parsed);
    ut_rb_free(objc_files_parsed);

    return;
error:
    project_obj->error = true;
}

int bakemain(bake_driver_api *driver) {
    ut_init("bake.amalgamate");
    driver->generate(generate);
    return 0;
}
