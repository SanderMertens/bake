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

/** @file
 * @section config Configuration type
 * @brief Configuration settings loaded from bake configuration file.
 **/

#ifndef BAKE_CONFIG_H_
#define BAKE_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bake_bundle {
    char *project;
    char *id;
} bake_bundle;

typedef struct bake_repository {
    char *id;
    char *url;
    char *branch;
    char *commit;
    char *tag;
    char *project;
    char *bundle;
} bake_repository;

struct bake_config {
    const char *environment;    /* Id of environment in use */
    const char *configuration;  /* Id of configuration in use */
    const char *architecture;   /* Id of architecture in use */

    /* Configuration attributes */
    bool symbols;               /* Enable symbols in binaries */
    bool debug;                 /* Enable debug information in binaries */
    bool optimizations;         /* Enable optimizations in binaries */
    bool coverage;              /* Enable code coverage in binaries */
    bool strict;                /* Enable strict compiler settings */
    bool static_lib;            /* Enable static linking */
    bool sanitize_memory;       /* Enable memory sanitizer (if supported) */
    bool sanitize_thread;       /* Enable thread sanitizer (if supported) */
    bool sanitize_undefined;    /* Enable UB sanitizier (if supported) */

    /* Environment attribubtes */
    ut_ll env_variables;        /* List with environment variable names */
    ut_ll env_values;           /* List with environment variable values */

    /* Bundle information */
    ut_ll bundles;           /* Bundles loaded by bake */
    ut_rb repositories;      /* Repositories loaded from bundles */

    /* Set by configuration loader */
    char *home;              /* $BAKE_HOME */
    char *meta;              /* $BAKE_HOME/meta */
    char *tmpl;              /* $BAKE_HOME/template */
    char *platform;          /* $BAKE_HOME/<platform> */
    char *target;            /* $BAKE_HOME/<platform>/<configuration> */
    char *bin;               /* $BAKE_HOME/<platform>-<configuration>/bin */
    char *lib;               /* $BAKE_HOME/<platform>-<configuration>/lib */
};

#ifdef __cplusplus
}
#endif

#endif
