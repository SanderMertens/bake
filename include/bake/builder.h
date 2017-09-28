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
 * @section builder Builder framework for bake.
 * @brief The builder provides an interface for building bake plugins.
 */

#ifndef BAKE_BUILDER_H_
#define BAKE_BUILDER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bake_builder bake_builder;

typedef int16_t (*bake_builder_cb)(
    bake_builder _this,
    void *ctx);

typedef int16_t (*bake_builder_chain_cb)(
    bake_builder _this,
    void *ctx);

struct bake_builder {
    char *package;
    corto_dl dl;

    void (*pattern)(const char *name, const char *pattern);
    void (*rule)(const char *name, const char *source, bake_rule_target target, bake_rule_action_cb action);
    void (*dependency_rule)(const char *name, const char *deps, bake_rule_target dep_mapping, bake_rule_action_cb action);

    bake_rule_target (*target_1)(const char *target);
    bake_rule_target (*target_pattern)(const char *pattern);
    bake_rule_target (*target_n)(bake_rule_map_cb mapping);
};

/** Add a pattern to a builder.
 * A pattern is a idmatch-string matching a set of files. Patterns can be used
 * in rules to indicate a source or target. When a pattern is used as a
 * target in a rule and it does not match any files, the rule is executed.
 *
 * @param b The builder object.
 * @param name The name of the rule
 * @param pattern A pattern in idmatch format.
 */
void bake_builder_pattern(
    bake_builder *b, 
    const char *name, 
    const char *pattern);

/** Add a rule to a builder.
 * A rule specifies a relationship between a source and a target that indicates
 * to bake whether a target needs rebuilding. When a target needs rebuilding,
 * a specified rule action is invoked.
 *
 * The rule action will be invoked when either the source is newer than the
 * target or if the target does not exist. If the target is a pattern that does
 * not match any files, the rule action will also be invoked.
 *
 * Three kind of targets can be specifed: 1, n and pattern. The diferent kinds
 * indicate the cardinality of the target. "1" means there is exactly one target
 * for the specified sources. "n" means that for each source there is a target.
 * "pattern" indicate s
 *
 * @param b The builder object.
 * @param name The name of the rule
 * @param source A pattern indicating a source.
 * @param target The generated target files for the rule.
 * @param action The action to generate target from source.
 */
void bake_builder_rule(
    bake_builder *b, 
    const char *name, 
    const char *source,
    bake_rule_target target,
    bake_rule_action_cb action);

/** Add a dependency rule to a builder.
 * Dependency rules allow for programatically adding dependencies to a target
 * that cannot be derived from a list of files. The requirement that introduced
 * this feature is to rebuild source files when one of the included header files
 * change.
 *
 * @param b The builder object.
 * @param target The target for which to specify the dependencies.
 * @param dependencies A target indicating the source of the dependency information.
 * @param dep_mapping The generated target files for the rule.
 * @param action The action to generate target from source.
 */
void bake_builder_dependency_rule(
    bake_builder *b, 
    const char *target, 
    bake_rule_target *deps,
    bake_rule_map_cb dep_mapping,
    bake_rule_action_cb action);

#ifdef __cplusplus
}
#endif

#endif
