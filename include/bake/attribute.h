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
 * @section attribute Attribute API
 * @brief API for retrieving driver-specific configuration.
 **/

#ifndef bake_attr_H_
#define bake_attr_H_

typedef enum bake_attr_kind {
    BAKE_BOOLEAN,
    BAKE_STRING,
    BAKE_NUMBER,
    BAKE_ARRAY
} bake_attr_kind;

typedef struct bake_attr {
    char *name;
    bake_attr_kind kind;
    union {
        bool boolean;
        char *string;
        double number;
        ut_ll array;
    } is;
} bake_attr;

/** Get attribute from attribute list */
bake_attr *bake_attr_get(
    ut_ll attributes,
    const char *name);

/** Free attribute */
void bake_attr_free(
    bake_attr *attr);

/** Free attribute list */
void bake_attr_free_attr_array(
    ut_ll list);

/** Free list of strings (used for array attributes) */
void bake_attr_free_string_array(
    ut_ll list);


int16_t bake_attr_add(
    bake_config *config,
    bake_project *project,
    const char *project_id,
    ut_ll attributes,
    const char *name,
    JSON_Value *value);

#endif
