/*
 * light weight pthread compatible library for Windows
 * (C) 2009 Okamura Yasunobu
 *
 *    WARNING This library does NOT support all future of pthread
 *
 */

#ifndef UT_WIN_THREAD_H
#define UT_WIN_THREAD_H

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>
#include <process.h>
#include <errno.h>

typedef struct pthread_tag {
    HANDLE handle;
} pthread_t;

/* stub */
typedef struct pthread_attr_tag {
    int attr;
} pthread_attr_t;

typedef struct pthread_mutexattr_tag {
    int attr;
} pthread_mutexattr_t;

typedef DWORD pthread_key_t;

typedef pthread_key_t ut_tls;

typedef struct ut_mutex_s {
    CRITICAL_SECTION mutex;
} ut_mutex_s;

typedef struct ut_rwmutex_s {
    SRWLOCK mutex;
} ut_rwmutex_s;

typedef struct ut_cond_s {
    CONDITION_VARIABLE cond;
} ut_cond_s;

typedef struct ut_sem_s {
    ut_mutex_s mutex;
    ut_cond_s cond;
    int value;
} ut_sem_s;

#ifdef __cplusplus
}
#endif

#endif // !_WIN32

#endif // !UT_WIN_THREAD_H

