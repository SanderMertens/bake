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

#include "bake.h"

bake_project* bake_project_new(
    const char *path,
    const char *projectConfig)    
{
    bake_project* result = corto_calloc(sizeof (bake_project));
    result->path = strdup(path);
    result->projectConfig = projectConfig ? strdup(projectConfig) : NULL;
    return result;
}

char* bake_project_binaryPath(
    bake_project *p)
{
    char *kind, *subdir;
    if (p->kind == BAKE_APPLICATION) {
        kind = "bin";
        subdir = "cortobin";
    } else if (p->kind == BAKE_LIBRARY) {
        kind = "lib";
        subdir = "corto";
    } else if (p->kind == BAKE_TOOL) {
        return corto_envparse(
            "$CORTO_TARGET/bin");
    } else {
        corto_seterr("unsupported project kind '%s'", p->kind);
        return NULL;
    }

    return corto_envparse(
        "$CORTO_TARGET/%s/%s/$CORTO_VERSION/%s", 
        kind,
        subdir,
        p->id);
}

char* bake_project_includePath(
    bake_project *p)
{
    return corto_envparse(
        "$CORTO_TARGET/include/corto/$CORTO_VERSION/%s", 
        p->id);
}

char* bake_project_etcPath(
    bake_project *p)
{
    return corto_envparse(
        "$CORTO_TARGET/etc/corto/$CORTO_VERSION/%s", 
        p->id);
}
