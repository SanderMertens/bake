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

/** @file
 * @section install Install files from project to package hierarchy
 * @brief The installer copies include & etc files to the package hierarchy.
 *        This is not the functionality that installs packages to the global
 *        package hierarchy.
 */

/** Copy static files to package hierarchy (pre build).
 *
 * @param id Project id.
 * @param projectPath Path to the project source directory.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_pre(
    bake_project *project);

/** Copy artefact to package hierarchy (post build).
 *
 * @param id Project id.
 * @param artefact Filename of the artefact to be copied.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_post(
    bake_project *project, 
    char *artefact);

/** Remove files from package hierarchy for project.
 *
 * @param id Project id.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_uninstall(
    bake_project *project);
