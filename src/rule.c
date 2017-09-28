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

bake_pattern* bake_pattern_new(
    const char *name, 
    const char *pattern)
{
    bake_pattern *result = corto_calloc(sizeof(bake_pattern));
    result->super.name = name;
    result->pattern = pattern;
    return result;
}

bake_rule* bake_rule_new(
    const char *name, 
    const char *source, 
    bake_rule_target target, 
    bake_rule_action_cb action) 
{
    bake_rule *result = corto_calloc(sizeof(bake_rule));
    result->super.name = name;
    result->source = source;
    result->target = target;
    result->action = action;
    return result;
}

bake_dependency_rule* bake_dependency_rule_new(
    const char *name, 
    const char *deps, 
    bake_rule_target dep_mapping, 
    bake_rule_action_cb action)
{
    bake_dependency_rule *result = corto_calloc(sizeof(bake_dependency_rule));
    result->super.name = name;
    result->deps = deps;
    result->dep_mapping = dep_mapping;
    result->action = action;
    return result;
}


