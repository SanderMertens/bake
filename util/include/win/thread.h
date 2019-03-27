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

typedef struct pthread_mutex_tag {
    HANDLE handle;
} pthread_mutex_t;

/* stub */
typedef struct pthread_attr_tag {
    int attr;
} pthread_attr_t;

typedef struct pthread_mutexattr_tag {
    int attr;
} pthread_mutexattr_t;

typedef DWORD pthread_key_t;
typedef PSRWLOCK pthread_rwlock_t;
typedef PCONDITION_VARIABLE pthread_cond_t;

typedef pthread_key_t ut_tls;

typedef struct ut_mutex_s {
    pthread_mutex_t mutex;
} ut_mutex_s;

typedef struct ut_rwmutex_s {
    pthread_rwlock_t mutex;
} ut_rwmutex_s;

typedef struct ut_cond_s {
    pthread_cond_t cond;
} ut_cond_s;

typedef struct ut_sem_s {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int value;
} ut_sem_s;

#ifdef __cplusplus
}
#endif

#endif // !_WIN32

#endif // !UT_WIN_THREAD_H

