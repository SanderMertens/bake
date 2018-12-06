/* Copyright (c) 2010-2018 the corto developers
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

#ifndef UT__MATCHER_H_
#define UT__MATCHER_H_

#include "../include/util.h"

typedef enum ut_exprToken {
    UT_EXPR_TOKEN_NONE,
    UT_EXPR_TOKEN_THIS,
    UT_EXPR_TOKEN_PARENT,
    UT_EXPR_TOKEN_IDENTIFIER,
    UT_EXPR_TOKEN_FILTER,
    UT_EXPR_TOKEN_AND,
    UT_EXPR_TOKEN_OR,
    UT_EXPR_TOKEN_NOT,
    UT_EXPR_TOKEN_SCOPE,
    UT_EXPR_TOKEN_TREE,
    UT_EXPR_TOKEN_SEPARATOR
} ut_exprToken;

typedef struct ut_exprOp {
    ut_exprToken token;
    char *start;
    bool containsWildcard;
} ut_exprOp;

struct ut_expr_program_s {
    int kind; /* 0 = default, 1 = identifier, 2 = this, 3 = /, 4 = // */
    ut_exprOp ops[UT_EXPR_MAX_OP];
    uint8_t size;
    char *tokens;
};

int16_t ut_exprParseIntern(
    ut_expr_program data,
    const char *expr,
    bool allowScopes,
    bool allowSeparators);

#endif
