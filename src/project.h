/* Copyright (c) 2010-2018 the corto developers
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

bake_project* bake_project_new(
    const char *path,
    bake_config *cfg);

void bake_project_free(
    bake_project *p);

char* bake_project_binaryPath(
    bake_project *p);

char* bake_project_includePath(
    bake_project *p);

char* bake_project_etcPath(
    bake_project *p);

void bake_project_addSource(
    bake_project *p,
    const char *source);

void bake_project_addInclude(
    bake_project *p,
    const char *include);

void bake_project_use(
    bake_project *p,
    const char *use);

char *bake_project_definitionFile(
    bake_project *p);

int16_t bake_project_parse_attributes(
    bake_project *p);

int16_t bake_project_loadDependeeConfig(
    bake_project *p,
    const char *package_id,
    const char *file);
