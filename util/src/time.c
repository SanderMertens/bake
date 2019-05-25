/* Copyright (c) 2010-2019 the corto developers
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

#include <bake_util.h>

void ut_sleep(unsigned int sec, unsigned int nanosec) {
#ifndef _WIN32
    struct timespec sleepTime;

    sleepTime.tv_sec = sec;
    sleepTime.tv_nsec = nanosec;
    if (nanosleep(&sleepTime, NULL)) {
        ut_error("nanosleep failed: %s", strerror(errno));
    }
#else
	Sleep(sec + nanosec/1000000);
#endif
}

void timespec_gettime(struct timespec* time) {
#ifdef __MACH__ // OS X does not have clock_gettime, use clock_get_time
    clock_serv_t cclock;
    mach_timespec_t mts;
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);
    time->tv_sec = mts.tv_sec;
    time->tv_nsec = mts.tv_nsec;
#elif _WIN32
    timespec_get(time, TIME_UTC);
#else
    clock_gettime(CLOCK_REALTIME, time);
#endif
}

struct timespec timespec_add(struct timespec t1, struct timespec t2) {
    struct timespec result;

    result.tv_nsec = t1.tv_nsec + t2.tv_nsec;
    result.tv_sec = t1.tv_sec + t2.tv_sec;
    if (result.tv_nsec > 1000000000) {
        result.tv_sec++;
        result.tv_nsec = result.tv_nsec - 1000000000;
    }

    return result;
}

struct timespec timespec_sub(struct timespec t1, struct timespec t2) {
    struct timespec result;

    if (t1.tv_nsec >= t2.tv_nsec) {
        result.tv_nsec = t1.tv_nsec - t2.tv_nsec;
        result.tv_sec = t1.tv_sec - t2.tv_sec;
    } else {
        result.tv_nsec = t1.tv_nsec - t2.tv_nsec + 1000000000;
        result.tv_sec = t1.tv_sec - t2.tv_sec - 1;
    }

    return result;
}

double timespec_toDouble(struct timespec t) {
    double result;

    result = t.tv_sec;
    result += (double)t.tv_nsec / (double)1000000000;

    return result;
}

int timespec_compare(struct timespec t1, struct timespec t2) {
    int result;

    if (t1.tv_sec < t2.tv_sec) {
        result = -1;
    } else if (t1.tv_sec > t2.tv_sec) {
        result = 1;
    } else if (t1.tv_nsec < t2.tv_nsec) {
        result = -1;
    } else if (t1.tv_nsec > t2.tv_nsec) {
        result = 1;
    } else {
        result = 0;
    }

    return result;
}

char *timespec_to_str(
    struct timespec t)
{
    char str[256];
    time_t now = t.tv_sec;
    struct tm *_tm = localtime(&now);
    strftime(str, sizeof(str) - 1, "%d %m %Y %H:%M", _tm);
    return ut_strdup(str);
}

double timespec_measure(
    struct timespec *start)
{
    struct timespec stop, temp;
    timespec_gettime(&stop);
    temp = stop;
    stop = timespec_sub(stop, *start);
    *start = temp;
    return timespec_toDouble(stop);
}
