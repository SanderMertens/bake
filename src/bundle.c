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

#include "bake.h"

int16_t bake_use(
    bake_config *config, 
    const char *expr)
{
    char *pkg = ut_strdup(expr);
    char *tag = strrchr(pkg, ':');
    if (tag) {
        *tag = '\0';
        tag ++;
    }

    const char *path = ut_locate(pkg, NULL, UT_LOCATE_PROJECT);
    if (!path) {
        ut_error("cannot find bundle '%s'", pkg);
        goto error;
    }

    bool changed = false;
    ut_try( bake_config_use_bundle(config, pkg, tag, &changed), NULL);

    return 0;
error:
    return -1;
}