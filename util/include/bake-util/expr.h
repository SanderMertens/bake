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

/** @file
 * @section idmatch Id matching API
 * @brief API for matching corto identifiers against a pattern
 */

#ifndef UT_EXPR_H
#define UT_EXPR_H

typedef struct ut_expr_program_s* ut_expr_program;

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


/** Match an id against a pattern.
 * Simple function that tests if an object id matches the provided pattern.
 *
 * @param pattern The pattern against which to match the object identifier
 * @param id The object identifier to match
 * @return true if the identifier matches, false if it does not match
 * @see ut_expr_compile ut_expr_run ut_expr_free
 */
UT_API
bool ut_expr(
    const char *pattern,
    const char *id);

/** Compile an id expression.
 * When a pattern needs to be evaluated against multiple strings, it is faster
 * to compile it first with the ut_expr_compile function. Once compiled,
 * strings can be matched with the ut_expr_run function. When the program
 * is no longer necessary, it should be freed with ut_expr_free.
 *
 * @param pattern The pattern against which to match the object identifier
 * @pattern allowScopes Determines whether `/` and `//` are allowed in the pattern
 * @pattern allowSeparators Determines whether `,` is allowed in a pattern
 * @return A compiled version of the string pattern.
 * @see ut_expr_run ut_expr_free ut_expr
 */
UT_API
ut_expr_program ut_expr_compile(
    const char *pattern,
    bool allowScopes,
    bool allowSeparators);

/** Run a compiled idmatch program.
 *
 * @param program A compiled program, created by ut_expr_compile
 * @pattern id The object identifier to match
 * @return true if the identifier matches, false if it does not match
 * @see ut_expr ut_expr_compile ut_expr_free
 */
UT_API
bool ut_expr_run(
    ut_expr_program program,
    const char *id);

/** Return if program matches single object, n objects or a tree of objects.
 *
 * @param program A compiled program, created by ut_expr_compile
 * @return 0 if single object, 1 if multiple objects and 2 if tree is matched.
 * @see ut_expr ut_expr_compile ut_expr_free
 */
UT_API
int ut_expr_scope(
    ut_expr_program program);

/** Free a compiled program.
 * The ut_expr_run function evaluates an identifier against a compiled
 * pattern program. The compiled program can be reused multiple times for
 * different identifiers.
 *
 * @param program A compiled program, created by ut_expr_compile
 * @pattern id The object identifier to match
 * @return true if the identifier matches, false if it does not match
 * @see ut_expr ut_expr_compile ut_expr_free
 */
UT_API
void ut_expr_free(
    ut_expr_program program);

/** Match parent of an object identifier.
 * The ut_expr_parent function matches a specified parent identifier with
 * an object identifier. If ther is a match, the functino returns the remainder.
 *
 * @param parent An object identifier representing a parent
 * @pattern expr An object identifier
 * @return the remainder if the parent matches, NULL if it does not match
 */
UT_API
const char* ut_matchParent(
    const char *parent,
    const char *expr);

/** Utility function to test if character is an operator in idmatch expression.
 * Operators are all special characters besides the normal characters that may
 * appear in an object identifier, like ^, |, &.
 *
 * @param ch The character to test
 * @return true if an operator, false if not.
 */
UT_API
bool ut_expr_isOperator(
    char ch);

/** Utility function to test if idmatch expression has operators.
 * Operators are all special characters besides the normal characters that may
 * appear in an object identifier, like ^, |, &. A single / will not be treated
 * as operator, but a double // (recursive match) will.
 *
 * @param expr The expression to test
 * @return true if it contains operators, false if it does not.
 */
UT_API
bool ut_expr_hasOperators(
    const char *expr);

/** Determine whether a pattern matches an object, scope or tree.
 * Learns from a compiled pattern if it matches a tree (`foo//`), a
 * scope (`foo/`) or an object (`foo/bar`).
 *
 * @param program The program to evaluate
 * @return UT_ON_SELF if matching an object, UT_ON_SCOPE if matching a
 * scope, and UT_ON_TREE if matching a tree.
 */
UT_API
int ut_expr_get_scope(
    ut_expr_program program);

UT_API
int16_t ut_exprParseIntern(
    ut_expr_program data,
    const char *expr,
    bool allowScopes,
    bool allowSeparators);

#endif
