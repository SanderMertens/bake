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

#ifndef BAKE_ATTRIBUTE_H_
#define BAKE_ATTRIBUTE_H_

#include "../util/include/util.h"

typedef enum bake_attribute_kind {
    BAKE_BOOLEAN,
    BAKE_STRING,
    BAKE_NUMBER,
    BAKE_ARRAY
} bake_attribute_kind;

typedef struct bake_attribute {
    char *name;
    bake_attribute_kind kind;
    union {
        bool boolean;
        char *string;
        double number;
        ut_ll array;
    } is;
} bake_attribute;

bake_attribute *bake_attribute_get(
    ut_ll attributes,
    const char *name);

#endif
