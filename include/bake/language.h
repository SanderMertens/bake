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
 * @section language Builder framework for bake.
 * @brief The language provides an interface for building bake plugins.
 */

#ifndef BAKE_LANGUAGE_H_
#define BAKE_LANGUAGE_H_

#include <bake/rule.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t (*bake_language_cb)(
    bake_language _this,
    void *ctx);

typedef int16_t (*bake_language_chain_cb)(
    bake_language _this,
    void *ctx);

struct bake_language_s {
    char *name;
    char *package;
    corto_dl dl;

    void (*pattern)(const char *name, const char *pattern);
    void (*rule)(const char *name, const char *source, bake_rule_target target, bake_rule_action_cb action);
    void (*dependency_rule)(const char *name, const char *deps, bake_rule_target dep_mapping, bake_rule_action_cb action);
    void (*artefact)(bake_rule_artefact_cb action);
    void (*standalone_artefact)(bake_rule_artefact_cb action);
    void (*condition)(const char *name, bake_rule_condition_cb cond);
    void (*setup_project)(bake_rule_setup_project_cb action);
    void (*init)(bake_rule_init_cb action);
    void (*clean)(bake_rule_clean_cb action);

    bake_rule_target (*target_pattern)(const char *pattern);
    bake_rule_target (*target_map)(bake_rule_map_cb mapping);

    void (*exec)(const char *cmd);

    bake_rule_init_cb init_cb;
    bake_rule_artefact_cb artefact_cb;
    bake_rule_artefact_cb standalone_artefact_cb;
    bake_rule_clean_cb clean_cb;
    bake_rule_setup_project_cb setup_project_cb;

    corto_ll nodes;
    int16_t error;
};

/** Add a pattern to a language.
 * A pattern is a idmatch-string matching a set of files. Patterns can be used
 * in rules to indicate a source or target. When a pattern is used as a
 * target in a rule and it does not match any files, the rule is executed.
 *
 * @param l The language object.
 * @param name The name of the rule
 * @param pattern A pattern in idmatch format.
 */
void bake_language_pattern(
    bake_language *l,
    const char *name,
    const char *pattern);

/** Add a rule to a language.
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
 * @param l The language object.
 * @param name The name of the rule
 * @param source A pattern indicating a source.
 * @param target The generated target files for the rule.
 * @param action The action to generate target from source.
 */
void bake_language_rule(
    bake_language *l,
    const char *name,
    const char *source,
    bake_rule_target target,
    bake_rule_action_cb action);

/** Add a dependency rule to a language.
 * Dependency rules allow for programatically adding dependencies to a target
 * that cannot be derived from a list of files. The requirement that introduced
 * this feature is to rebuild source files when one of the included header files
 * change.
 *
 * @param l The language object.
 * @param target The target for which to specify the dependencies.
 * @param dependencies A target indicating the source of the dependency information.
 * @param dep_mapping The generated target files for the rule.
 * @param action The action to generate target from source.
 */
void bake_language_dependency_rule(
    bake_language *l,
    const char *target,
    const char *deps,
    bake_rule_target dep_mapping,
    bake_rule_action_cb action);

/** Initialize a project (calls language initializer).
 *
 * @param l The language object.
 * @param p The project to initialize.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_language_init(
    bake_language *l,
    bake_project *p);

/** Setup a directory as a bake project.
 *
 * @param l The language object.
 * @param p The project to initialize.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_language_setup_project(
    bake_language *l,
    const char *id,
    bake_project_kind kind);

/** Generate code for a project.
 *
 * @param l The language object.
 * @param p The project for which to generate code.
 * @param c The build configuration.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_language_generate(
    bake_language *l,
    bake_project *p,
    bake_config *c);

/** Build a project of the specified language.
 *
 * @param l The language object.
 * @param p The project to build.
 * @param c The build configuration.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_language_build(
    bake_language *l,
    bake_project *p,
    bake_config *c);

/** Clean a project of the specified language.
 *
 * @param l The language object.
 * @param p The project to clean.
 * @return 0 if success, non-zero if failed.
 */
int16_t bake_language_clean(
    bake_language *l,
    bake_project *p);

/** Obtain artefact for a project
 *
 * @param l The language object.
 * @param p The project for which to obtain the artefact.
 * @return A string containing the name of the (installed) artefact or NULL if failed.
 */
char* bake_language_artefact(
    bake_language *l,
    bake_project*p);

/** Obtain standalone artefact for a project
 *
 * @param l The language object.
 * @param p The project for which to obtain the artefact.
 * @return A string containing the name of the (installed) artefact or NULL if failed.
 */
char* bake_language_standalone_artefact(
    bake_language *l,
    bake_project*p);

#ifdef __cplusplus
}
#endif

#endif
