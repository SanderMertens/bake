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

#define get(list, index) ut_ll_get(list, index)
#define walk(list, cb, ctx) ut_ll_walk(list, cb, ctx)

#define next(iter) ut_ll_iterNext(&iter)
#define remove(iter) ut_ll_iterRemove(&iter)
#define hasNext(iter) ut_ll_iterHasNext(&iter)
#define insert(iter, data) ut_ll_iterInsert(&iter, data)
#define set(iter) ut_ll_iterSet(&iter)

#define ut_iterData(iter) ((ut_ll_iter_s*)(iter).ctx)

/* New list */
ut_ll ut_ll_new() {
    ut_ll result = (ut_ll)malloc(sizeof(ut_ll_s));

    result->first = 0;
    result->last = 0;
    result->last = 0;
    result->size = 0;

    return result;
}

/* Get listsize */
int ut_ll_count(ut_ll list) {
    return list ? list->size : 0;
}

void ut_ll_free(ut_ll list) {
    if (!list) {
        return;
    }

    ut_ll_node node, next;
    node = list->first;
    while (node) {
        next = node->next;
        free(node);
        node = next;
    }

    free(list);
}

int ut_ll_walk(ut_ll list, ut_elementWalk_cb callback, void* userdata) {
    ut_ll_node next;
    ut_ll_node ptr;
    int result;
    uint32_t i=0;

    ptr = list->first;
    result = 1;

    while (ptr) {
        i++;
        next = ptr->next;
        result = callback(ptr->data, userdata);
        if (!result) {
            break;
        }
        ptr = next;
    }

    return result;
}

int ut_ll_walkPtr(ut_ll list, ut_elementWalk_cb callback, void* userdata) {
    ut_ll_node next;
    ut_ll_node ptr;
    int result;
    uint32_t i=0;

    ptr = list->first;
    result = 1;

    while (ptr) {
        i++;
        next = ptr->next;
        result = callback(&ptr->data, userdata);
        if (!result) {
            break;
        }
        ptr = next;
    }

    return result;
}

/* Insert at start */
void* ut_ll_insert(ut_ll list, void* data) {
    ut_iter iter = ut_ll_iter(list);
    return insert(iter, data);
}

/* Insert at end */
void* ut_ll_append(ut_ll list, void* data) {
    ut_ll_node node = list->first;
    ut_iter iter = ut_ll_iter(list);

    if (node) {
        node = list->last;
    }

    ut_iterData(iter)->cur = node;
    ut_iterData(iter)->next = 0;

    return insert(iter, data);
}

void ut_ll_set(ut_ll list, int index, void *value) {
    ut_ll_node node = list->first;
    int i = 0;

    while(node && (i < index)) {
        node = node->next;
        i++;
    }
    if (node) {
        node->data = value;
    }
}

/* Random access read */
void* ut_ll_get(ut_ll list, int index) {
    ut_ll_node node = list->first;
    void* result = NULL;
    int i = 0;

    while(node && (i < index)) {
        node = node->next;
        i++;
    }
    if (node) {
        result = node->data;
    }
    return result;
}

/* Get element ptr */
void* ut_ll_getPtr(ut_ll list, int index) {
    ut_ll_node node;
    void* result;
    int i;

    result = 0;
    i = 0;
    node = list->first;

    while(node && (i < index)) {
        node = node->next;
        i++;
    }
    if (node) {
        result = &node->data;
    }

    return result;
}

void* ut_ll_findPtr(ut_ll list, ut_compare_cb callback, void* o) {
    ut_ll_node ptr;
    void* result;

    ptr = list->first;
    result = 0;

    while (ptr) {
        if (!callback(o, ptr->data)) {
            result = &ptr->data;
            break;
        }
        ptr = ptr->next;
    }

    return result;
}

void* ut_ll_find(ut_ll list, ut_compare_cb callback, void* o) {
    void *result = ut_ll_findPtr(list, callback, o);
    return result ? *(void**)result : NULL;
}

uint32_t ut_ll_hasObject(ut_ll list, void* o) {
    ut_ll_node ptr;
    uint32_t index = 0;

    ptr = list->first;

    while (ptr) {
        if (o == ptr->data) {
            break;
        }
        ptr = ptr->next;
        index++;
    }

    return ptr ? index+1 : 0;
}

/* Last element */
void* ut_ll_last(ut_ll list) {
    if (list->last) {
        return list->last->data;
    }
    return 0;
}

/* Take first */
void* ut_ll_takeFirst(ut_ll list) {
    void* data;
    ut_ll_node node;

    node = list->first;
    data = 0;

    if (node) {
        data = node->data;
        list->first = node->next;
        free(node);
        if (!list->first) {
            list->last = 0;
        }
        list->size--;
    }

    return data;
}

/* Take last */
void* ut_ll_takeLast(ut_ll list) {
    void* data;
    ut_ll_node node;

    node = list->last;
    data = 0;

    if (node) {
        data = node->data;
        list->last = node->prev;
        free(node);
        if (!list->last) {
            list->first = 0;
        }
        list->size --;
    }

    return data;
}

/* Remove object */
void* ut_ll_remove(ut_ll list, void* o) {
    ut_ll_node node, prev;
    void *result = NULL;

    prev = 0;
    node = list->first;

    while(node && !result) {
        if (node->data == o) {
            result = o;
            if (prev) {
                prev->next = node->next;
                if (!prev->next) {
                    list->last = prev;
                }
            } else {
                list->first = node->next;
                if (!node->next) {
                    list->last = list->first;
                }
            }
            free(node);
            list->size --;
            break;
        }
        prev = node;
        node = node->next;
    }

    return result;
}

void* ut_ll_remove_at(ut_ll list, uint32_t index) {
    ut_ll_node node, prev;
    void *result = NULL;

    prev = 0;
    node = list->first;

    unsigned int i;
    for (i = 0; node && i <= index; i ++) {
        if (i == index) {
            result = node->data;
            if (prev) {
                prev->next = node->next;
                if (!prev->next) {
                    list->last = prev;
                }
            } else {
                list->first = node->next;
                if (!node->next) {
                    list->last = list->first;
                }
            }
            free(node);
            list->size --;
        } else {
            prev = node;
            node = node->next;
        }
    }

    return result;
}

/* Replace object */
void ut_ll_replace(ut_ll list, void* src, void* by) {
    ut_ll_node node;

    node = list->first;
    while(node) {
        if (node->data == src) {
            node->data = by;
            break;
        }
        node = node->next;
    }
}

/* Append one list to another */
void ut_ll_appendList(ut_ll l1, ut_ll l2) {
    ut_ll_node ptr;

    ptr = l2->first;
    while(ptr) {
        ut_ll_insert(l1, ptr->data);
        ptr = ptr->next;
    }
}

/* Insert one list into another */
void ut_ll_insertList(ut_ll l1, ut_ll l2) {
    printf("TODO\n");
    (void)l1;
    (void)l2;
}

/* Reverse list */
void ut_ll_reverse(ut_ll list) {
    uint32_t i, size = ut_ll_count(list);
    ut_ll_node start = list->first;
    ut_ll_node end = list->last;
    ut_ll_node ptr;

    for(i=0; i<size / 2; i++) {
        void *tmp = start->data;
        start->data = end->data;
        end->data = tmp;
        start = start->next;

        /* Do in-place reverse, find node that precedes 'end' */
        if (start != end) {
            ptr = start;
            while(ptr && (ptr->next != end)) {
                ptr = ptr->next;
            }
            ut_assert(ptr != NULL, "linked list corrupt");
            end = ptr;
        }
    }
}

/* Copy list */
ut_ll ut_ll_copy(ut_ll list) {
    ut_iter iter = ut_ll_iter(list);
    ut_ll result = NULL;
    if (list) {
        result = ut_ll_new();
        while (ut_iter_hasNext(&iter)) {
            ut_ll_append(result, ut_iter_next(&iter));
        }
    }
    return result;
}

/* Clear list */
void ut_ll_clear(ut_ll list) {
    while(list->size) {
        ut_ll_takeFirst(list);
    }
}

/* Return list iterator */
ut_iter _ut_ll_iter(ut_ll list, void *ctx) {
    ut_iter result;

    result.ctx = ctx;
    ut_iterData(result)->cur = 0;
    ut_iterData(result)->next = list ? list->first : NULL;
    ut_iterData(result)->list = list;
    result.hasNext = ut_ll_iterHasNext;
    result.next = ut_ll_iterNext;
    result.nextPtr = ut_ll_iterNextPtr;
    result.release = NULL;

    return result;
}

ut_iter ut_ll_iterAlloc(ut_ll list) {
    ut_iter result;
    ut_ll_iter_s *ctx =  malloc(sizeof(ut_ll_iter_s));
    result = _ut_ll_iter(list, ctx);
    result.release = ut_ll_iterRelease;
    return result;
}

void ut_ll_iterRelease(ut_iter *iter) {
    ut_assert(iter->ctx != NULL, "iterator context not set");

    free(iter->ctx);
    iter->ctx = NULL;
}

void ut_ll_iterMoveFirst(ut_iter* iter) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    ut_iterData(*iter)->cur = 0;
    ut_iterData(*iter)->next = (ut_iterData(*iter)->list)->first;
}

void* ut_ll_iterMove(ut_iter* iter, unsigned int index) {
    ut_assert(iter->ctx != NULL, "iterator context not set");

    void* result;

    result = NULL;

    if (ut_iterData(*iter)->list->size <= index) {
        ut_critical("iterMove exceeds list-bound (%d >= %d).", index, (ut_iterData(*iter)->list)->size);
    }
    ut_ll_iterMoveFirst(iter);
    while(index) {
        result = ut_iter_next(iter);
        index--;
    }

    return result;
}

void *ut_ll_iterMoveFind(ut_iter *iter, ut_compare_cb comparator, void *data) {
    ut_assert(iter->ctx != NULL, "iterator context not set");

    ut_ll_iter_s *ctx = iter->ctx;
    ut_ll_node current = ctx->cur;
    void *result = NULL;

    while(ut_ll_iterHasNext(iter)) {
        void *o = ut_ll_iterNext(iter);
        if (comparator(o, data)) {
            result = o;
            break;
        }
    }

    /* If not found, start looking from beginning */
    if (!result) {
        ut_ll_iterMoveFirst(iter);

        while(ut_ll_iterHasNext(iter)) {
            void *o = ut_ll_iterNext(iter);

            /* If at old position, we've looped around. Break out of loop. */
            if (ctx->cur == current) {
                break;
            }

            if (comparator(o, data)) {
                result = o;
                break;
            }
        }
    }

    return result;
}

static int ut_ll_comparator_equals(void *o1, void *o2) {
    return o1 == o2;
}

bool ut_ll_iterMoveTo(ut_iter *iter, void *o) {
    return ut_ll_iterMoveFind(iter, ut_ll_comparator_equals, o) != NULL;
}

/* Can the iterator provide a 'next' value */
bool ut_ll_iterHasNext(ut_iter* iter) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    return ut_iterData(*iter)->next != 0;
}

/* Take next element of iterator */
void* ut_ll_iterNext(ut_iter* iter) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    ut_ll_node current;
    void* result;

    current = ut_iterData(*iter)->next;
    result = 0;

    if (current) {
        ut_iterData(*iter)->next = current->next;
        result = current->data;
        ut_iterData(*iter)->cur = current;
    } else {
        ut_critical(
            "Illegal use of 'next' by ut_iter. Use 'hasNext' to check if data is still available.");
    }

    return result;
}

/* Take next element of iterator */
void* ut_ll_iterNextPtr(ut_iter* iter) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    ut_ll_node current;
    void* result;

    current = ut_iterData(*iter)->next;
    result = 0;

    if (current) {
        ut_iterData(*iter)->next = current->next;
        result = &current->data;
        ut_iterData(*iter)->cur = current;
    } else {
        ut_critical("Illegal use of 'next' by ut_iter. Use 'hasNext' to check if data is still available.");
    }

    return result;
}

void* ut_ll_iterCurrent(ut_iter* iter) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    ut_ll_node node = ut_iterData(*iter)->cur;
    if (node) {
        return node->data;
    } else {
        return NULL;
    }
}

/* Remove the last-read element from the iterator. */
void* ut_ll_iterRemove(ut_iter* iter) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    ut_ll_node current;
    void* result;

    result = 0;

    if ((current = ut_iterData(*iter)->cur)) {
        if ((ut_iterData(*iter)->list)->first == current) {
            (ut_iterData(*iter)->list)->first = current->next;
        }
        if ((ut_iterData(*iter)->list)->last == current) {
            (ut_iterData(*iter)->list)->last = current->next;
        }
        if (current->prev) {
            current->prev->next = current->next;
        }
        if (current->next) {
            current->next->prev = current->prev;
        }
        result = current->data;
        free(current);
        (ut_iterData(*iter)->list)->size--;
    } else {
        ut_critical("Illegal use of 'remove' by ut_iter: no element selected. Use 'next' to select an element first.");
    }

    return result;
}

/* Insert element after current (update next of iterator) */
void* ut_ll_iterInsert(ut_iter* iter, void* o) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    ut_ll_node newNode;
    ut_ll_node current;
    ut_ll_node next;

    current = ut_iterData(*iter)->cur;
    next = ut_iterData(*iter)->next;

    newNode = malloc(sizeof(ut_ll_node_s));
    newNode->data = o;
    newNode->prev = current;
    newNode->next = next;
    if (!next) {
        (ut_iterData(*iter)->list)->last = newNode;
    }
    if (current) {
        current->next = newNode;
    } else {
        (ut_iterData(*iter)->list)->first = newNode;
    }
    if (next) {
        next->prev = newNode;
    }

    (ut_iterData(*iter)->list)->size++;
    ut_iterData(*iter)->next = newNode;

    return &newNode->data;
}

/* Set data of current element. */
void ut_ll_iterSet(ut_iter* iter, void* o) {
    ut_assert(iter->ctx != NULL, "iterator context not set");
    if (ut_iterData(*iter)->cur) {
        ((ut_ll_node)ut_iterData(*iter)->cur)->data = o;
    } else {
        ut_critical("Illegal use of 'set' by ut_iter: no element selected. Use 'next' to select an element first.");
    }
}

ut_ll ut_ll_map(ut_ll l, ut_mapAction f, void* data)
{
    ut_ll ll = ut_ll_new();
    {
        ut_iter i = ut_ll_iter(l);
        while (ut_iter_hasNext(&i)) {
            void* e = ut_iter_next(&i);
            void* r = f(e, data);
            ut_ll_append(ll, r);
        }
    }
    return ll;
}
