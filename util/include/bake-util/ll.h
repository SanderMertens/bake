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

#ifndef UT_LL_H_
#define UT_LL_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ut_ll_node_s* ut_ll_node;

typedef struct ut_ll_node_s {
    void* data;
    ut_ll_node next;
    ut_ll_node prev;
} ut_ll_node_s;

typedef struct ut_ll_s {
    ut_ll_node first;
    ut_ll_node last;
    unsigned int size;
} ut_ll_s;

typedef struct ut_ll_iter_s {
    ut_ll list;
    ut_ll_node cur;
    ut_ll_node next;
} ut_ll_iter_s;

UT_API ut_ll ut_ll_new(void);
UT_API void ut_ll_free(ut_ll);

/* Walk list */
UT_API int ut_ll_walk(ut_ll list, ut_elementWalk_cb callback, void* userdata);

/* Walk list, return pointers to elements */
UT_API int ut_ll_walkPtr(ut_ll list, ut_elementWalk_cb callback, void* userdata);

/* Insert at start */
UT_API void* ut_ll_insert(ut_ll list, void* data);

/* Insert at end */
UT_API void* ut_ll_append(ut_ll list, void* data);

/* Remove object */
UT_API void* ut_ll_remove_at(ut_ll list, uint32_t index);

/* Remove object */
UT_API void* ut_ll_remove(ut_ll list, void* o);

/* Replace object */
UT_API void ut_ll_replace(ut_ll list, void* o, void* by);

/* Take first */
UT_API void* ut_ll_takeFirst(ut_ll);

/* Take last */
UT_API void* ut_ll_takeLast(ut_ll);

/* Random access read */
UT_API void* ut_ll_get(ut_ll list, int index);

/* Random access write */
void ut_ll_set(ut_ll list, int index, void *value);

/* Get element ptr */
UT_API void* ut_ll_getPtr(ut_ll list, int index);

/* Find object - comparison by value */
UT_API void* ut_ll_find(ut_ll list, ut_compare_cb callback, void* o);

/* Find object, return ptr - comparison by value */
UT_API void* ut_ll_findPtr(ut_ll list, ut_compare_cb callback, void* o);

/* Check if object is in list - simple compare on address */
UT_API unsigned int ut_ll_hasObject(ut_ll list, void* o);

/* Last element */
UT_API void* ut_ll_last(ut_ll list);

/* Get listsize */
UT_API int ut_ll_count(ut_ll list);

/* Obtain regular iterator, not valid outside scope of origin. */
#define ut_ll_iter(list) _ut_ll_iter(list, alloca(sizeof(ut_ll_iter_s)));
UT_API ut_iter _ut_ll_iter(ut_ll, void *ctx);

/* Obtain persistent iterator. Requries ut_iter_release to be called */
UT_API ut_iter ut_ll_iterAlloc(ut_ll);

/* Iterator cleanup functions */
UT_API void ut_ll_iterRelease(ut_iter *iter);

/* Append one list to another */
UT_API void ut_ll_appendList(ut_ll l1, ut_ll l2);

/* Insert one list into another */
UT_API void ut_ll_insertList(ut_ll l1, ut_ll l2);

/* Reverse list */
UT_API void ut_ll_reverse(ut_ll list);

/* Clear list */
UT_API void ut_ll_clear(ut_ll list);

/* Copy list */
UT_API ut_ll ut_ll_copy(ut_ll list);

/* Iterator implementation */
UT_API void ut_ll_iterMoveFirst(ut_iter* iter);
UT_API void *ut_ll_iterMove(ut_iter* iter, unsigned int index);
UT_API void *ut_ll_iterMoveFind(ut_iter *iter, ut_compare_cb callback, void *data);
UT_API bool ut_ll_iterMoveTo(ut_iter *iter, void *o);
UT_API bool ut_ll_iterHasNext(ut_iter* iter);
UT_API void* ut_ll_iterNext(ut_iter* iter);
UT_API void* ut_ll_iterNextPtr(ut_iter* iter);
UT_API void* ut_ll_iterCurrent(ut_iter* iter);
UT_API void* ut_ll_iterRemove(ut_iter* iter);
UT_API void* ut_ll_iterInsert(ut_iter* iter, void* o);
UT_API void ut_ll_iterSet(ut_iter* iter, void* o);

/* Functional-style */
typedef void* (*ut_mapAction)(void* elem, void* data);
UT_API ut_ll ut_ll_map(ut_ll l, ut_mapAction f, void* data);


#ifdef __cplusplus
}
#endif

#endif
