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

#ifndef UT_RBTREE_H_
#define UT_RBTREE_H_

#include "jsw_rbtree.h"

#ifdef __cplusplus
extern "C" {
#endif

UT_API
ut_rb ut_rb_new(
    ut_equals_cb compare,
    void *ctx);

UT_API
void ut_rb_free(
    ut_rb tree);

UT_API
void* ut_rb_find(
    ut_rb tree,
    const void* key);

UT_API
void* ut_rb_findPtr(
    ut_rb tree,
    const void* key);

UT_API
void ut_rb_set(
    ut_rb tree,
    const void* key,
    void* value);

#define ut_rb_findOrSet(tree, key, value)\
    jsw_rbinsert((jsw_rbtree_t*)tree, (void*)key, value, FALSE, FALSE)

#define ut_rb_findOrSetPtr(tree, key)\
    jsw_rbinsert((jsw_rbtree_t*)tree, (void*)key, NULL, FALSE, TRUE)

UT_API
void* ut_rb_remove(
    ut_rb tree,
    void* key);

UT_API
bool ut_rb_hasKey(
    ut_rb tree,
    const void* key,
    void** value);

UT_API
bool ut_rb_hasKey_w_cmp(
    ut_rb tree,
    const void* key,
    void** value,
    ut_equals_cb cmp);

UT_API
void* ut_rb_min(
    ut_rb tree,
    void** key_out);

UT_API
void* ut_rb_max(
    ut_rb tree,
    void** key_out);

UT_API
void* ut_rb_next(
    ut_rb tree,
    void* key,
    void** key_out);

UT_API
void* ut_rb_prev(
    ut_rb tree,
    void* key,
    void** key_out);

UT_API
uint32_t ut_rb_count(
    ut_rb tree);

UT_API
int ut_rb_walk(
    ut_rb tree,
    ut_elementWalk_cb callback,
    void* userData);

UT_API
int ut_rb_walkPtr(
    ut_rb tree,
    ut_elementWalk_cb callback,
    void* userData);

#define ut_rb_iter(tree) _ut_rb_iter(tree, alloca(sizeof(struct jsw_rbtrav)));
UT_API ut_iter _ut_rb_iter(ut_rb tree, void *ctx);
UT_API bool ut_rb_iterChanged(ut_iter *iter);

#ifdef __cplusplus
}
#endif

#endif /* UT_RBTREE_H_ */
