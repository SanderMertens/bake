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
 * @section File utility functions.
 * @brief Functions that make commonly used file-based operations easier.
 */

#ifndef UT_ENV_H
#define UT_ENV_H

#ifdef __cplusplus
extern "C" {
#endif

/** Set environment variable.
 * 
 * @param varname Name of environment variable
 * @param value Value to assign to environment variable.
 * @return 0 if success, non-zero if failed.
 */
UT_API 
int16_t ut_setenv(
    const char *varname, 
    const char *value, 
    ...);

/** Append a value to an environment variable.
 * 
 * Used for appending to PATH-like variables. A separator (':', ';') will be
 * added to the value.
 * 
 * @param varname Name of environment variable
 * @param value Value to append to environment variable.
 * @return 0 if success, non-zero if failed.
 */
int16_t ut_appendenv(
    const char *varname, 
    const char *value, 
    ...);

/** Get environment variable.
 * 
 * @param varname Name of environment variable
 * @return value if environment variable, NULL if not set.
 */
UT_API 
char* ut_getenv(
    const char *varname);

/** Replace string with references to environment variables with their values.
 * A reference to an environment variable is an identifier prefixed with a $. 
 *
 * @param str String that contains references to environment variables.
 * @param value Value to assign to environment variable.
 * @return 0 if success, non-zero if failed.
 */
UT_API 
char* ut_envparse(
    const char* str, 
    ...);

/** Same as envparse but with a va_list parameter */
UT_API 
char* ut_venvparse(
    const char* str, 
    va_list args);

#ifdef __cplusplus
}
#endif

#endif
