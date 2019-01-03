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

#include "expr.h"

static
char* ut_exprTokenStr(
    ut_exprToken t)
{
    switch(t) {
    case UT_EXPR_TOKEN_NONE: return "none";
    case UT_EXPR_TOKEN_IDENTIFIER: return "identifier";
    case UT_EXPR_TOKEN_FILTER: return "filter";
    case UT_EXPR_TOKEN_SCOPE: return "/";
    case UT_EXPR_TOKEN_TREE: return "//";
    case UT_EXPR_TOKEN_THIS: return ".";
    case UT_EXPR_TOKEN_PARENT: return "..";
    case UT_EXPR_TOKEN_SEPARATOR: return ",";
    case UT_EXPR_TOKEN_AND: return "&";
    case UT_EXPR_TOKEN_OR: return "|";
    case UT_EXPR_TOKEN_NOT: return "^";
    }
    return NULL;
}

static
int ut_exprValidate(
    ut_expr_program data)
{
    int op;

    ut_exprToken t = UT_EXPR_TOKEN_NONE, tprev = UT_EXPR_TOKEN_NONE;
    for (op = 0; op < data->size; op++) {
        t = data->ops[op].token;

        switch(t) {
        case UT_EXPR_TOKEN_AND:
            switch(tprev) {
            case UT_EXPR_TOKEN_AND:
            case UT_EXPR_TOKEN_OR:
            case UT_EXPR_TOKEN_NOT:
            case UT_EXPR_TOKEN_SEPARATOR:
            case UT_EXPR_TOKEN_SCOPE:
            case UT_EXPR_TOKEN_PARENT:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_OR:
            switch(tprev) {
            case UT_EXPR_TOKEN_AND:
            case UT_EXPR_TOKEN_OR:
            case UT_EXPR_TOKEN_NOT:
            case UT_EXPR_TOKEN_SEPARATOR:
            case UT_EXPR_TOKEN_SCOPE:
            case UT_EXPR_TOKEN_PARENT:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_NOT:
            switch(tprev) {
            case UT_EXPR_TOKEN_AND:
            case UT_EXPR_TOKEN_OR:
            case UT_EXPR_TOKEN_NOT:
            case UT_EXPR_TOKEN_SEPARATOR:
            case UT_EXPR_TOKEN_SCOPE:
            case UT_EXPR_TOKEN_PARENT:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_SEPARATOR:
            switch(tprev) {
            case UT_EXPR_TOKEN_AND:
            case UT_EXPR_TOKEN_OR:
            case UT_EXPR_TOKEN_NOT:
            case UT_EXPR_TOKEN_SEPARATOR:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_IDENTIFIER:
            switch(tprev) {
            case UT_EXPR_TOKEN_IDENTIFIER:
            case UT_EXPR_TOKEN_FILTER:
            case UT_EXPR_TOKEN_THIS:
            case UT_EXPR_TOKEN_PARENT:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_SCOPE:
            switch(tprev) {
            case UT_EXPR_TOKEN_SCOPE:
            case UT_EXPR_TOKEN_TREE:
            case UT_EXPR_TOKEN_AND:
            case UT_EXPR_TOKEN_OR:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_TREE:
            switch(tprev) {
            case UT_EXPR_TOKEN_SCOPE:
            case UT_EXPR_TOKEN_TREE:
            case UT_EXPR_TOKEN_PARENT:
            case UT_EXPR_TOKEN_AND:
            case UT_EXPR_TOKEN_OR:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_THIS:
        case UT_EXPR_TOKEN_PARENT:
            switch(tprev) {
            case UT_EXPR_TOKEN_THIS:
            case UT_EXPR_TOKEN_PARENT:
            case UT_EXPR_TOKEN_NOT:
                goto error;
            default: break;
            }
            break;
        case UT_EXPR_TOKEN_FILTER:
            switch(tprev) {
            case UT_EXPR_TOKEN_IDENTIFIER:
            case UT_EXPR_TOKEN_THIS:
            case UT_EXPR_TOKEN_PARENT:
            case UT_EXPR_TOKEN_FILTER:
                goto error;
            default: break;
            }
            break;
        default:
            break;
        }
        tprev = t;
    }

    return 0;
error:
    ut_throw("unexpected '%s' after '%s'",
        ut_exprTokenStr(t),
        ut_exprTokenStr(tprev));
    return -1;
}

bool ut_expr_isOperator(
    char ch)
{
    switch (ch) {
    case '*':
    case '?':
    case '^':
    case '&':
    case '|':
    case ',':
        return true;
    default:
        return false;
    }
}

bool ut_expr_hasOperators(
    const char *expr)
{
    const char *ptr;
    char ch;
    for (ptr = expr; (ch = *ptr); ptr++) {
        if (ch == '/' && ptr[1] == '/') {
            return true;
        } else if (ut_expr_isOperator(ch)) {
            return true;
        }
    }
    return false;
}

int16_t ut_exprParseIntern(
    ut_expr_program data,
    const char *expr,
    bool allowScopes,
    bool allowSeparators)
{
    char *ptr, *start, ch;
    int op = 0;

    data->size = 0;
    data->kind = 0;
    data->tokens = ut_strdup(expr);
    strlower(data->tokens);

    ptr = data->tokens;
    for (; (ch = *ptr); data->ops[op].start = ptr, ptr++) {
        data->ops[op].containsWildcard = false;
        data->ops[op].start = NULL;
        start = ptr;
        switch(ch) {
        case '/':
            if (!allowScopes) {
                ut_throw("scope operators not allowed");
                goto error;
            }
            if (ptr[1] == '/') {
                data->ops[op].token = UT_EXPR_TOKEN_TREE;
                *ptr = '\0';
                ptr++;
            } else {
                *ptr = '\0';
                data->ops[op].token = UT_EXPR_TOKEN_SCOPE;
            }
            break;
        case ':':
            if (!allowScopes) {
                ut_throw("scope operators not allowed");
                goto error;
            }
            if (ptr[1] == ':') {
                data->ops[op].token = UT_EXPR_TOKEN_SCOPE;
                *ptr = '\0';
                ptr++;
            } else {
                ut_throw("invalid usage of ':'");
                goto error;
            }
            break;
        case '|':
            data->ops[op].token = UT_EXPR_TOKEN_OR;
            *ptr = '\0';
            break;
        case '&':
            data->ops[op].token = UT_EXPR_TOKEN_AND;
            *ptr = '\0';
            break;
        case '^':
            data->ops[op].token = UT_EXPR_TOKEN_NOT;
            *ptr = '\0';
            break;
        case ',':
            if (!allowSeparators) {
                ut_throw("scope operators not allowed");
                goto error;
            }
            data->ops[op].token = UT_EXPR_TOKEN_SEPARATOR;
            *ptr = '\0';
            break;
        case '.':
            if (!allowScopes) {
                ut_throw("scope operators not allowed");
                goto error;
            }

            if (ptr[1] == '.') {
                if ((op < 4) ||
                    ((data->ops[op - 2].token == UT_EXPR_TOKEN_PARENT) &&
                     (data->ops[op - 1].token == UT_EXPR_TOKEN_SCOPE)))
                {
                    data->ops[op].token = UT_EXPR_TOKEN_PARENT;
                } else {
                    op -= 4;
                }
                *ptr = '\0';
                ptr++;
            } else {
                if ((op < 2) || (data->ops[op - 1].token != UT_EXPR_TOKEN_SCOPE)) {
                    data->ops[op].token = UT_EXPR_TOKEN_THIS;
                } else {
                    op -= 2;
                }
                *ptr = '\0';
            }
            break;
        default:
            data->ops[op].token = UT_EXPR_TOKEN_IDENTIFIER;
            while((ch = *ptr++) &&
                  (isalnum(ch) || (ch == '_') || (ch == '*') || (ch == '?') ||
                    (ch == '(') || (ch == ')') || (ch == '{') || (ch == '}') ||
                    (ch == ' ') || (ch == '$') || (ch == '.')))
            {
                if ((ch == '*') || (ch == '?')) {
                    data->ops[op].token = UT_EXPR_TOKEN_FILTER;
                }
            }

            ptr--; /* Go back one character to adjust for lookahead of one */
            if (!(ptr - start)) {
                ut_throw("invalid character '%c' (expr = '%s')", ch, expr);
                goto error;
            }
            ptr--;
            break;
        }

        if (!data->ops[op].start) {
            if (data->ops[op].token == UT_EXPR_TOKEN_IDENTIFIER) {
                if (op && data->ops[op - 1].token == UT_EXPR_TOKEN_THIS) {
                    op --;
                    start --;
                    *start = '.';
                    data->ops[op].token = UT_EXPR_TOKEN_IDENTIFIER;
                }
            }
            data->ops[op].start = start;
        }
        if (++op == (UT_EXPR_MAX_OP - 2)) {
            ut_throw("expression contains too many tokens");
            goto error;
        }
    }

    if (op) {
        /* If expression ends with scope or tree, append '*' */
        if ((data->ops[op - 1].token == UT_EXPR_TOKEN_SCOPE) ||
            (data->ops[op - 1].token == UT_EXPR_TOKEN_TREE))
        {
            data->ops[op].token = UT_EXPR_TOKEN_FILTER;
            data->ops[op].start = "*";
            op ++;
        }
        data->size = op;
        data->ops[op].token = UT_EXPR_TOKEN_NONE; /* Close with NONE */
        if (ut_exprValidate(data)) {
            data->size = 0;
            goto error;
        }
    }

    return 0;
error:
    return -1;
}

ut_expr_program ut_expr_compile(
    const char *expr,
    bool allowScopes,
    bool allowSeparators)
{
    ut_expr_program result = malloc(sizeof(struct ut_expr_program_s));
    result->kind = 0;
    result->tokens = NULL;

    ut_debug("match: compile expression '%s'", expr);
    if (ut_exprParseIntern(result, expr, allowScopes, allowSeparators) || !result->size) {
        if (!result->size) {
            ut_throw("expression '%s' resulted in empty program", expr);
        }
        free(result->tokens);
        free(result);
        result = NULL;
        goto error;
    }

    /* Optimize for common cases (*, simple identifier) */
    if (result->size == 1) {
        if (result->ops[0].token == UT_EXPR_TOKEN_IDENTIFIER) {
            result->kind = 1;
        } else if (result->ops[0].token == UT_EXPR_TOKEN_THIS) {
            result->kind = 2;
        } else if (result->ops[0].token == UT_EXPR_TOKEN_FILTER) {
            if (!strcmp(result->ops[0].start, "*")) {
                result->kind = 3;
            }
        }
    } else if (result->size == 2) {
        if (result->ops[0].token == UT_EXPR_TOKEN_SCOPE) {
            if (result->ops[1].token == UT_EXPR_TOKEN_FILTER) {
                if (!strcmp(result->ops[1].start, "*")) {
                    result->kind = 3;
                }
            } else if (result->ops[1].token == UT_EXPR_TOKEN_IDENTIFIER) {
                result->kind = 1;
            } else if (result->ops[1].token == UT_EXPR_TOKEN_THIS) {
                result->kind = 2;
            }
        } else if (result->ops[0].token == UT_EXPR_TOKEN_TREE) {
            if (result->ops[1].token == UT_EXPR_TOKEN_FILTER) {
                if (!strcmp(result->ops[1].start, "*")) {
                    result->kind = 4;
                }
            }
        }
    }

    return result;
error:
    return NULL;
}

int ut_expr_scope(
    ut_expr_program program)
{
    int result = 0;

    if (program->kind == 1) {
        result = 0;
    } else if (program->kind == 2) {
        result = 0;
    } else if (program->kind == 3) {
        result = 1;
    } else if (program->kind == 4) {
        result = 2;
    } else {
        int i;
        for (i = 0; i < program->size; i++) {
            switch(program->ops[i].token) {
            case UT_EXPR_TOKEN_SCOPE:
                if (result < 1) result = 1;
                break;
            case UT_EXPR_TOKEN_TREE:
                result = 2;
                break;
            default:
                break;
            }
        }
    }

    return result;
}

bool ut_expr_runExpr(
    ut_exprOp **op,
    const char **elements[],
    ut_exprToken precedence)
{
    bool done = false;
    bool result = true;
    bool right = false;
    bool identifierMatched = false;
    ut_exprOp *cur;
    const char **start = *elements; // Pointer to array of strings

    do {
        /*printf("eval %s [%s => %s] (prec=%s)\n",
          ut_exprTokenStr((*op)->token),
          (*elements)[0],
          (*op)->start,
          ut_exprTokenStr(precedence));*/

        cur = *op; (*op) ++;

        switch(cur->token) {
        case UT_EXPR_TOKEN_THIS:
            result = !strcmp(".", (*elements)[0]);
            identifierMatched = true;
            break;
        case UT_EXPR_TOKEN_IDENTIFIER:
        case UT_EXPR_TOKEN_FILTER: {
            const char *elem = (*elements)[0];
            if (elem && strcmp(elem, ".")) {
                result = !fnmatch(cur->start, (*elements)[0], 0);
            } else {
                result = false;
                done = true;
            }
            identifierMatched = true;
            break;
        }
        case UT_EXPR_TOKEN_AND:
            right = ut_expr_runExpr(op, elements, UT_EXPR_TOKEN_IDENTIFIER);
            if (result) result = right;
            break;
        case UT_EXPR_TOKEN_OR:
            right = ut_expr_runExpr(op, elements, UT_EXPR_TOKEN_AND);
            if (!result) result = right;
            break;
        case UT_EXPR_TOKEN_NOT:
            right = ut_expr_runExpr(op, elements, UT_EXPR_TOKEN_OR);
            if (result) result = !right;
            break;
        case UT_EXPR_TOKEN_SCOPE:
            (*elements)++;
            if (!(*elements)[0]) {
                result = false;
                done = true;
                (*op)++; /* progress op for skipped value */
            } else {
                right = ut_expr_runExpr(op, elements, UT_EXPR_TOKEN_NOT);
                if (result) result = right;
            }
            break;
        case UT_EXPR_TOKEN_TREE: {
            ut_exprOp *opPtr = *op;
            if (identifierMatched) {
                if (!result) {
                    done = true;
                    break;
                }
                (*elements)++;

            }

            const char **elementPtr = *elements, **elementFound = NULL;
            right = false;
            if (!elementPtr[0]) {
                result = true;
                done = true;
                (*op)++; /* progress op for skipped value */
            } else {
                do {
                    *elements = elementPtr;
                    *op = opPtr;
                    right = ut_expr_runExpr(op, elements, UT_EXPR_TOKEN_SCOPE);
                    if (right) {
                        elementFound = *elements;
                    }
                } while ((elementFound ? right : !right) && (++elementPtr)[0]);
                if ((result = (elementFound != NULL))) {
                    *elements = elementFound;
                }
            }
            break;
        }
        case UT_EXPR_TOKEN_SEPARATOR:
            *(elements) = start;
            right = ut_expr_runExpr(op, elements, UT_EXPR_TOKEN_TREE);
            if (!result) {
                result = right;
            }
            break;
        default:
            result = false;
            done = true;
            break;
        }
    } while(!done &&
            ((*op)->token != UT_EXPR_TOKEN_NONE) &&
            ((*op)->token <= precedence));

    return result;
}

bool ut_expr_run(
    ut_expr_program program,
    const char *str)
{
    bool result = false;

    if (!program->size) {
        return false;
    }

    if (program->kind == 0) {
        const char *elements[UT_MAX_SCOPE_DEPTH + 1];
        ut_exprOp *op = program->ops;
        const char **elem = elements;
        ut_id id;
        if (str) {
            strcpy(id, str);
            strlower(id);
        } else {
            strcpy(id, "");
        }

        int8_t elementCount = ut_pathToArray(id, elements, "/");
        if (elementCount == -1) {
            goto error;
        }
        elements[elementCount] = NULL;

        /* Ignore leading scope tokens ('/') in expression and string */
        if (op->token == UT_EXPR_TOKEN_SCOPE) {
            op ++;
        }

        if (str && str[0] && !elements[0][0]) elem++;

        result = ut_expr_runExpr(&op, &elem, UT_EXPR_TOKEN_SEPARATOR);
        if (result) {
            if (elem != &elements[elementCount - 1]) {
                /* Not all elements have been matched */
                result = false;
            }
        }
    } else if (program->kind == 1) {
        /* Match identifier */
        result = !stricmp(program->ops[0].start, str);
    } else if (program->kind == 2) {
        result = !strcmp(".", str);
    } else if (program->kind == 3) {
        /* Match any identifier in scope */
        const char *ptr = str;
        if (ptr[0] == '/') ptr ++;
        if (ptr[0] == '.' && !ptr) {
            result = false;
        } else if (!strchr(ptr, '/')) {
            result = true;
        } else {
            result = false;
        }
    } else if (program->kind == 4) {
        /* Match any identifier in tree */
        if (strcmp(str, ".")) {
            result = true;
        }
    }

    return result;
error:
    return false;
}

const char* ut_matchParent(
    const char *parent,
    const char *expr)
{
    const char *parentPtr = parent, *exprPtr = expr;
    char parentCh, exprCh;

    if (!parent) {
        return expr;
    }

    if (*parentPtr == '/') parentPtr++;
    if (*exprPtr == '/') exprPtr++;

    if (!*parentPtr) {
        return exprPtr;
    }

    while ((parentCh = *parentPtr) &&
           (exprCh = *exprPtr))
    {
        if (parentCh < 97) parentCh = tolower(parentCh);
        if (exprCh < 97) exprCh = tolower(exprCh);

        if (parentCh != exprCh) {
            break;
        }

        parentPtr++;
        exprPtr++;
    }

    if (*parentPtr == '\0') {
        if (*exprPtr == '/') {
            exprPtr ++;
        } else if (*exprPtr == '\0') {
            exprPtr = ".";
        } else {
            exprPtr = NULL;
        }
        return exprPtr;
    } else {
        return NULL;
    }
}

void ut_expr_free(ut_expr_program matcher) {
    if (matcher) {
        if (matcher->tokens) {
            free(matcher->tokens);
        }
        free(matcher);
    }
}

bool ut_expr(
    const char *expr,
    const char *str)
{
    struct ut_expr_program_s matcher;
    if (ut_exprParseIntern(&matcher, expr, true, true)) {
        goto error;
    }
    bool result = ut_expr_run(&matcher, str);
    free(matcher.tokens);
    return result;
error:
    return false;
}

int ut_expr_get_scope(
    ut_expr_program program)
{
    int result = 1;
    int32_t op;
    bool quit = false;

    for (op = 0; (op < program->size) && !quit; op ++) {
        switch (program->ops[op].token) {
        case UT_EXPR_TOKEN_IDENTIFIER:
        case UT_EXPR_TOKEN_THIS:
        case UT_EXPR_TOKEN_PARENT:
            result = 0;
            break;
        case UT_EXPR_TOKEN_SCOPE:
            result = 1;
            break;
        case UT_EXPR_TOKEN_TREE:
            result = 2;
        default:
            quit = true;
            break;
        }
    }

    return result;
}
