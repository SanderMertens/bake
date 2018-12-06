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

#include "../include/util.h"

ut_rb ut_rb_new(ut_equals_cb compare, void *ctx) {
    return (ut_rb)jsw_rbnew(ctx, compare);
}

void ut_rb_free(ut_rb tree) {
    jsw_rbdelete((jsw_rbtree_t*)tree);
}

void* ut_rb_find(ut_rb tree, const void* key) {
    return jsw_rbfind((jsw_rbtree_t*)tree, key);
}

void* ut_rb_findPtr(ut_rb tree, const void* key) {
    return jsw_rbfindPtr((jsw_rbtree_t*)tree, key);
}

void ut_rb_set(ut_rb tree, const void* key, void* value) {
    jsw_rbinsert((jsw_rbtree_t*)tree, (void*)key, value, TRUE, FALSE);
}

void* ut_rb_remove(ut_rb tree, void* key) {
    return jsw_rberase((jsw_rbtree_t*)tree, key);
}

bool ut_rb_hasKey(ut_rb tree, const void* key, void** value) {
    return jsw_rbhaskey((jsw_rbtree_t*)tree, key, value);
}

bool ut_rb_hasKey_w_cmp(ut_rb tree, const void* key, void** value, ut_equals_cb cmp) {
    return jsw_rbhaskey_w_cmp((jsw_rbtree_t*)tree, key, value, cmp);
}

uint32_t ut_rb_count(ut_rb tree) {
    return tree ? jsw_rbsize((jsw_rbtree_t*)tree) : 0;
}

void* ut_rb_min(ut_rb tree, void** key_out){
    return jsw_rbgetmin((jsw_rbtree_t*)tree, key_out);
}

void* ut_rb_max(ut_rb tree, void** key_out) {
    return jsw_rbgetmax((jsw_rbtree_t*)tree, key_out);
}

void* ut_rb_next(ut_rb tree, void* key, void** key_out) {
    return jsw_rbgetnext((jsw_rbtree_t*)tree, key, key_out);
}

void* ut_rb_prev(ut_rb tree, void* key, void** key_out)  {
    return jsw_rbgetprev((jsw_rbtree_t*)tree, key, key_out);
}

/* Note that this function cannot handle NULL values in the tree */
int ut_rb_walk(ut_rb tree, ut_elementWalk_cb callback, void* userData) {
    jsw_rbtrav_t tdata;
    void* data;

    /* Move to first */
    data = jsw_rbtfirst(&tdata, (jsw_rbtree_t*)tree);
    if (data) {
        if (!callback(data, userData)) {
            return 0;
        }

        /* Walk values */
        while((data = jsw_rbtnext(&tdata))) {
            if (!callback(data, userData)) {
                return 0;
            }
        }
    }

    return 1;
}

/* Note that this function cannot handle NULL values in the tree */
int ut_rb_walkPtr(ut_rb tree, ut_elementWalk_cb callback, void* userData) {
    jsw_rbtrav_t tdata;
    void* data;

    /* Move to first */
    data = jsw_rbtfirstptr(&tdata, (jsw_rbtree_t*)tree);
    if (data) {
        if (!callback(data, userData)) {
            return 0;
        }

        /* Walk values */
        while((data = jsw_rbtnextptr(&tdata))) {
            if (!callback(data, userData)) {
                return 0;
            }
        }
    }

    return 1;
}

#define ut_iterData(iter) ((jsw_rbtrav_t*)iter->ctx)

static bool ut_rb_iterHasNext(ut_iter *iter) {
    return ut_iterData(iter)->it != NULL;
}

static void* ut_rb_iterNext(ut_iter *iter) {
    void* data = ut_iterData(iter)->it ?
        jsw_rbnodedata(ut_iterData(iter)->it) : NULL;
    jsw_rbtnext(ut_iterData(iter));
    return data;
}

bool ut_rb_iterChanged(ut_iter *iter) {
    if (ut_iterData(iter)) {
        return jsw_rbtchanged(ut_iterData(iter));
    } else {
        return FALSE;
    }
}

ut_iter _ut_rb_iter(ut_rb tree, void *ctx) {
    ut_iter result;

    result.ctx = ctx;
    jsw_rbtfirst(result.ctx, (jsw_rbtree_t*)tree);
    result.hasNext = ut_rb_iterHasNext;
    result.next = ut_rb_iterNext;
    result.release = NULL;

    return result;
}
