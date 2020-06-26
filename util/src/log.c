/* Copyright (c) 2010-2019 Sander Mertens
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

#include <bake_util.h>

#define UT_LOG_FILE_LEN (20)
#define UT_MAX_LOG (1024)

#ifdef _WIN32
void ut_enable_console_color(int io_handle)
{
    HANDLE hOut = GetStdHandle(io_handle);
    if (hOut == INVALID_HANDLE_VALUE)
        return;

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
        return;

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#endif

ut_log_verbosity ut_logv(
    char const *file,
    unsigned int line,
    char const *function,
    ut_log_verbosity kind,
    unsigned int level,
    const char *fmt,
    va_list arg,
    bool list_defined,
    FILE* f,
    bool overwrite);

/* This global variable is set at startup and a public symbol */
char *ut_log_appName = "";

/* Variables used for synchronization & TLS */
extern ut_mutex_s ut_log_lock;
static ut_tls UT_KEY_LOG = 0;

typedef struct ut_log_handler {
    void *ctx;
    ut_log_handler_cb cb;
} ut_log_handler;

static ut_log_handler log_handler;

/* These global variables are shared across threads and are *not* protected by
 * a mutex. Libraries should not invoke functions that touch these, and an
 * application should set them during startup. */
static ut_log_verbosity UT_LOG_LEVEL = UT_INFO;
static unsigned int UT_LOG_DEPTH = INT_MAX;
static bool UT_LOG_PROFILE = false;
static ut_log_exceptionAction UT_LOG_EXCEPTION_ACTION = 0;
static char *UT_LOG_FMT_APPLICATION;
static char *UT_LOG_FMT_CURRENT = UT_LOG_FORMAT_DEFAULT;
static bool ut_log_shouldEmbedCategories = true;
static bool UT_LOG_USE_COLORS = true;

/* Maximum stacktrace */
#define BACKTRACE_DEPTH 60

UT_EXPORT char* ut_backtraceString(void);
UT_EXPORT void ut_printBacktrace(FILE* f, int nEntries, char** symbols);

/* One frame for each function where ut_throw is called */
typedef struct ut_log_codeframe {
    char *file;
    char *function;
    unsigned int line;
    char *error;
    char *detail;
    bool thrown; /* Is frame thrown or copied from category stack */
} ut_log_codeframe;

/* One frame for each category */
typedef struct ut_log_frame {
    char *category;
    int count;
    bool printed;

    /* The initial frame contains where log_push was called */
    ut_log_codeframe initial;

    /* The frames array contains the seterr calles for the current category */
    ut_log_codeframe frames[UT_MAX_LOG_CODEFRAMES];
    uint32_t sp;
    struct timespec lastTime;
} ut_log_frame;

/* Main thread-specific log administration type */
typedef struct ut_log_tlsData {
    /* Last reported error data */
    char *lastInfo;
    char *exceptionCategories[UT_MAX_LOG_CATEGORIES + 1];
    ut_log_frame exceptionFrames[UT_MAX_LOG_CATEGORIES + 1];
    uint32_t exceptionCount;
    bool viewed;
    char *backtrace;
    uint16_t last_printed_len;

    /* Current category */
    char* categories[UT_MAX_LOG_CATEGORIES + 1];
    ut_log_frame frames[UT_MAX_LOG_CATEGORIES + 1];
    uint32_t sp;

    /* Last reported time (used for computing deltas) */
    struct timespec lastTime;

    /* Detect if program is unwinding stack in case error was reported */
    void *stack_marker;
} ut_log_tlsData;

static
ut_log_tlsData* ut_getThreadData(void){
    ut_log_tlsData* result;
    if (!UT_KEY_LOG) {
        fprintf(
            stderr,
            "*** UT_KEY_LOG not initialized! Run ut_start first ***\n");
        abort();
    }

    result = ut_tls_get(UT_KEY_LOG);
    if (!result) {
        result = ut_calloc(sizeof(ut_log_tlsData));
        ut_tls_set(UT_KEY_LOG, result);
    }
    return result;
}

void ut_log_handlerRegister(
    ut_log_handler_cb callback,
    void *ctx)
{
    log_handler.cb = callback;
    log_handler.ctx = ctx;
}

bool ut_log_handlerRegistered(void) {
    return log_handler.cb != NULL;
}

void ut_err_notifyCallkback(
    ut_log_verbosity level,
    char *msg)
{
    if (log_handler.cb) {
        log_handler.cb(level, msg, log_handler.ctx);
    }
}

void ut_printBacktrace(FILE* f, int nEntries, char** symbols) {
    int i;
    for(i=1; i<nEntries; i++) { /* Skip this function */
        fprintf(f, "  %s\n", symbols[i]);
    }
    fprintf(f, "\n");
}

void ut_backtrace(FILE* f) {
#ifndef _WIN32
    int nEntries;
    void* buff[BACKTRACE_DEPTH];
    char** symbols;

    #ifdef ENABLE_BACKTRACE
    nEntries = backtrace(buff, BACKTRACE_DEPTH);
    if (nEntries) {
        symbols = backtrace_symbols(buff, BACKTRACE_DEPTH);

        ut_printBacktrace(f, nEntries, symbols);

        free(symbols);
    } else {
        fprintf(f, "obtaining backtrace failed.");
    }
    #endif
#else
    void *stack[TRACE_MAX_STACK_FRAMES];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    WORD numberOfFrames = CaptureStackBackTrace(0, TRACE_MAX_STACK_FRAMES, stack, NULL);
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)malloc(sizeof(SYMBOL_INFO) + (TRACE_MAX_FUNCTION_NAME_LENGTH - 1) * sizeof(TCHAR));
    symbol->MaxNameLen = TRACE_MAX_FUNCTION_NAME_LENGTH;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    DWORD displacement;
    IMAGEHLP_LINE64 *line = (IMAGEHLP_LINE64 *)malloc(sizeof(IMAGEHLP_LINE64));
    line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    for (int i = 0; i < numberOfFrames; i++)
    {
        DWORD64 address = (DWORD64)(stack[i]);
        SymFromAddr(process, address, NULL, symbol);
        if (SymGetLineFromAddr64(process, address, &displacement, line))
        {
            fprintf(f, "\tat %s in %s: line: %lu: address: 0x%I64X\n", symbol->Name, line->FileName, line->LineNumber, symbol->Address);
        }
        else
        {
            fprintf(f, "\tSymGetLineFromAddr64 returned error code %lu.\n", GetLastError());
            fprintf(f, "\tat %s, address 0x%I64X.\n", symbol->Name, symbol->Address);
        }
    }
#endif
}

char* ut_backtraceString(void) {
    int nEntries;
    void* buff[BACKTRACE_DEPTH];
    char** symbols;
    char* result;

    result = malloc(10000);
    *result = '\0';

#ifndef _WIN32
    #ifdef ENABLE_BACKTRACE
    nEntries = backtrace(buff, BACKTRACE_DEPTH);
    if (nEntries) {
        symbols = backtrace_symbols(buff, BACKTRACE_DEPTH);

        int i;
        for(i=1; i<nEntries; i++) { /* Skip this function */
            sprintf(result, "%s  %s\n", result, symbols[i]);
        }
        strcat(result, "\n");

        free(symbols);
    } else {
        fprintf(stderr, "obtaining backtrace failed.");
    }
    #else
    result = ut_strdup("backtrace unavailable");
    #endif
#else
    void *stack[TRACE_MAX_STACK_FRAMES];
    HANDLE process = GetCurrentProcess();
    SymInitialize(process, NULL, TRUE);
    WORD numberOfFrames = CaptureStackBackTrace(0, TRACE_MAX_STACK_FRAMES, stack, NULL);
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)malloc(sizeof(SYMBOL_INFO) + (TRACE_MAX_FUNCTION_NAME_LENGTH - 1) * sizeof(TCHAR));
    symbol->MaxNameLen = TRACE_MAX_FUNCTION_NAME_LENGTH;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    DWORD displacement;
    IMAGEHLP_LINE64 *line = (IMAGEHLP_LINE64 *)malloc(sizeof(IMAGEHLP_LINE64));
    line->SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    for (int i = 0; i < numberOfFrames; i++)
    {
        DWORD64 address = (DWORD64)(stack[i]);
        SymFromAddr(process, address, NULL, symbol);
        if (SymGetLineFromAddr64(process, address, &displacement, line))
        {
            sprintf(result, "%s \tat %s in %s: line: %lu: address: 0x%I64X\n", result, symbol->Name, line->FileName, line->LineNumber, symbol->Address);
        }
        else
        {
            sprintf(result, "%s \tSymGetLineFromAddr64 returned error code %lu.\n", result, GetLastError());
            sprintf(result, "%s \tat %s, address 0x%I64X.\n", result, symbol->Name, symbol->Address);
        }
    }
    strcat(result, "\n");
#endif

    return result;
}

static
const char* ut_logprint_kind_color(
    ut_log_verbosity kind)
{
    switch(kind) {
    case UT_ERROR_PROC: return "#[red]";
    case UT_ERROR_PUSH: return "#[red]";
    case UT_ERROR_FROM: return "#[red]";
    case UT_THROW: return "#[red]";
    case UT_ERROR: return "#[red]";
    case UT_WARNING: return "#[yellow]";
    case UT_INFO: return "#[blue]";
    case UT_OK: return "#[green]";
    case UT_TRACE: return "#[grey]";
    case UT_DEBUG: return "#[grey]";
    default: return "#[red]";
    }
}

static
char* ut_log_categoryIndent(
    char *categories[],
    int count,
    ut_log_verbosity kind)
{
    int i = 0;
    ut_strbuf buff = UT_STRBUF_INIT;
    const char *color = ut_logprint_kind_color(kind);

    while (categories[i] && (!count || i < count)) {
        i ++;
        ut_strbuf_append(&buff, "%s|#[reset] ", color);
    }

    return ut_strbuf_get(&buff);
}

static
char* ut_log_categoryString(
    char *categories[])
{
    int32_t i = 0;
    ut_strbuf buff = UT_STRBUF_INIT;

    if (categories) {
        ut_strbuf_appendstr(&buff, "#[grey]");
        while (categories[i]) {
            ut_strbuf_append(&buff, "%s", categories[i]);
            i ++;
            if (categories[i]) {
                ut_strbuf_appendstr(&buff, ".");
            }
        }
        ut_strbuf_appendstr(&buff, "#[reset]");
    }

    if (!i) {
        ut_strbuf_reset(&buff);
    }

    return ut_strbuf_get(&buff);
}

static
size_t printlen(
    const char *str)
{
    const char *ptr;
    char ch;
    int len = 0;
    for (ptr = str; (ch = *ptr); ptr++) {
        if (ch == '\033') {
            ptr += 7;
        }
        if (ch == '\xF0') {
            ptr += 2;
        }
        len ++;
        if (!*ptr) break;
    }
    if (ch == 27) {
        len --; /* If last character was a color code, correct length */
    }
    return len + 1;
}

static
char* ut_log_colorize(
    char *msg)
{
    ut_strbuf buff = UT_STRBUF_INIT;
    char *ptr, ch, prev = '\0';
    bool isNum = FALSE;
    char isStr = '\0';
    bool isVar = false;
    bool overrideColor = false;
    bool autoColor = true;
    bool dontAppend = false;

    for (ptr = msg; (ch = *ptr); ptr++) {
        dontAppend = false;

        if (!overrideColor) {
            if (isNum && !isdigit(ch) && !isalpha(ch) && (ch != '.') && (ch != '%')) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
                isNum = FALSE;
            }
            if (isStr && (isStr == ch) && prev != '\\') {
                isStr = '\0';
            } else if (((ch == '\'') || (ch == '"')) && !isStr && !isalpha(prev) && (prev != '\\')) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_CYAN);
                isStr = ch;
            }

            if ((isdigit(ch) || (ch == '%' && isdigit(prev)) || (ch == '-' && isdigit(ptr[1]))) && !isNum && !isStr && !isVar && !isalpha(prev) && !isdigit(prev) && (prev != '_') && (prev != '.')) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_GREEN);
                isNum = TRUE;
            }

            if (isVar && !isalpha(ch) && !isdigit(ch) && ch != '_') {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
                isVar = FALSE;
            }

            if (!isStr && !isVar && ch == '$' && isalpha(ptr[1])) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_CYAN);
                isVar = TRUE;
            }
        }

        if (!isVar && !isStr && !isNum && ch == '#' && ptr[1] == '[') {
            bool isColor = true;
            overrideColor = true;

            /* Custom colors */
            if (!strncmp(&ptr[2], "]", strlen("]"))) {
                autoColor = false;
            } else if (!strncmp(&ptr[2], "green]", strlen("green]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_GREEN);
            } else if (!strncmp(&ptr[2], "red]", strlen("red]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_RED);
            } else if (!strncmp(&ptr[2], "blue]", strlen("red]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_BLUE);
            } else if (!strncmp(&ptr[2], "magenta]", strlen("magenta]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_MAGENTA);
            } else if (!strncmp(&ptr[2], "cyan]", strlen("cyan]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_CYAN);
            } else if (!strncmp(&ptr[2], "yellow]", strlen("yellow]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_YELLOW);
            } else if (!strncmp(&ptr[2], "grey]", strlen("grey]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_GREY);
            } else if (!strncmp(&ptr[2], "white]", strlen("white]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
            } else if (!strncmp(&ptr[2], "bold]", strlen("bold]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_BOLD);
            } else if (!strncmp(&ptr[2], "normal]", strlen("normal]"))) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
            } else if (!strncmp(&ptr[2], "reset]", strlen("reset]"))) {
                overrideColor = false;
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
            } else {
                isColor = false;
                overrideColor = false;
            }

            if (isColor) {
                ptr += 2;
                while ((ch = *ptr) != ']') ptr ++;
                dontAppend = true;
            }
            if (!autoColor) {
                overrideColor = true;
            }
        }

        if (ch == '\n') {
            if (isNum || isStr || isVar || overrideColor) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
                overrideColor = false;
                isNum = false;
                isStr = false;
                isVar = false;
            }
        }

        if (!dontAppend) {
            ut_strbuf_appendstrn(&buff, ptr, 1);
        }

        if (!overrideColor) {
            if (((ch == '\'') || (ch == '"')) && !isStr) {
                if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
            }
        }

        prev = ch;
    }

    if (isNum || isStr || isVar || overrideColor) {
        if (UT_LOG_USE_COLORS) ut_strbuf_appendstr(&buff, UT_NORMAL);
    }

    return ut_strbuf_get(&buff);
}

static
void ut_logprint_kind(
    ut_strbuf *buf,
    ut_log_verbosity kind,
    bool placeholder)
{
    const char *color, *levelstr;
    int levelspace;

    if (placeholder) {
        color = "#[grey]";
        levelstr = "";
    } else {
        switch(kind) {
        case UT_ERROR_PROC: levelstr = "proc"; break;
        case UT_ERROR_PUSH: levelstr = "push"; break;
        case UT_ERROR_FROM: levelstr = " from"; break;
        case UT_THROW: levelstr = "throw"; break;
        case UT_ERROR: levelstr = "error"; break;
        case UT_WARNING: levelstr = "warn"; break;
        case UT_INFO: levelstr = "info"; break;
        case UT_OK: levelstr = "ok"; break;
        case UT_TRACE: levelstr = "trace"; break;
        case UT_DEBUG: levelstr = "debug"; break;
        default: levelstr = "critical"; break;
        }
        color = ut_logprint_kind_color(kind);
    }

    levelspace = 5;

    if (kind != UT_LOG) {
        ut_strbuf_append(
            buf,
            "%s[#[reset]%7s%s]%*s#[reset]",
            color,
            levelstr,
            color,
            levelspace - strlen(levelstr),
            "");
    }
}

static
void ut_logprint_time(
    ut_strbuf *buf,
    struct timespec t)
{
    ut_strbuf_append(buf, "%.9d.%.4d", t.tv_sec, t.tv_nsec / 100000);
}

static
void ut_logprint_friendlyTime(
    ut_strbuf *buf,
    struct timespec t)
{
    char tbuff[512];
    time_t sec = t.tv_sec;
    struct tm * timeinfo = localtime(&sec);
    strftime(tbuff, sizeof(tbuff), "%F %T", timeinfo);
    ut_strbuf_append(buf, "%s.%.4d", tbuff, t.tv_nsec / 100000);
}

static
void ut_logprint_deltaTime(
    ut_strbuf *buf,
    struct timespec now,
    ut_log_tlsData *data,
    bool printCategory)
{
    ut_strbuf_appendstr(buf, "#[grey]");
    if (UT_LOG_PROFILE) {
        ut_strbuf_appendstr(buf, " --------");
    } else {
        if (data->lastTime.tv_sec || (data->sp && !printCategory)) {
            struct timespec delta;
            if (data->sp && !data->frames[data->sp - 1].printed && !printCategory) {
                delta = timespec_sub(now, data->frames[data->sp - 1].lastTime);
            } else {
                delta = timespec_sub(now, data->lastTime);
            }

            ut_strbuf_append(buf, "+%.2d.%.5d", delta.tv_sec, delta.tv_nsec / 10000);
        } else {
            ut_strbuf_appendstr(buf, " --------");
        }
    }
    ut_strbuf_appendstr(buf, "#[reset]");
}

static
bool ut_logprint_sumTime(
    ut_strbuf *buf,
    struct timespec now,
    ut_log_tlsData *data)
{
    ut_log_frame *frame = &data->frames[data->sp - 1];
    if (frame->lastTime.tv_sec) {
        struct timespec delta = timespec_sub(now, frame->lastTime);
        if (!delta.tv_sec && delta.tv_nsec < 50000) {
            if (UT_LOG_PROFILE) {
                ut_strbuf_appendstr(buf, "#[grey] -------- #[reset]");
            }
            return false;
        }
        ut_strbuf_append(buf, " #[green]%.2d.%.5d#[reset]", delta.tv_sec, delta.tv_nsec / 10000);
    } else {
        ut_strbuf_appendstr(buf, "#[grey] --------#[reset]");
    }
    return true;
}

static
int ut_logprint_categories(
    ut_strbuf *buf,
    char *categories[])
{
    char *categoryStr = categories ? ut_log_categoryString(categories) : NULL;
    if (categoryStr) {
        int l = categoryStr[0] != 0;
        ut_strbuf_appendstr(buf, categoryStr);
        free(categoryStr);
        return l;
    } else {
        return 0;
    }
}

static
int ut_logprint_msg(
    ut_strbuf *buf,
    char* msg)
{
    if (!msg) {
        return 0;
    }
    ut_strbuf_appendstr(buf, msg);
    return 1;
}

static
char const* ut_log_stripFunctionName(
    char const *file)
{
    if (file[0] == '.') {
        char const *ptr;
        char ch;
        for (ptr = file; (ch = *ptr); ptr++) {
            if (ch == '.' || ch == UT_OS_PS[0]) {
                file = ptr + 1;
            } else {
                break;
            }
        }
        return ptr;
    } else {
        return file;
    }
}

static
int ut_logprint_file(
    ut_strbuf *buf,
    char const *file,
    bool fixedWidth)
{
    file = ut_log_stripFunctionName(file);
    if (file) {
        if (!fixedWidth) {
            ut_strbuf_append(buf, "#[bold]%s#[reset]", file);
        } else {
            int len = strlen(file);
            if (len > (UT_LOG_FILE_LEN)) {
                file += len - UT_LOG_FILE_LEN + 2;
                ut_strbuf_append(buf, "#[bold]..%*s#[reset]", UT_LOG_FILE_LEN - 2, file);
            } else {
                ut_strbuf_append(buf, "#[bold]%*s#[reset]", UT_LOG_FILE_LEN, file);
            }
        }
        return 1;
    } else {
        return 0;
    }
}

static
int ut_logprint_line(
    ut_strbuf *buf,
    uint64_t line,
    bool fixedWidth)
{
    if (line) {
        ut_strbuf_append(buf, "#[bold]%u#[reset]", line);
        if (fixedWidth) {
            int len = 4 - (floor(log10(line)) + 1);
            if (len) {
                ut_strbuf_append(buf, "%*s", len, "");
            }
        }
        return 1;
    } else {
        return 0;
    }
}

static
int ut_logprint_function(
    ut_strbuf *buf,
    char const *function)
{
    if (function) {
        ut_strbuf_append(buf, "#[cyan]%s#[reset]", function);
        return 1;
    } else {
        return 0;
    }
}

static
void ut_log_resetCursor(
    ut_log_tlsData *data)
{
    int i;
    for (i = 0; i < data->last_printed_len; i ++) {
        fprintf(stderr, "\b");
    }
}

static
void ut_log_clearLine(
    ut_log_tlsData *data)
{
    int i;
    if (data->last_printed_len) {
        for (i = 0; i < data->last_printed_len - 1; i ++) {
            fprintf(stderr, " ");
            //ut_sleep(0, 10000000);
        }
        for (i = 0; i < data->last_printed_len - 1; i ++) {
            //ut_sleep(0, 50000000);
            fprintf(stderr, "\b");
        }
    }
    //printf("len = %d\n", data->last_printed_len);
    data->last_printed_len = 0;
}

static
void ut_logprint(
    FILE *f,
    const char *log_fmt,
    ut_log_verbosity kind,
    char *categories[],
    char const *file,
    uint64_t line,
    char const *function,
    char *msg,
    uint16_t breakAtCategory,
    bool closeCategory)
{
    ut_strbuf buf = UT_STRBUF_INIT, *cur;
    const char *fmtptr;
    char ch;
    ut_log_tlsData *data = ut_getThreadData();
    bool stop = false;
    bool isTail = kind < UT_LOG_LEVEL;

    bool only_warn = (kind >= UT_WARNING || kind == UT_THROW) && kind != UT_LOG;

    if (kind == UT_THROW && UT_LOG_LEVEL == UT_DEBUG) {
        isTail = false;
    }

    bool
        prevSeparatedBySpace = TRUE,
        separatedBySpace = FALSE,
        inParentheses = FALSE;
    struct timespec now;

    if (!breakAtCategory || closeCategory) {
        timespec_gettime(&now);
    } else {
        now = data->frames[breakAtCategory - 1].lastTime;
    }

    //ut_log_clearLine(data);

    for (fmtptr = log_fmt; (ch = *fmtptr); fmtptr++) {
        ut_strbuf tmp = UT_STRBUF_INIT;
        if (inParentheses) {
            cur = &tmp;
        } else {
            cur = &buf;
        }

        if (ch == '%' && fmtptr[1]) {
            int ret = 1;
            switch(fmtptr[1]) {
            case 'd':
                if (closeCategory) {
                    if (!ut_logprint_sumTime(cur, now, data)) {
                        if (UT_LOG_PROFILE) {
                            stop = true;
                            break;
                        }
                    }
                } else {
                    ut_logprint_deltaTime(cur, now, data, breakAtCategory);
                }
                break;
            case 'T':
                ut_logprint_friendlyTime(cur, now);
                break;
            case 't': ut_logprint_time(cur, now); break;
            case 'v': ut_logprint_kind(cur, kind, breakAtCategory || closeCategory); break;
            case 'c':
            case 'C':
                if (breakAtCategory) {
                    stop = true;
                    break;
                }

                if (!isTail &&  !ut_log_shouldEmbedCategories && kind < UT_WARNING) {
                    int i = 0;

                    while (data->categories[i]) {
                        char *empty = "", *indent = empty;

                        if (i > 1) {
                            indent = ut_log_categoryIndent(data->categories, i - 1, UT_DEBUG);
                        }

                        if (!data->frames[i].printed) {
                            bool computeSum = !data->categories[i + 1] && !msg;
                            if (i) {
                                ut_logprint(
                                    f, log_fmt, kind, categories, file, line, function, NULL, i + 1, computeSum);
                                ut_log(
                                    "%s#[grey]├> %s#[reset]\n",
                                    indent,
                                    data->categories[i]);
                            } else {
                                ut_logprint(
                                    f, log_fmt, kind, categories, file, line, function, NULL, i + 1, computeSum);
                                ut_log(
                                    "#[grey]%s#[reset]\n",
                                    data->categories[i]);
                            }
                            data->frames[i].printed = true;
                        }

                        if (indent != empty) free(indent);
                        i ++;
                    }
                    if (i) data->frames[i - 1].count ++;

                    char *indent = ut_log_categoryIndent(data->categories, 0, UT_DEBUG);
                    ut_strbuf_appendstr(cur, indent);
                    free(indent);
                }

                ret = ut_logprint_categories(cur, categories);
                break;
            case 'f': ret = ut_logprint_file(cur, file, !ut_log_shouldEmbedCategories); break;
            case 'l': ret = ut_logprint_line(cur, line, !ut_log_shouldEmbedCategories); break;
            case 'r': ret = ut_logprint_function(cur, function); break;
            case 'm': ret = ut_logprint_msg(cur, msg); break;
            case 'a': ut_strbuf_append(cur, "#[cyan]%s#[reset]", ut_log_appName); break;
            case 'V': if (only_warn) { ut_logprint_kind(cur, kind, breakAtCategory || closeCategory); } else { ret = 0; } break;
            case 'F': if (only_warn) { ret = ut_logprint_file(cur, file, FALSE); } else { ret = 0; } break;
            case 'L': if (only_warn) { ret = ut_logprint_line(cur, line, FALSE); } else { ret = 0; } break;
            case 'R': if (only_warn) { ret = ut_logprint_function(cur, function); } else { ret = 0; } break;
            default:
                ut_strbuf_appendstr(cur, "%");
                ut_strbuf_appendstrn(cur, &fmtptr[1], 1);
                break;
            }

            if (stop) {
                break;
            }

            if (fmtptr[2] == ' ') {
                separatedBySpace = TRUE;
            } else if (fmtptr[2] && (fmtptr[2] != '%') && (fmtptr[3] == ' ')) {
                separatedBySpace = TRUE;
            } else {
                separatedBySpace = FALSE;
            }

            if (!ret) {
                if (fmtptr[2] && (fmtptr[2] != '%')) {
                    if ((fmtptr[2] != ' ') && (fmtptr[3] == ' ') && prevSeparatedBySpace) {
                        fmtptr += 2;
                    } else {
                        fmtptr += 1;
                    }
                }
            } else {
                if (inParentheses) {
                    ut_strbuf_appendstrn(&buf, "(", 1);
                    char *str = ut_strbuf_get(cur);
                    ut_strbuf_appendstr(&buf, str);
                    free(str);
                }
            }

            prevSeparatedBySpace = separatedBySpace;
            fmtptr += 1;
            inParentheses = false;
        } else {
            if (ch == '(' && fmtptr[1] == '%' && (fmtptr[2] && fmtptr[2] != '%')) {
                inParentheses = true;
            } else {
                ut_strbuf_appendstrn(&buf, &ch, 1);
                inParentheses = false;
            }
        }
    }

    data->lastTime = now;

    char *str = ut_strbuf_get(&buf);

    if (str) {
        char *colorized = ut_log_colorize(str);

        if (breakAtCategory) {
            fprintf(f, "%s", colorized);
        } else {
            if (isTail) {
                fprintf(f, "%s\n", colorized);
                //data->last_printed_len = printlen(colorized);
                //ut_log_resetCursor(data);
            } else {
                if (msg) {
                    fprintf(f, "%s\n", colorized);
                }
            }
        }
        free(colorized);
        free(str);
    }
}

static
char* ut_getLastInfo(void) {
    ut_log_tlsData *data = ut_getThreadData();
    return data->lastInfo;
}

static
void ut_raise_codeframe(
    ut_log_codeframe *codeframe,
    bool first,
    const char *log_fmt,
    const char *prefix)
{
    if (codeframe->error || UT_LOG_LEVEL != UT_INFO) {
        int kind;
        if (codeframe->thrown && !first) {
            kind = UT_ERROR_FROM;
        } else if (first) {
            kind = UT_ERROR;
        } else {
            kind = UT_ERROR_PUSH;
        }

        char *error = codeframe->error;
        if (prefix && error) {
            error = ut_asprintf("%s%s", prefix, codeframe->error);
        }

        ut_logprint(
          stderr,
          log_fmt,
          kind,
          NULL,
          codeframe->file,
          codeframe->line,
          codeframe->function,
          error,
          FALSE,
          FALSE);

        if (codeframe->detail) {
            ut_log("   %s\n", codeframe->detail);
        }

        if (error != codeframe->error) {
            free(error);
        }
    }
}

static
bool ut_raise_intern(
    ut_log_tlsData *data,
    bool clearCategory,
    const char *log_fmt,
    const char *prefix)
{
    if (!data->viewed && data->exceptionCount && UT_LOG_LEVEL <= UT_ERROR) {
        unsigned int category, function, count = 0, total = data->exceptionCount;

        for (category = 0; category < data->exceptionCount; category ++) {
            ut_log_frame *frame = &data->exceptionFrames[category];
            for (function = 0; function < frame->sp; function ++) {
                ut_log_codeframe *codeframe = &frame->frames[function];
                ut_raise_codeframe(codeframe, !count, log_fmt, prefix);
                count ++;
            }

            if (category != data->exceptionCount - 1) {
                ut_raise_codeframe(&frame->initial, !count, log_fmt, prefix);
                count ++;
            }

            if (clearCategory) {
                data->exceptionCategories[total - category - 1] = NULL;
            }

            frame->sp = 0;
        }

        if (count == 0) {
            printf("abort! non-viewed exception raised without frames\n");
            ut_backtrace(stderr);
            abort();
        }

        if (clearCategory) {
            data->exceptionCount = 0;
        }

        data->viewed = true;

        if (UT_LOG_BACKTRACE) {
            ut_backtrace(stderr);
        }

        if (UT_LOG_EXCEPTION_ACTION == UT_LOG_ON_EXCEPTION_EXIT) {
            exit(-1);
        } else if (UT_LOG_EXCEPTION_ACTION == UT_LOG_ON_EXCEPTION_ABORT) {
            abort();
        }

        return true;
    } else {
        if (clearCategory) {
            data->exceptionCount = 0;
            data->exceptionFrames[0].sp = 0;
        }
        return false;
    }
}

static
void ut_frame_free(
    ut_log_frame *frame)
{
    char *str;
    unsigned int i;
    for (i = 0; i < frame->sp; i ++) {
        ut_log_codeframe *codeframe = &frame->frames[i];
        if ((str = codeframe->error)) free(str);
        if ((str = codeframe->file)) free(str);
        if ((str = codeframe->function)) free(str);
        if ((str = codeframe->detail)) free(str);
    }
    frame->sp = 0;
}

static
void ut_lasterrorFree(
    void* tls)
{
    ut_log_tlsData* data = tls;
    if (data) {
        ut_raise_intern(data, true, UT_LOG_FMT_CURRENT, NULL);
        unsigned int i;
        for (i = 0; i < data->sp; i ++) {
            ut_frame_free(&data->frames[i]);
        }
        for (i = 0; i < data->exceptionCount; i ++) {
            ut_frame_free(&data->exceptionFrames[i]);
        }

        if (data->backtrace) {
            free(data->backtrace);
        }
        free(data);
    }
}

static
void ut_log_setError(
    char const *file,
    unsigned int line,
    char const *function,
    char* error,
    bool raiseUnreported)
{
    int stack_marker; /* Use this variable to check if moving up or down on the stack */
    (void)stack_marker;

    ut_log_tlsData *data = ut_getThreadData();
    if (data->backtrace) free(data->backtrace);

    if (data->stack_marker >= (void*)&stack_marker) {
        /* If stack marker is higher than the current stack, program is
         * traveling up the stack after an exception was reported. */
        if (raiseUnreported) {
            ut_raise_intern(data, true, UT_LOG_FMT_CURRENT, NULL);
        }
    }

    data->viewed = false;

    if (!data->exceptionCount && !data->exceptionFrames[0].sp) {
        data->stack_marker = (void*)&stack_marker;

        /* Copy all current frames to exception cache in reverse order */
        unsigned int i;
        for (i = 1; i <= data->sp; i ++) {
            data->exceptionFrames[i - 1] = data->frames[data->sp - i];
            data->exceptionFrames[i - 1].category = ut_strdup(data->frames[data->sp - i].category);
            data->exceptionFrames[i - 1].initial.file = ut_strdup(data->frames[data->sp - i].initial.file);
            data->exceptionFrames[i - 1].initial.function = ut_strdup(data->frames[data->sp - i].initial.function);
            data->exceptionFrames[i - 1].sp = 0;
        }
        data->exceptionCount = data->sp + 1;

        /* Copy category stack in normal order */
        for (i = 1; i <= data->sp; i ++) {
            data->exceptionCategories[i - 1] = data->exceptionFrames[data->sp - i].category;
        }

        /* Array must end with a NULL */
        data->exceptionCategories[i - 1] = NULL;

        /* Set error in top-level frame */
        data->exceptionFrames[0].frames[0].file = ut_strdup(file);
        data->exceptionFrames[0].frames[0].function = ut_strdup(function);
        data->exceptionFrames[0].frames[0].line = line;
        data->exceptionFrames[0].frames[0].error = error ? ut_strdup(error) : NULL;
        data->exceptionFrames[0].frames[0].thrown = true;
        data->exceptionFrames[0].sp = 1;

    } else {
        if (data->exceptionCount == 0) {
            printf("abort: no active exception to append to\n");
            ut_backtrace(stderr);
            abort();
        }
        if (data->exceptionCount <= data->sp) {
            printf("abort: the total number of exceptions must be larger than the current stack\n");
            ut_backtrace(stderr);
            abort();
        }

        ut_log_frame *frame = &data->exceptionFrames[data->exceptionCount - data->sp - 1];

        ut_assert(frame->sp < UT_MAX_LOG_CODEFRAMES, "max number of code frames reached");

        frame->frames[frame->sp].file = ut_strdup(file);
        frame->frames[frame->sp].function = ut_strdup(function);
        frame->frames[frame->sp].line = line;
        frame->frames[frame->sp].error = error ? ut_strdup(error) : NULL;
        frame->frames[frame->sp].thrown = true;
        frame->sp ++;
    }
}

static
void ut_setLastMessage(
    char* err)
{
    ut_log_tlsData *data = ut_getThreadData();
    if (data->lastInfo) free(data->lastInfo);
    data->lastInfo = err ? ut_strdup(err) : NULL;
}

static
char* ut_log_parseComponents(
    char *categories[],
    char *msg)
{
    char *ptr, *prev = msg, ch;
    int count = 0;
    ut_log_tlsData *data = ut_getThreadData();

    if (ut_log_shouldEmbedCategories) {
        while (data->categories[count]) {
            categories[count] = data->categories[count];
            count ++;
        }
    }

    for (ptr = msg; (ch = *ptr) && (isalpha(ch) || isdigit(ch) || (ch == ':') || (ch == '/') || (ch == '_')); ptr++) {
        if ((ch == ':') && (ptr[1] == ' ')) {
            *ptr = '\0';
            categories[count ++] = prev;
            ptr ++;
            prev = ptr + 1;
            if (count == UT_MAX_LOG_CATEGORIES) {
                break;
            }
        }
    }

    categories[count] = NULL;

    return prev;
}

void ut_log_fmt(
    char *fmt)
{
    bool newValue = true;
    if (!fmt || !strlen(fmt)) {
        newValue = false;
    }

    if (newValue && UT_LOG_FMT_APPLICATION) {
        free(UT_LOG_FMT_APPLICATION);
    }

    if (newValue) {
        UT_LOG_FMT_CURRENT = strdup(fmt);
        UT_LOG_FMT_APPLICATION = UT_LOG_FMT_CURRENT;
    }

    ut_setenv("UT_LOG_FORMAT", "%s", UT_LOG_FMT_CURRENT);

    char *ptr, ch;
    for (ptr = UT_LOG_FMT_CURRENT; (ch = *ptr); ptr++) {
        if (ch == '%') {
            if (ptr[1] == 'C') {
                ut_log_embedCategories(false);
            } else if (ptr[1] == 'c') {
                ut_log_embedCategories(true);
            }
        }
    }
}

const char* ut_log_fmtGet(void)
{
    return UT_LOG_FMT_CURRENT;
}

ut_log_verbosity ut_logv(
    char const *file,
    unsigned int line,
    char const *function,
    ut_log_verbosity kind,
    unsigned int level,
    const char *fmt,
    va_list arg,
    bool list_defined,
    FILE* f,
    bool overwrite)
{
    ut_log_tlsData *data = ut_getThreadData();
    ut_raise_intern(data, false, UT_LOG_FMT_CURRENT, NULL);

    if (kind >= UT_LOG_LEVEL ||
        (overwrite && (UT_LOG_LEVEL - kind == 1)) ||
        ut_log_handlerRegistered())
    {
        char* alloc = NULL;
        char buff[UT_MAX_LOG + 1];
        char *categories[UT_MAX_LOG_CATEGORIES];
        size_t n = 0;
        char* msg = buff, *msgBody;
        va_list argcpy;

        UT_UNUSED(level);

        if (list_defined) {
            va_copy(argcpy, arg); /* Make copy of arglist in
                                   * case vsnprintf needs to be called twice */
        }

        if (list_defined) {
            if ((n = (vsnprintf(buff, UT_MAX_LOG, fmt, arg) + 1)) >
                UT_MAX_LOG)
            {
                alloc = malloc(n + 2);
                vsnprintf(alloc, n, fmt, argcpy);
                msg = alloc;
            }
        } else {
            msg = ut_strdup(fmt);
        }

        msgBody = ut_log_parseComponents(categories, msg);

        if (kind >= UT_LOG_LEVEL || overwrite) {
            if (data->sp < UT_LOG_DEPTH) {
                ut_logprint(
                  f,
                  UT_LOG_FMT_CURRENT,
                  kind, categories,
                  file,
                  line,
                  function,
                  msgBody,
                  FALSE,
                  FALSE);
            }
        }


        ut_err_notifyCallkback(kind, msgBody);

        if (alloc) {
            free(alloc);
        }
    }

    ut_catch();

    return kind;
}

int _ut_log_push(
    char const *file,
    unsigned int line,
    char const *function,
    const char *category)
{
    ut_log_tlsData *data = ut_getThreadData();

    ut_assert(data->sp < UT_MAX_LOG_CATEGORIES,
        "cannot push '%s', max nested categories reached(%d)",
        category,
        UT_MAX_LOG_CATEGORIES);

    /* Clear any errors before pushing a new stack */
    ut_raise_intern(data, false, UT_LOG_FMT_CURRENT, NULL);

    ut_log_frame *frame = &data->frames[data->sp];

    frame->category = category ? strdup(category) : NULL;
    data->categories[data->sp] = frame->category;
    frame->count = 0;
    frame->printed = false;
    frame->initial.file = strdup(ut_log_stripFunctionName(file));
    frame->initial.function = strdup(function);
    frame->initial.line = line;
    frame->initial.thrown = false;
    frame->sp = 0;
    timespec_gettime(&frame->lastTime);

    if (data->sp) {
        data->frames[data->sp - 1].count ++;
    }

    data->sp ++;

    return -1;
}

void _ut_log_pop(
    char const *file,
    unsigned int line,
    char const *function)
{
    ut_log_tlsData *data = ut_getThreadData();
    bool printed = false;

    if (data->sp) {
        ut_log_frame *frame = &data->frames[data->sp - 1];

        if (!frame->printed && UT_LOG_PROFILE) {
            printed = true;
            ut_logprint(
                stdout, UT_LOG_FMT_CURRENT, UT_INFO, NULL, file, line, function, NULL, FALSE, TRUE);
        }

        if (strcmp(frame->initial.function, function)) {
            va_list empty_list;
            ut_logv(
                file,
                line,
                function,
                UT_TRACE,
                0,
                strarg("log_pop called in '%s' but matching log_push in '%s'",
                    function, frame->initial.function),
                empty_list,
                false,
                stdout,
                false);
        }

        if (frame->initial.file) free(frame->initial.file);
        if (frame->initial.function) free(frame->initial.function);
        if (frame->category) free(frame->category);
        frame->sp = 0;

        data->frames[data->sp - 1].count += data->frames[data->sp].count;
        data->categories[data->sp - 1] = NULL;

        /* If categories are not embedded in log message, they are displayed in
         * a hierarchical it */
        if (!ut_log_shouldEmbedCategories && !data->exceptionCount) {

            /* Only print close if messages were logged for category */
            if (frame->printed && !printed) {
                char *indent = ut_log_categoryIndent(data->categories, 0, UT_DEBUG);
                /* Print everything that preceeds the category */
                ut_logprint(
                    stdout, UT_LOG_FMT_CURRENT, UT_INFO, data->categories, file, line, NULL, NULL, TRUE, TRUE);
                ut_log(
                    "%s#[grey]+#[reset]\n", indent ? indent : "");
                if (indent) free(indent);
            }
        }

        data->sp --;
    } else {
        ut_critical_fl(
            file, line, "ut_log_pop called more times than ut_log_push");
    }
}

void _ut_assertv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_ASSERT, 0, fmt, args, true, stderr, false);
    ut_backtrace(stderr);
    abort();
}

void ut_criticalv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_CRITICAL, 0, fmt, args, true, stderr, false);
    ut_backtrace(stderr);
    fflush(stderr);
    abort();
}

void ut_debugv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_DEBUG, 0, fmt, args, true, stderr, false);
}

void ut_tracev(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_TRACE, 0, fmt, args, true, stdout, false);
}

void ut_warningv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_WARNING, 0, fmt, args, true, stderr, false);
}

void ut_errorv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_ERROR, 0, fmt, args, true, stderr, false);
    if (UT_LOG_BACKTRACE || (ut_log_verbosityGet() == UT_DEBUG)) {
        ut_backtrace(stderr);
    }
}

void ut_okv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_OK, 0, fmt, args, true, stdout, false);
}

void ut_infov(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, UT_INFO, 0, fmt, args, true, stdout, false);
}

void _ut_log_overwritev(
    char const *file,
    unsigned int line,
    char const *function,
    ut_log_verbosity verbosity,
    const char *fmt,
    va_list args)
{
    ut_logv(file, line, function, verbosity, 0, fmt, args, true, stdout, true);
}

static
void ut_throwv_intern(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args,
    bool raiseUnreported)
{
    char *err = NULL, *categories = NULL;
    ut_log_tlsData *data = ut_getThreadData();

    if (data) {
        categories = ut_log_categoryString(data->categories);
    }

    if (fmt) {
        err = ut_vasprintf(fmt, args);
    }

    ut_log_setError(file, line, function, err, raiseUnreported);

    if (fmt && ((ut_log_verbosityGet() <= UT_DEBUG) || UT_APP_STATUS)) {
        if (UT_APP_STATUS == 1) {
            printf("!! error raised while starting up\n");
            ut_raise();
        } else if (UT_APP_STATUS){
            printf("!! error raised while shutting down\n");
            ut_raise();
        } else {
            ut_logprint(stderr, UT_LOG_FMT_CURRENT, UT_THROW, NULL, file, line, function, err, FALSE, FALSE);
        }
    }

    if (err) free(err);
}

void ut_throwv(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    va_list args)
{
    ut_throwv_intern(file, line, function, fmt, args, TRUE);
}

void ut_throw_detailv(
    const char *fmt,
    va_list args)
{
    ut_log_tlsData *data = ut_getThreadData();

    ut_assert(
        data->exceptionCount != 0,
        "no active exception to append to (sp = %d)",
        data->exceptionFrames[0].sp);

    if (fmt) {
        ut_log_frame *frame = &data->exceptionFrames[data->exceptionCount - data->sp - 1];
        ut_assert(frame->sp > 0, "no codeframe to attach detail to");
        char *detail = ut_vasprintf(fmt, args);
        frame->frames[frame->sp - 1].detail = detail;
    }
}

void ut_setmsgv(
    char *fmt,
    va_list args)
{
    char *err = NULL;
    if (fmt) {
        err = ut_vasprintf(fmt, args);
    }
    ut_setLastMessage(err);
    free(err);
}

void _ut_debug(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_debugv(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_trace(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_tracev(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_info(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt, ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_infov(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_ok(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_okv(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_warning(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_warningv(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_error(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_errorv(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_critical(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_criticalv(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_assert(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    _ut_assertv(file, line, function, fmt, arglist);
    va_end(arglist);
}

void _ut_log_overwrite(
    char const *file,
    unsigned int line,
    char const *function,
    ut_log_verbosity verbosity,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    _ut_log_overwritev(file, line, function, verbosity, fmt, arglist);
    va_end(arglist);
}

char* ut_lastinfo(void) {
    return ut_getLastInfo();
}

void _ut_throw(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_throwv_intern(file, line, function, fmt, arglist, TRUE);
    va_end(arglist);
}

void _ut_throw_fallback(
    char const *file,
    unsigned int line,
    char const *function,
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_throwv_intern(file, line, function, fmt, arglist, FALSE);
    va_end(arglist);
}

void ut_throw_detail(
    const char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_throw_detailv(fmt, arglist);
    va_end(arglist);
}

void ut_setinfo(
    char *fmt,
    ...)
{
    va_list arglist;

    va_start(arglist, fmt);
    ut_setmsgv(fmt, arglist);
    va_end(arglist);
}

bool ut_catch(void)
{
    /* Clear exception */
    ut_log_tlsData *data = ut_getThreadData();
    unsigned int i;
    if (data->exceptionCount) {
        for (i = 0; i < data->exceptionCount; i ++) {
            ut_log_frame *frame = &data->exceptionFrames[i];
            ut_frame_free(frame);
        }
        data->exceptionCount = 0;
        return true;
    } else {
        return false;
    }
}

bool ut_raised(void)
{
    ut_log_tlsData *data = ut_getThreadData();
    if (data->exceptionCount) {
        return true;
    } else {
        return false;
    }
}

bool ut_raise(void) {
    ut_log_tlsData *data = ut_getThreadData();
    return ut_raise_intern(data, true, UT_LOG_FMT_CURRENT, NULL);
}

bool ut_raise_ext(
    const char *log_fmt,
    const char *prefix_fmt, ...)
{
    ut_log_tlsData *data = ut_getThreadData();
    va_list arglist;
    bool result;

    if (!log_fmt) {
        log_fmt = UT_LOG_FMT_CURRENT;
    }

    va_start(arglist, prefix_fmt);
    char *prefix = ut_vasprintf(prefix_fmt, arglist);
    va_end(arglist);
    result = ut_raise_intern(data, true, log_fmt, prefix);

    return result;
}

bool __ut_raise_check(void) {
    ut_log_tlsData *data = ut_getThreadData();
    return ut_raise_intern(data, false, UT_LOG_FMT_CURRENT, NULL);
}

static
char *ut_log_levelToStr(
    ut_log_verbosity level)
{
    switch(level) {
    case UT_DEBUG: return "DEBUG";
    case UT_TRACE: return "TRACE";
    case UT_OK: return "OK";
    case UT_INFO: return "INFO";
    case UT_WARNING: return "WARNING";
    case UT_ERROR: return "ERROR";
    case UT_CRITICAL: return "CRITICAL";
    case UT_ASSERT: return "ASSERT";
    default:
        ut_critical("invalid verbosity level %d", level);
        return NULL;
    }
}

ut_log_verbosity ut_log_verbositySet(
    ut_log_verbosity level)
{
    ut_log_verbosity old = UT_LOG_LEVEL;
    ut_setenv("UT_VERBOSITY", ut_log_levelToStr(level));
    UT_LOG_LEVEL = level;
    return old;
}

ut_log_verbosity ut_log_verbosityGet() {
    return UT_LOG_LEVEL;
}

int ut_log_verbositySetDepth(
    int depth)
{
    int old = UT_LOG_DEPTH;
    UT_LOG_DEPTH = depth;
    return old;
}

void ut_log_embedCategories(
    bool embed)
{
    ut_log_shouldEmbedCategories = embed;
}

int16_t ut_log_init(void) {
#ifdef _WIN32
    #if NTDDI_VERSION > NTDDI_WINBLUE
    ut_enable_console_color(STD_OUTPUT_HANDLE);
    ut_enable_console_color(STD_ERROR_HANDLE);
    #endif
#endif

    if (UT_KEY_LOG) {
        return  0;
    } else {
        return ut_tls_new(&UT_KEY_LOG, ut_lasterrorFree);
    }
}

char *ut_lasterr(void) {
    ut_catch();
    return "< lasterr deprecated, replace with ut_catch or ut_raise >";
}

void ut_log(char *fmt, ...) {
    va_list arglist;
    char *formatted, *colorized;
    ut_log_tlsData *data = ut_getThreadData();
    int len;

    va_start(arglist, fmt);
    formatted = ut_vasprintf(fmt, arglist);
    va_end(arglist);

    colorized = ut_log_colorize(formatted);
    len = printlen(colorized);
    fprintf(stdout, "%s", colorized);

    free(colorized);
    free(formatted);
}

bool ut_log_profile(
    bool enable)
{
    bool result = UT_LOG_PROFILE;
    UT_LOG_PROFILE = enable;
    if (enable) {
        ut_setenv("UT_LOG_PROFILE", "TRUE");
    } else {
        ut_setenv("UT_LOG_PROFILE", "FALSE");
    }
    return result;
}

ut_log_exceptionAction ut_log_setExceptionAction(
    ut_log_exceptionAction action)
{
    ut_log_exceptionAction result = UT_LOG_EXCEPTION_ACTION;
    UT_LOG_EXCEPTION_ACTION = action;
    return result;
}

bool ut_log_useColors(
    bool enable)
{
    bool result = UT_LOG_USE_COLORS;
    UT_LOG_USE_COLORS = enable;
    return result;
}

void ut_log_deinit() {
    if (UT_LOG_FMT_APPLICATION) {
        free(UT_LOG_FMT_APPLICATION);
        UT_LOG_FMT_APPLICATION = NULL;
    }
}
