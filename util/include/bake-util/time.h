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
 * @section Time functions.
 * @brief Functions for doing basic operations/calculations on timespec values.
 */

#ifndef UT_TIME_H
#define UT_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

/** Sleep for specified amount of time.
 *
 * @param sec Seconds to sleep
 * @param nanoseconds to sleep
 */
UT_API
void ut_sleep(
    unsigned int sec,
    unsigned int nanosec);

/** Get current time.
 *
 * @param time Variable that will contain the time.
 */
UT_API
void timespec_gettime(
    struct timespec* time);

/** Add two time values.
 *
 * @param t1 Time value.
 * @param t2 Time value.
 * @result Result of adding t1 and t2
 */
UT_API
struct timespec timespec_add(
    struct timespec t1,
    struct timespec t2);

/** Subtract two time values.
 *
 * @param t1 Time value to subtract from.
 * @param t2 Time value to subtract.
 * @result Result of subtracting t1 and t2
 */
UT_API
struct timespec timespec_sub(
    struct timespec t1,
    struct timespec t2);

/** Compare two time values.
 *
 * @param t1 Time value.
 * @param t2 Time value.
 * @result 0 if equal, -1 if t2 is less than t1, 1 if t1 is larger than t2
 */
UT_API
int timespec_compare(
    struct timespec t1,
    struct timespec t2);

/** Measure time passed since last measurement.
 *
 * @param t1 Last measurement
 * @result Time elapsed since last measurement
 */
UT_API
double timespec_measure(
    struct timespec *t1);

/** Convert time to double-precision floating point value.
 *
 * @param t Time value.
 * @result Floating point representation of the time.
 */
UT_API
double timespec_toDouble(
    struct timespec t);

UT_API
char* timespec_to_str(
    struct timespec t);

#ifdef __cplusplus
}
#endif

#endif
