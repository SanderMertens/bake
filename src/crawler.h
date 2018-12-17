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

typedef struct bake_crawler bake_crawler;

typedef int (*bake_crawler_cb)(
    bake_config *config,
    bake_crawler *_this,
    bake_project *project);

/** Create a new crawler.
 *
 * @return New crawler object.
 */
bake_crawler* bake_crawler_new(
    bake_config *cfg);

/** Free crawler.
 *
 * @return New crawler object.
 */
void bake_crawler_free(
    bake_crawler *_this);

/** Search a path for projects.
 *
 * @param _this A crawler object.
 * @param path A path to search.
 * @return 0 if success, non-zero if failed.
 */
uint32_t bake_crawler_search(
    bake_crawler *_this,
    const char *path);

/** Count number of projects found by searches.
 *
 * @param _this A crawler object.
 * @return Number of projects found.
 */
uint32_t bake_crawler_count(
    bake_crawler *_this);

/** Manually add a project to the crawler.
 *
 * @param _this A crawler object.
 * @param project Project to add.
 * @return 0 if success, non-zero if failed.
 */
 int16_t bake_crawler_add(
     bake_crawler *_this,
     bake_project *project);

 /** Test if crawler has a project.
  *
  * @param _this A crawler object.
  * @param id Id of project to check.
  * @return true if project has been added, false if not.
  */
  bool bake_crawler_has(
      bake_crawler *_this,
      const char *id);

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
    bake_config *config,
    bake_crawler *_this,
    const char *action_name,
    bake_crawler_cb action);
