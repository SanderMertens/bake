#include <bake.h>
#include <bake_amalgamate.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

BAKE_AMALGAMATE_API
int bakemain(bake_driver_api *driver);

#define MAX_LINE_LENGTH (4096)
#define MAX_COND (256)

static
const char *skip_ws(
    const char *ptr)
{
    while (ptr && *ptr && isspace((unsigned char)*ptr)) {
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
    ut_strbuf_append(&path_buf, "%s" UT_OS_PS "%s", path, file);
    return ut_strbuf_get(&path_buf);
}

static
bool is_ident_char(
    char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

/* Get file from include statement. Returns NULL if line is not an #include. */
static
char* parse_include_file(
    const char *line,
    bool *relative)
{
    *relative = false;

    const char *p = skip_ws(line);
    if (!p || p[0] != '#') {
        return NULL;
    }

    p = skip_ws(p + 1);
    if (strncmp(p, "include", 7)) {
        return NULL;
    }

    p = skip_ws(p + 7);
    if (!p || !p[0]) {
        return NULL;
    }

    char end = '>';
    if (p[0] == '"') {
        end = '"';
        *relative = true;
    } else if (p[0] != '<') {
        return NULL;
    }

    p ++;
    const char *end_ptr = strchr(p, end);
    if (!end_ptr) {
        return NULL;
    }

    size_t len = (size_t)(end_ptr - p);
    char *out = malloc(len + 1);
    memcpy(out, p, len);
    out[len] = '\0';
    return out;
}

/* -- Disable-flag (conditional compilation) handling -- */

typedef enum cpp_directive_kind {
    CPP_NONE,
    CPP_IF,
    CPP_IFDEF,
    CPP_IFNDEF,
    CPP_ELIF,
    CPP_ELSE,
    CPP_ENDIF
} cpp_directive_kind;

typedef struct cond_frame {
    bool managed;
    bool emit;
    bool taken;
} cond_frame;

static
cpp_directive_kind parse_cpp_directive(
    const char *line,
    const char **arg_out)
{
    *arg_out = NULL;

    const char *p = skip_ws(line);
    if (!p || p[0] != '#') {
        return CPP_NONE;
    }

    p = skip_ws(p + 1);
    const char *word = p;
    while (is_ident_char(*p)) {
        p ++;
    }

    size_t len = (size_t)(p - word);
    cpp_directive_kind kind = CPP_NONE;
    if (len == 2 && !strncmp(word, "if", 2)) {
        kind = CPP_IF;
    } else if (len == 5 && !strncmp(word, "ifdef", 5)) {
        kind = CPP_IFDEF;
    } else if (len == 6 && !strncmp(word, "ifndef", 6)) {
        kind = CPP_IFNDEF;
    } else if (len == 4 && !strncmp(word, "elif", 4)) {
        kind = CPP_ELIF;
    } else if (len == 4 && !strncmp(word, "else", 4)) {
        kind = CPP_ELSE;
    } else if (len == 5 && !strncmp(word, "endif", 5)) {
        kind = CPP_ENDIF;
    }

    if (kind != CPP_NONE) {
        *arg_out = p;
    }
    return kind;
}

static
bool disable_contains(
    ut_ll disable,
    const char *name,
    size_t len)
{
    if (!disable || !len) {
        return false;
    }
    ut_iter it = ut_ll_iter(disable);
    while (ut_iter_hasNext(&it)) {
        const char *flag = ut_iter_next(&it);
        if (!strncmp(flag, name, len) && !flag[len]) {
            return true;
        }
    }
    return false;
}

static
bool arg_is_disabled_macro(
    const char *arg,
    ut_ll disable)
{
    const char *p = skip_ws(arg);
    const char *name = p;
    while (is_ident_char(*p)) {
        p ++;
    }
    return disable_contains(disable, name, (size_t)(p - name));
}

static
bool is_disabled_define(
    const char *line,
    ut_ll disable)
{
    const char *p = skip_ws(line);
    if (p[0] != '#') {
        return false;
    }
    p = skip_ws(p + 1);
    if (strncmp(p, "define", 6) || is_ident_char(p[6])) {
        return false;
    }
    p = skip_ws(p + 6);
    const char *name = p;
    while (is_ident_char(*p)) {
        p ++;
    }
    if (p[0] == '(') {
        return false;
    }
    return disable_contains(disable, name, (size_t)(p - name));
}

static
const char* parse_defined_term(
    const char *expr,
    ut_ll disable,
    bool *known_out,
    bool *value_out)
{
    const char *p = skip_ws(expr);
    bool negate = false;
    if (p[0] == '!') {
        negate = true;
        p = skip_ws(p + 1);
    }

    if (strncmp(p, "defined", 7) || is_ident_char(p[7])) {
        return NULL;
    }
    p += 7;

    bool had_paren = false;
    p = skip_ws(p);
    if (p[0] == '(') {
        had_paren = true;
        p = skip_ws(p + 1);
    } else if (!is_ident_char(p[0])) {
        return NULL;
    }

    const char *name = p;
    while (is_ident_char(*p)) {
        p ++;
    }
    size_t name_len = (size_t)(p - name);

    p = skip_ws(p);
    if (had_paren) {
        if (p[0] != ')') {
            return NULL;
        }
        p = skip_ws(p + 1);
    }

    *known_out = disable_contains(disable, name, name_len);
    *value_out = negate;
    return p;
}

static
bool eval_defined_expr(
    const char *expr,
    ut_ll disable,
    bool *value_out)
{
    const char *p = expr;
    char op = '\0';
    bool has_true = false, has_false = false, all_known = true;

    while (true) {
        bool known = false, value = false;
        p = parse_defined_term(p, disable, &known, &value);
        if (!p) {
            return false;
        }

        if (!known) {
            all_known = false;
        } else if (value) {
            has_true = true;
        } else {
            has_false = true;
        }

        if (p[0] == '\0' || p[0] == '\n' || p[0] == '\r') {
            break;
        }

        if ((p[0] != '&' && p[0] != '|') || p[1] != p[0]) {
            return false;
        }

        if (op && op != p[0]) {
            return false;
        }

        op = p[0];
        p += 2;
    }

    bool is_or = op == '|';
    if (is_or ? has_true : has_false) {
        *value_out = is_or;
        return true;
    }

    if (all_known) {
        *value_out = !is_or;
        return true;
    }

    return false;
}

static
char* condition_text(
    const char *arg)
{
    const char *start = skip_ws(arg);
    size_t len = strlen(start);
    char *text = malloc(len + 1);
    memcpy(text, start, len);
    text[len] = '\0';

    char *p;
    for (p = text; *p; p ++) {
        if (p[0] == '/' && (p[1] == '/' || p[1] == '*')) {
            *p = '\0';
            break;
        }
        if (p[0] == '\n' || p[0] == '\r') {
            *p = '\0';
            break;
        }
    }

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *(--end) = '\0';
    }

    return text;
}

static
bool cond_eval_managed(
    cpp_directive_kind kind,
    const char *arg,
    ut_ll disable,
    bool *body_emit_out)
{
    if (kind == CPP_IFDEF) {
        if (arg_is_disabled_macro(arg, disable)) {
            *body_emit_out = false;
            return true;
        }
        return false;
    }

    if (kind == CPP_IFNDEF) {
        if (arg_is_disabled_macro(arg, disable)) {
            *body_emit_out = true;
            return true;
        }
        return false;
    }

    if (kind == CPP_IF) {
        char *cond = condition_text(arg);
        bool value = false;
        bool managed = eval_defined_expr(cond, disable, &value);
        free(cond);
        if (managed) {
            *body_emit_out = value;
            return true;
        }
        return false;
    }

    return false;
}

/* Records a verbatim (non-inlined) include and reports whether it should be
 * emitted. Duplicate includes are dropped only at the top level of a
 * translation unit (if_depth 0); includes guarded by #if are always kept since
 * the guard may select between alternatives. emitted is NULL for outputs that
 * are not deduplicated (e.g. the amalgamated header). */
static
bool mark_verbatim_include(
    ut_rb emitted,
    int32_t if_depth,
    const char *include,
    bool relative)
{
    if (!emitted || if_depth != 0) {
        return true;
    }

    char *key = ut_asprintf("%c%s", relative ? '"' : '<', include);
    if (ut_rb_find(emitted, key)) {
        free(key);
        return false;
    }

    ut_rb_set(emitted, key, key);
    return true;
}

/* Amalgamate source file */
static
int amalgamate(
    const char *include_name,
    FILE *out,
    const char *include_path,
    bool is_include,
    const char *const_file,
    const char *src_file,
    int32_t src_line,
    ut_rb files_parsed,
    ut_ll disable,
    bool *main_included,
    ut_rb emitted_includes,
    int32_t if_depth_in)
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

    /* Open file for reading */
    FILE* in = fopen(file, "rb");
    if (!in) {
        ut_error("cannot read file '%s'", file);
        goto error;
    }

    int32_t line_count = 0;
    bool in_block_comment = false;
    bool has_disable = disable && ut_ll_count(disable) > 0;
    cond_frame cond_stack[MAX_COND];
    int32_t cond_depth = 0;
    int32_t suppressed = 0;
    int32_t if_depth = if_depth_in;

    while (fgets(line, MAX_LINE_LENGTH, in) != NULL) {
        line_count ++;

        /* Track whether the line starts inside a block comment, and update the
         * comment state for the next line. This prevents #include and other
         * directives inside comments from being processed. */
        bool line_in_block_comment_at_start = in_block_comment;
        const char *scan = line;
        while (*scan) {
            if (in_block_comment) {
                if (scan[0] == '*' && scan[1] == '/') {
                    in_block_comment = false;
                    scan += 2;
                    continue;
                }
                scan ++;
                continue;
            }
            if (scan[0] == '/' && scan[1] == '*') {
                in_block_comment = true;
                scan += 2;
                continue;
            }
            scan ++;
        }

        /* Track preprocessor conditional nesting so verbatim includes can be
         * deduplicated only when they sit at the top level of the unit. */
        if (!line_in_block_comment_at_start) {
            const char *depth_arg = NULL;
            cpp_directive_kind depth_dir =
                parse_cpp_directive(line, &depth_arg);
            if (depth_dir == CPP_IF ||
                depth_dir == CPP_IFDEF ||
                depth_dir == CPP_IFNDEF)
            {
                if_depth ++;
            } else if (depth_dir == CPP_ENDIF && if_depth > 0) {
                if_depth --;
            }
        }

        /* Handle conditional directives for disabled flags */
        if (has_disable && !line_in_block_comment_at_start) {
            const char *arg = NULL;
            cpp_directive_kind directive = parse_cpp_directive(line, &arg);

            if (directive == CPP_IF ||
                directive == CPP_IFDEF ||
                directive == CPP_IFNDEF)
            {
                if (cond_depth >= MAX_COND) {
                    ut_error("preprocessor nesting too deep in '%s'", file);
                    goto error_close;
                }

                bool body_emit = true;
                bool managed = cond_eval_managed(
                    directive, arg, disable, &body_emit);

                cond_frame *frame = &cond_stack[cond_depth ++];
                frame->managed = managed;
                if (managed) {
                    frame->emit = body_emit;
                    frame->taken = body_emit;
                    if (!body_emit) {
                        suppressed ++;
                    }
                } else {
                    frame->emit = true;
                    frame->taken = false;
                    if (suppressed == 0) {
                        fprintf(out, "%s", line);
                    }
                }
                continue;
            }

            if (directive == CPP_ELSE && cond_depth > 0) {
                cond_frame *frame = &cond_stack[cond_depth - 1];
                if (frame->managed) {
                    if (frame->taken) {
                        if (frame->emit) {
                            frame->emit = false;
                            suppressed ++;
                        }
                    } else {
                        if (!frame->emit) {
                            suppressed --;
                        }
                        frame->emit = true;
                        frame->taken = true;
                    }
                } else if (suppressed == 0) {
                    fprintf(out, "%s", line);
                }
                continue;
            }

            if (directive == CPP_ELIF && cond_depth > 0) {
                cond_frame *frame = &cond_stack[cond_depth - 1];
                if (frame->managed) {
                    if (frame->taken) {
                        if (frame->emit) {
                            frame->emit = false;
                            suppressed ++;
                        }
                    } else {
                        if (!frame->emit) {
                            suppressed --;
                        }
                        frame->emit = true;
                        frame->managed = false;
                        if (suppressed == 0) {
                            fprintf(out, "#if%s", arg);
                        }
                    }
                } else if (suppressed == 0) {
                    fprintf(out, "%s", line);
                }
                continue;
            }

            if (directive == CPP_ENDIF && cond_depth > 0) {
                cond_frame *frame = &cond_stack[-- cond_depth];
                if (frame->managed) {
                    if (!frame->emit) {
                        suppressed --;
                    }
                } else if (suppressed == 0) {
                    fprintf(out, "%s", line);
                }
                continue;
            }
        }

        if (suppressed > 0) {
            continue;
        }

        /* Drop the enable "#define FLAG" for a disabled flag so the flag is
         * genuinely undefined for the compiler. */
        if (has_disable && !line_in_block_comment_at_start &&
            is_disabled_define(line, disable))
        {
            continue;
        }

        bool relative = false;
        char *include = NULL;
        if (!line_in_block_comment_at_start) {
            include = parse_include_file(line, &relative);
        }

        if (!include) {
            fprintf(out, "%s", line);
            continue;
        }

        if (!is_include && main_included && !main_included[0]) {
            /* If this is the first include of the source file, add include
             * statement for main header */
            fprintf(out, "#include \"%s.h\"\n", include_name);
            main_included[0] = true;
        }

        bool recurse = false;
        char *path = NULL;

        if (!relative) {
            /* If this is in the bake_config.h file, replace the statement with
             * one that uses "". Assume that the file exists, as bake may still
             * be generating it. */
            if (bake_config_h) {
                fprintf(out, "#include \"%s\"\n", include);
                free(include);
                continue;
            }

            /* If this is an absolute path, this either refers to a system
             * include file or to a file in the include folder. Only amalgamate
             * when the file is in our include folder. */
            path = combine_path(include_path, include);
            if (ut_file_test(path) == 1) {
                recurse = true;
            }
        } else {
            /* Relative path: search relative to current file, then in the
             * include path. */
            path = combine_path(cur_path, include);
            if (ut_file_test(path) != 1) {
                free(path);
                path = combine_path(include_path, include);
                if (ut_file_test(path) == 1) {
                    recurse = true;
                } else {
                    free(path);
                    path = NULL;
                }
            } else {
                recurse = true;
            }
        }

        if (recurse) {
            ut_try( amalgamate(include_name, out, include_path, is_include, path,
                file, line_count, files_parsed, disable, main_included,
                emitted_includes, if_depth), NULL);
        } else if (mark_verbatim_include(
            emitted_includes, if_depth, include, relative))
        {
            fprintf(out, "%s", line);
        }

        free(path);
        free(include);
    }

    fprintf(out, "\n"); /* Support for empty files */

    fclose(in);

    return 0;
error_close:
    fclose(in);
error:
    free(cur_path);
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

// Sort file paths by directory depth, then alphabetically
static
int file_path_compare(
    const void *ptr1,
    const void *ptr2)
{
    const char *path1 = *((char**)ptr1);
    const char *path2 = *((char**)ptr2);

    int32_t depth1 = 0, depth2 = 0;
    const char *ptr = path1;
    while ((ptr = strchr(ptr, UT_OS_PS[0]))) {
        depth1 ++;
        ptr ++;
    }
    ptr = path2;
    while ((ptr = strchr(ptr, UT_OS_PS[0]))) {
        depth2 ++;
        ptr ++;
    }

    if (depth1 != depth2) {
        // Sort by the shortest path first
        return depth1 - depth2;
    }

    return strcmp(path1, path2);
}

/* -- Output cleanup: drop @file comment blocks, collapse blank line runs -- */

static
bool comment_has_file_directive(
    const char *start,
    const char *end)
{
    const char *q;
    for (q = start; q + 5 <= end; q ++) {
        if ((q[0] == '@' || q[0] == '\\') &&
            q[1] == 'f' && q[2] == 'i' && q[3] == 'l' && q[4] == 'e')
        {
            return true;
        }
    }
    return false;
}

static
char* clean_amalgamation(
    const char *in,
    size_t in_len)
{
    char *out = malloc(in_len + 1);
    if (!out) {
        return NULL;
    }

    size_t w = 0;
    int newline_run = 2;
    const char *p = in;
    const char *in_end = in + in_len;

    while (*p) {
        char c = *p;

        if (c == '"' || c == '\'') {
            char quote = c;
            out[w ++] = *p ++;
            while (*p) {
                if (*p == '\\' && p[1]) {
                    out[w ++] = *p ++;
                    out[w ++] = *p ++;
                    continue;
                }
                char s = *p;
                out[w ++] = *p ++;
                if (s == quote) {
                    break;
                }
            }
            newline_run = 0;
            continue;
        }

        if (c == '/' && p[1] == '/') {
            while (*p && *p != '\n') {
                out[w ++] = *p ++;
            }
            newline_run = 0;
            continue;
        }

        if (c == '/' && p[1] == '*') {
            const char *end = strstr(p + 2, "*/");
            const char *comment_end = end ? end + 2 : in_end;
            if (comment_has_file_directive(p, comment_end)) {
                p = comment_end;
                continue;
            }
            while (p < comment_end) {
                out[w ++] = *p ++;
            }
            newline_run = 0;
            continue;
        }

        if (c == '\n') {
            if (newline_run >= 2) {
                p ++;
                continue;
            }
            out[w ++] = '\n';
            newline_run ++;
            p ++;
            continue;
        }

        out[w ++] = c;
        newline_run = 0;
        p ++;
    }

    out[w] = '\0';
    return out;
}

static
char* read_file(
    const char *path,
    size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)n + 1);
    size_t rd = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[rd] = '\0';
    if (len_out) {
        *len_out = rd;
    }
    return buf;
}

/* Clean tmp file, write to out file only if content changed, remove tmp */
static
int finalize_amalgamation(
    const char *tmp_file,
    const char *out_file)
{
    size_t len = 0;
    char *raw = read_file(tmp_file, &len);
    ut_rm(tmp_file);
    if (!raw) {
        return -1;
    }

    char *cleaned = clean_amalgamation(raw, len);
    free(raw);
    if (!cleaned) {
        return -1;
    }

    size_t cleaned_len = strlen(cleaned);
    size_t old_len = 0;
    char *old = read_file(out_file, &old_len);
    bool unchanged = old && old_len == cleaned_len &&
        !memcmp(old, cleaned, cleaned_len);
    free(old);

    if (!unchanged) {
        FILE *f = fopen(out_file, "wb");
        if (!f) {
            free(cleaned);
            return -1;
        }
        fwrite(cleaned, 1, cleaned_len, f);
        fclose(f);
    }

    free(cleaned);
    return 0;
}

/* Generate a single amalgamation (one entry in the amalgamate config list) */
static
int generate_one(
    bake_project *project_obj,
    const char *project_id,
    const char *include_path,
    const char *src_path,
    const char *include_file,
    const char *target_path,
    bake_amalgamate_config *cfg)
{
    int result = -1;
    ut_ll disable = cfg->disable_flags;
    const char *output_base =
        (cfg->prefix && cfg->prefix[0]) ? cfg->prefix : project_id;
    const char *src_ext = "c";
    if (project_obj->language &&
        (!strcmp(project_obj->language, "cpp") ||
         !strcmp(project_obj->language, "c++")))
    {
        src_ext = "cpp";
    }

    char *output_path;
    if (cfg->path && cfg->path[0]) {
        output_path = ut_asprintf("%s" UT_OS_PS "%s", target_path, cfg->path);
        ut_mkdir(output_path);
    } else {
        output_path = ut_strdup(target_path);
    }

    char *include_file_out = ut_asprintf("%s/%s.h", output_path, output_base);
    char *include_file_tmp = ut_asprintf("%s/%s.h.tmp", output_path, output_base);
    char *src_file_out = ut_asprintf("%s/%s.%s", output_path, output_base, src_ext);
    char *src_file_tmp = ut_asprintf("%s/%s.%s.tmp", output_path, output_base, src_ext);
    char *m_file_out = ut_asprintf("%s/%s_objc.m", output_path, output_base);
    char *m_file_tmp = ut_asprintf("%s/%s_objc.m.tmp", output_path, output_base);

    ut_rb files_parsed = ut_rb_new(compare_string, NULL);
    ut_rb emitted = ut_rb_new(compare_string, NULL);
    ut_rb objc_emitted = NULL;

    /* -- Amalgamate header -- */
    FILE *include_out = fopen(include_file_tmp, "wb");
    if (!include_out) {
        ut_error("cannot open output file '%s'", include_file_tmp);
        goto error;
    }

    fprintf(include_out, "// Comment out this line when using as DLL\n");
    fprintf(include_out, "#define %s_STATIC\n", project_id);
    ut_try( amalgamate(output_base, include_out, include_path, true,
        include_file, "(main header)", 0, files_parsed, disable, NULL,
        NULL, 0), NULL);
    fclose(include_out);

    /* -- Amalgamate source -- */
    FILE *src_out = fopen(src_file_tmp, "wb");
    if (!src_out) {
        ut_error("cannot open output file '%s'", src_file_tmp);
        goto error;
    }

    bool main_included = false;

    char *main_src_file = find_main_src_file(project_obj, src_path);
    if (main_src_file) {
        ut_try( amalgamate(output_base, src_out, include_path, false,
            main_src_file, "(main source)", 0, files_parsed, disable,
            &main_included, emitted, 0), NULL);
    }

    /* Recursively iterate sources and store the paths */
    ut_ll source_files = ut_ll_new();
    ut_iter it;
    ut_try( ut_dir_iter(src_path, "//*.c,*.cpp", &it), NULL);
    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *file_path = combine_path(src_path, file);
        ut_ll_append(source_files, file_path);
    }

    uint32_t count = ut_ll_count(source_files);
    char **buffer = malloc(sizeof(char*) * (count ? count : 1));
    uint32_t i = 0, x;
    it = ut_ll_iter(source_files);
    while (ut_iter_hasNext(&it)) {
        buffer[i ++] = ut_iter_next(&it);
    }
    ut_ll_free(source_files);

    qsort(buffer, i, sizeof(char*), file_path_compare);

    for (x = 0; x < i; x ++) {
        char *file_path = buffer[x];
        if (!main_src_file || strcmp(file_path, main_src_file)) {
            ut_try( amalgamate(output_base, src_out, include_path, false,
                file_path, "(source)", 0, files_parsed, disable,
                &main_included, emitted, 0), NULL);
        }
        free(file_path);
    }
    free(buffer);

    if (!main_included) {
        fprintf(src_out, "#include \"%s.h\"\n", output_base);
    }
    fclose(src_out);
    free(main_src_file);

    /* -- Amalgamate Objective-C sources (only if any exist) -- */
    FILE *m_out = NULL;
    ut_rb objc_files_parsed = NULL;
    bool m_created = false;
    ut_try( ut_dir_iter(src_path, "//*.m", &it), NULL);
    while (ut_iter_hasNext(&it)) {
        char *file = ut_iter_next(&it);
        char *file_path = combine_path(src_path, file);

        if (!m_out) {
            m_out = fopen(m_file_tmp, "wb");
            if (!m_out) {
                ut_error("cannot open output file '%s'", m_file_tmp);
                free(file_path);
                goto error;
            }
            objc_files_parsed = ut_rb_new(compare_string, NULL);
            objc_emitted = ut_rb_new(compare_string, NULL);
            m_created = true;
        }

        ut_try( amalgamate(output_base, m_out, include_path, false,
            file_path, "(obj-C source)", 0, objc_files_parsed, disable,
            &main_included, objc_emitted, 0), NULL);

        free(file_path);
    }
    if (m_out) {
        fclose(m_out);
    }

    ut_rb_free(files_parsed);
    ut_rb_free(emitted);
    if (objc_files_parsed) {
        ut_rb_free(objc_files_parsed);
    }
    if (objc_emitted) {
        ut_rb_free(objc_emitted);
    }

    /* Clean output & write only when content changed */
    ut_try( finalize_amalgamation(include_file_tmp, include_file_out), NULL);
    ut_try( finalize_amalgamation(src_file_tmp, src_file_out), NULL);
    if (m_created) {
        ut_try( finalize_amalgamation(m_file_tmp, m_file_out), NULL);
    }

    result = 0;
error:
    free(include_file_out);
    free(include_file_tmp);
    free(src_file_out);
    free(src_file_tmp);
    free(m_file_out);
    free(m_file_tmp);
    free(output_path);
    return result;
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

    const char *project = project_obj->id_underscore;
    const char *project_path = project_obj->path;
    if (!project_path) {
        ut_error("cannot find source for project '%s'", project);
        goto error;
    }

    const char *target_path = project_path;
    if (project_obj->generate_path) {
        target_path = project_obj->generate_path;
    }

    char *include_path = combine_path(project_path, "include");
    char *src_path = combine_path(project_path, "src");
    char *include_file = ut_asprintf("%s/%s.h", include_path, project);

    if (ut_file_test(include_file) != 1) {
        ut_error("cannot find include file '%s'", include_file);
        goto error_free;
    }

    /* Use the configured amalgamation list, or synthesize a single config from
     * the legacy amalgamate-path setting (also used when copying amalgamated
     * sources from dependencies). */
    ut_ll configs = project_obj->amalgamate_configs;
    ut_ll synth_list = NULL;
    bake_amalgamate_config synth;
    if (!configs || !ut_ll_count(configs)) {
        synth.path = project_obj->amalgamate_path;
        synth.prefix = NULL;
        synth.disable_flags = NULL;
        synth_list = ut_ll_new();
        ut_ll_append(synth_list, &synth);
        configs = synth_list;
    }

    ut_iter it = ut_ll_iter(configs);
    while (ut_iter_hasNext(&it)) {
        bake_amalgamate_config *cfg = ut_iter_next(&it);
        if (generate_one(project_obj, project, include_path, src_path,
            include_file, target_path, cfg))
        {
            if (synth_list) {
                ut_ll_free(synth_list);
            }
            goto error_free;
        }
    }

    if (synth_list) {
        ut_ll_free(synth_list);
    }

    free(include_path);
    free(src_path);
    free(include_file);

    return;
error_free:
    free(include_path);
    free(src_path);
    free(include_file);
error:
    project_obj->error = true;
}

int bakemain(bake_driver_api *driver) {
    ut_init("bake.amalgamate");
    driver->generate(generate);
    return 0;
}
