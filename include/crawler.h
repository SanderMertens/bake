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
 * @section crawler Crawler API
 * @brief API that searches a directory structure for corto projects.
 */

#ifndef BAKE_CRAWLER_H_
#define BAKE_CRAWLER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bake_crawler_s* bake_crawler;

typedef int (*bake_crawler_cb)(bake_crawler _this, bake_project *project, void *ctx);

/** Create a new crawler 
 * 
 * @return New crawler object.
 */
bake_crawler bake_crawler_new(void);

/** Search a path for projects.
 * 
 * @param _this A crawler object.
 * @param path A path to search.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_crawler_search(
    bake_crawler _this, 
    const char *path);

/** Walk projects.
 * This walks projects found with bake_crawler_search in correct dependency
 * order. 
 *
 * @param _this A crawler object.
 * @param action Callback to invoke when project is found.
 * @param ctx Context passed to action.
 * @return non-zero if success, zero if interrupted.
 */
int16_t bake_crawler_walk(
    bake_crawler _this, 
    bake_crawler_cb action, 
    void *ctx);

#ifdef __cplusplus
}
#endif

#endif
