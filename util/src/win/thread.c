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

#include "../../include/util.h"

ut_thread ut_thread_new(
    ut_thread_cb f,
    void* arg)
{
    HANDLE thread_handle;
    DWORD thread_id;
    thread_handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)f, arg, 0, &thread_id);

    if (thread_handle == NULL) {
        ut_critical("thread creation failed: %s", ut_last_win_error());
    }
    return (ut_thread)thread_handle;
}

int ut_thread_join(
    ut_thread thread,
    void** result)
{
    DWORD retvalue = WaitForSingleObject(thread, INFINITE);
    if (retvalue == WAIT_OBJECT_0) {
        return 0;
    }
    else {
        return EINVAL;
    }
}

int ut_thread_detach(
    ut_thread thread)
{
    /* Do nothing */
    return 0;
}

int ut_thread_setPriority(
    ut_thread thread,
    int priority)
{
    if (!SetThreadPriority(thread, priority))
    {
        ut_error("thread set priority");
        return 1;
    }
    return 0;
}

int ut_thread_getPriority(
    ut_thread thread)
{
    return GetThreadPriority(thread);
}

ut_thread ut_thread_self() {
    return (ut_thread)GetCurrentThread();
}

/* Keep track of registered thread keys since pthread does not call destructors
 * for the main thread */
typedef struct ut_threadTlsAdmin_t {
    ut_tls key;
    void (*destructor)(void*);
} ut_threadTlsAdmin_t;

static ut_threadTlsAdmin_t ut_threadTlsAdmin[UT_MAX_THREAD_KEY];
static int32_t ut_threadTlsCount = -1;


int ut_create_tls_key(pthread_key_t *key, void(*destr_function) (void *))
{
    DWORD dkey = TlsAlloc();
    if (dkey != 0xFFFFFFFF) {
        *key = dkey;
        return 0;
    }
    else {
        return EAGAIN;
    }
}

int ut_tls_new(
    ut_tls* key,
    void(*destructor)(void*))
{
    do {
        if (ut_create_tls_key(key, destructor)) {
            ut_throw("ut_tls_new failed.");
            goto error;
        }
    } while (*key == 0);

    if (destructor) {
        int32_t slot = ut_ainc(&ut_threadTlsCount);
        ut_threadTlsAdmin[slot].key = *key;
        ut_threadTlsAdmin[slot].destructor = destructor;
    }

    return 0;
error:
    return -1;
}

int _ut_tls_set(
    ut_tls key,
    const char *key_str,
    void* value)
{
    ut_assert(
        key != 0,
        "invalid (uninitialized?) key ('%s') specified for 'ut_tls_set'", key_str);
    if (TlsSetValue(key, (LPVOID)value)) {
        return 0;
    }
    else {
        return EINVAL;
    }
}

void* _ut_tls_get(
    ut_tls key,
    const char *key_str)
{
    ut_assert(
        key != 0,
        "invalid (uninitialized?) key ('%s') specified for 'ut_tls_set'", key_str);
    return TlsGetValue(key);
}

void ut_tls_free(void) {
    int32_t i;

    for (i = 0; i <= ut_threadTlsCount; i++) {
        void *data = ut_tls_get(ut_threadTlsAdmin[i].key);
        ut_threadTlsAdmin[i].destructor(data);
        ut_tls_set(ut_threadTlsAdmin[i].key, NULL);
    }
}

int ut_mutex_new(
    struct ut_mutex_s *m)
{
    int result = 0;
    InitializeCriticalSection(&m->mutex);
    return result;
}

int ut_mutex_lock(
    ut_mutex mutex)
{
    EnterCriticalSection(&mutex->mutex);
    return 0;
}

int ut_mutex_unlock(
    ut_mutex mutex) 
{
    LeaveCriticalSection(&mutex->mutex);
    return 0;
}

int ut_mutex_free(
    ut_mutex mutex)
{
    DeleteCriticalSection(&mutex->mutex);
    return 0;
}

int ut_mutex_try(
    ut_mutex mutex)
{
    BOOL retvalue = TryEnterCriticalSection(&mutex->mutex);
    if (retvalue) {
        return 0;
    }
    else {
        return EBUSY;
    }
}

/* Create read-write mutex */
int ut_rwmutex_new(
    struct ut_rwmutex_s *m)
{
    InitializeSRWLock(&m->mutex);
    return 0;
}

/* Free read-write mutex */
int ut_rwmutex_free(
    ut_rwmutex m)
{
    (void)*(&m->mutex);
    return 0;
}

/* Read lock */
int ut_rwmutex_read(
    ut_rwmutex mutex)
{
    AcquireSRWLockShared(&mutex->mutex);
    return 0;
}

/* Write lock */
int ut_rwmutex_write(
    ut_rwmutex mutex)
{
    AcquireSRWLockExclusive(&mutex->mutex);
    return 0;
}

/* Create condition variable */
int ut_cond_new(
    struct ut_cond_s *cond)
{
    InitializeConditionVariable(&cond->cond);
    return 0;
}

/* Signal condition variable */
int ut_cond_signal(
    ut_cond cond)
{
    WakeConditionVariable(&cond->cond);
    return 0;
}

/* Broadcast condition variable */
int ut_cond_broadcast(
    ut_cond cond)
{
    WakeAllConditionVariable(&cond->cond);
    return 0;
}

/* Wait for condition variable */
int ut_cond_wait(
    ut_cond cond, ut_mutex mutex)
{
    int result = 0;

    if (!SleepConditionVariableCS(&cond->cond, (PCRITICAL_SECTION)&mutex->mutex, INFINITE)) {
        ut_throw("cond_wait failed");
    }
    return result;
}

/* Free condition variable */
int ut_cond_free(
    ut_cond cond)
{
    /* Do nothing */
    return 0;
}

#define MAX_SEM_COUNT 10
#define THREADCOUNT 12

/* Create new semaphore */
ut_sem ut_sem_new(
    unsigned int initValue)
{
    ut_sem semaphore;
    HANDLE ghSemaphore;

    semaphore = malloc (sizeof(ut_sem_s));
    memset(semaphore, 0, sizeof(ut_sem_s));
    if (semaphore) {
        ut_mutex_new(&semaphore->mutex);
        semaphore->value = initValue;
    }
    return (ut_sem)semaphore;
}

/* Post to semaphore */
int ut_sem_post(
    ut_sem semaphore)
{
    ut_mutex_lock(&semaphore->mutex);
    semaphore->value++;
    if(semaphore->value > 0) {
        ut_cond_signal(&semaphore->cond);
    }
    ut_mutex_unlock(&semaphore->mutex);
    return 0;
}

/* Wait for semaphore */
int ut_sem_wait(
    ut_sem semaphore)
{
    ut_mutex_lock(&semaphore->mutex);
    while(semaphore->value <= 0) {
        ut_cond_wait(&semaphore->cond, &semaphore->mutex);
    }
    semaphore->value--;
    ut_mutex_free(&semaphore->mutex);
    return 0;
}

/* Trywait for semaphore */
int ut_sem_tryWait(
    ut_sem semaphore)
{
    int result;
    ut_mutex_lock(&semaphore->mutex);
    if(semaphore->value > 0) {
        semaphore->value--;
        result = 0;
    } else {
        errno = EAGAIN;
        result = -1;
    }
    ut_mutex_unlock(&semaphore->mutex);
    return result;
}

/* Get value of semaphore */
int ut_sem_value(
    ut_sem semaphore)
{
    return semaphore->value;
}

/* Free semaphore */
int ut_sem_free(
    ut_sem semaphore)
{
    ut_mutex_free(&semaphore->mutex);
    free(semaphore);
    return 0;
}

int ut_ainc(int* count) {
    return InterlockedIncrement(count);
}

int ut_adec(int* count) {
    return InterlockedDecrement(count);
}
