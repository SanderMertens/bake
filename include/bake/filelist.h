/* Copyright (c) 2010-2017 the corto developers
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
 * @section filelist API for retrieving and comparing lists of files.
 * @brief A filelist is used to evaluate whether rule actions should be invoked.
 */

typedef struct bake_file {
    char *name;
    char *offset;
    uint64_t timestamp;
} bake_file;

typedef struct bake_filelist {
    char *pattern;
    corto_ll files;
    int16_t (*set)(const char *pattern);
} bake_filelist;

bake_filelist* bake_filelist_new(
    const char *path, const char *pattern);

void bake_filelist_free(
    bake_filelist* fl);

int16_t bake_filelist_set(
    bake_filelist *fl,
    const char *pattern);

corto_iter bake_filelist_iter(
    bake_filelist *fl);

bake_file* bake_filelist_add(
    bake_filelist *fl,
    const char *filename);

int16_t bake_filelist_addPattern(
    bake_filelist *fl,
    const char *offset,
    const char *pattern);

int16_t bake_filelist_addList(
    bake_filelist *fl,
    bake_filelist *src);

uint64_t bake_filelist_count(
    bake_filelist *fl);
