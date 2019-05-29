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

#ifndef JSW_RBTREE_H
#define JSW_RBTREE_H

/*
  Red Black balanced tree library

    > Created (Julienne Walker): August 23, 2003
    > Modified (Julienne Walker): March 14, 2008
    > Modified (Sander Mertens): 2010 - 2017

  This code is in the public domain. Anyone may
  use it or change it in any way that they see
  fit. The author assumes no responsibility for
  damages incurred through use of the original
  code or any variations thereof.

  It is requested, but not required, that due
  credit is given to the original author and
  anyone who has modified the code through
  a header comment, such as this one.
*/

#ifdef __cplusplus
#include <cstddef>

using std::size_t;

extern "C" {
#else
#include <stddef.h>
#endif

/* User-defined item handling */
typedef void *(*dup_f) ( void *p );
typedef void  (*rel_f) ( void *p );

/* Red Black tree functions */
jsw_rbtree_t *jsw_rbnew ( void *ctx, ut_equals_cb cmp);
void          jsw_rbdelete ( jsw_rbtree_t *tree );
void         *jsw_rbctx( jsw_rbtree_t *tree);
void         *jsw_rbfind ( jsw_rbtree_t *tree, const void *key );
void         *jsw_rbfindPtr ( jsw_rbtree_t *tree, const void *key );
int           jsw_rbhaskey ( jsw_rbtree_t *tree, const void *key, void** data );
int           jsw_rbhaskey_w_cmp ( jsw_rbtree_t *tree, const void *key, void** data, ut_equals_cb f_cmp );

void*         jsw_rbinsert ( jsw_rbtree_t *tree, void* key, void *data, bool overwrite, bool returnPtr );
void*         jsw_rberase ( jsw_rbtree_t *tree, void *key );
size_t        jsw_rbsize ( jsw_rbtree_t *tree );

/* Get minimum and maximum */
void         *jsw_rbgetmin ( jsw_rbtree_t *tree, void** key_out);
void         *jsw_rbgetmax ( jsw_rbtree_t *tree, void** key_out);

/* Get next and prev */
void         *jsw_rbgetnext ( jsw_rbtree_t *tree, void* key, void** key_out);
void         *jsw_rbgetprev ( jsw_rbtree_t *tree, void* key, void** key_out);

/* Traversal functions */
jsw_rbtrav_t *jsw_rbtnew ( void );
void          jsw_rbtdelete ( jsw_rbtrav_t *trav );
void         *jsw_rbtfirst ( jsw_rbtrav_t *trav, jsw_rbtree_t *tree );
void         *jsw_rbtfirstptr ( jsw_rbtrav_t *trav, jsw_rbtree_t *tree );
void         *jsw_rbtlast ( jsw_rbtrav_t *trav, jsw_rbtree_t *tree );
void         *jsw_rbtnext ( jsw_rbtrav_t *trav );
void         *jsw_rbtnextptr ( jsw_rbtrav_t *trav );
void         *jsw_rbtprev ( jsw_rbtrav_t *trav );
bool          jsw_rbtchanged( jsw_rbtrav_t *trav );

void *jsw_rbnodedata(jsw_rbnode_t *node);

#ifdef __cplusplus
}
#endif

#endif
