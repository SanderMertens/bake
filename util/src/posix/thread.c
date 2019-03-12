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
    pthread_t thread;
    int r;

    if ((r = pthread_create (&thread, NULL, f, arg))) {
        ut_critical("pthread_create failed: %d", r);
    }

    return (ut_thread)thread;
}

int ut_thread_join(
    ut_thread thread,
    void** result)
{
    return pthread_join((pthread_t)thread, result);
}

int ut_thread_detach(
    ut_thread thread)
{
    pthread_detach((pthread_t)thread);
    return 0;
}

int ut_thread_setPriority(
    ut_thread thread,
    int priority)
{
    pthread_t tid;
    struct sched_param param;

    tid = (pthread_t)thread;

    param.sched_priority = priority;

    return pthread_setschedparam(tid, SCHED_OTHER, &param);
}

int ut_thread_getPriority(
    ut_thread thread)
{
    pthread_t tid;
    int policy;
    struct sched_param param;

    tid = (pthread_t)thread;

    if (pthread_getschedparam(tid, &policy, &param)) {
        return -1;
    }

    return param.sched_priority;
}

ut_thread ut_thread_self() {
    return (ut_thread)pthread_self();
}

/* Keep track of registered thread keys since pthread does not call destructors
 * for the main thread */
typedef struct ut_threadTlsAdmin_t {
    ut_tls key;
    void (*destructor)(void*);
} ut_threadTlsAdmin_t;

static ut_threadTlsAdmin_t ut_threadTlsAdmin[UT_MAX_THREAD_KEY];
static int32_t ut_threadTlsCount = -1;

int ut_tls_new(
    ut_tls* key,
    void(*destructor)(void*))
{
    do {
        if (pthread_key_create(key, destructor)) {
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
    return pthread_setspecific(key, value);
}

void* _ut_tls_get(
    ut_tls key,
    const char *key_str)
{
    ut_assert(
        key != 0,
        "invalid (uninitialized?) key ('%s') specified for 'ut_tls_set'", key_str);
    return pthread_getspecific(key);
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
    int result;
    if ((result = pthread_mutex_init (&m->mutex, NULL))) {
#ifdef NDEBUG
        ut_throw("mutex_new failed: %s", strerror(result));
#else
        ut_critical("mutex_new failed: %s", strerror(result));
#endif
    }
    return result;
}

int ut_mutex_lock(
    ut_mutex mutex)
{
    int result;
    if ((result = pthread_mutex_lock(&mutex->mutex))) {
#ifdef NDEBUG
        ut_throw("mutex_lock failed: %s", strerror(result));
#else
        ut_critical("mutex_lock failed: %s", strerror(result));
#endif
    }
    return result;
}

int ut_mutex_unlock(ut_mutex mutex) {
    return pthread_mutex_unlock(&mutex->mutex);
}

int ut_mutex_free(
    ut_mutex mutex)
{
    int result;
    if ((result = pthread_mutex_destroy(&mutex->mutex))) {
#ifdef NDEBUG
        ut_throw("mutex_free failed: %s", strerror(result));
#else
        ut_critical("mutex_free failed: %s", strerror(result));
#endif
    }
    return result;
}

int ut_mutex_try(
    ut_mutex mutex)
{
    int result;
    if ((result = pthread_mutex_trylock(&mutex->mutex))) {
#ifdef NDEBUG
        ut_throw("mutex_try failed: %s", strerror(result));
#else
        ut_critical("mutex_try failed: %s", strerror(result));
#endif
    }
    return result;
}

// OS X does not have pthread_mutex_timedlock
#ifdef __MACH__
static int pthread_mutex_timedlock (
    pthread_mutex_t *mutex,
    struct timespec *timeout)
{
   struct timespec timenow;
   struct timespec sleeptime;
   int retcode;

   sleeptime.tv_sec = 0;
   sleeptime.tv_nsec = 10000000; /* 10ms */

   while ((retcode = pthread_mutex_trylock (mutex)) == EBUSY) {
      timespec_gettime (&timenow);

      if (timenow.tv_sec >= timeout->tv_sec &&
         (timenow.tv_nsec * 1000) >= timeout->tv_nsec)
      {
          return ETIMEDOUT;
      }

      nanosleep (&sleeptime, NULL);
   }

   return retcode;
}
#endif

int ut_mutex_lockTimed(
    ut_mutex mutex,
    struct timespec ts)
{
    int result;

    if ((result = pthread_mutex_timedlock(&mutex->mutex, &ts))) {
        if (result != ETIMEDOUT) {
            ut_throw("mutex_try failed: %s", strerror(result));
        }
    }

    return result;
}

/* Create read-write mutex */
int ut_rwmutex_new(
    struct ut_rwmutex_s *m)
{
    int result = 0;

    if ((result = pthread_rwlock_init(&m->mutex, NULL))) {
        ut_throw("rwmutex_new failed: %s", strerror(result));
    }
    return result;
}

/* Free read-write mutex */
int ut_rwmutex_free(
    ut_rwmutex m)
{
    int result = 0;
    if ((result = pthread_rwlock_destroy(&m->mutex))) {
        ut_throw("rwmutex_free failed: %s", strerror(result));
    }
    return result;
}

/* Read lock */
int ut_rwmutex_read(
    ut_rwmutex mutex)
{
    int result = 0;
    if ((result = pthread_rwlock_rdlock(&mutex->mutex))) {
        ut_throw("rwmutex_read failed: %s", strerror(result));
    }
    return result;
}

/* Write lock */
int ut_rwmutex_write(
    ut_rwmutex mutex)
{
    int result = 0;
    if ((result = pthread_rwlock_wrlock(&mutex->mutex))) {
        ut_throw("rwmutex_write failed: %s", strerror(result));
    }
    return result;
}

/* Try read */
int ut_rwmutex_tryRead(
    ut_rwmutex mutex)
{
    int result;
    if ((result = pthread_rwlock_tryrdlock(&mutex->mutex))) {
        ut_throw("rwmutex_tryRead failed: %s", strerror(result));
    }
    return result;
}

/* Try write */
int ut_rwmutex_tryWrite(
    ut_rwmutex mutex)
{
    int result;
    if ((result = pthread_rwlock_trywrlock(&mutex->mutex))) {
        ut_throw("rwmutex_tryWrite failed: %s", strerror(result));
    }
    return result;
}

/* Write unlock */
int ut_rwmutex_unlock(
    ut_rwmutex mutex)
{
    int result;
    if ((result = pthread_rwlock_unlock(&mutex->mutex))) {
        ut_throw("rwmutex_unlock failed: %s", strerror(result));
    }
    return result;
}

/* Create condition variable */
int ut_cond_new(
    struct ut_cond_s *cond)
{
    int result = 0;
    if ((result = pthread_cond_init(&cond->cond, NULL))) {
        ut_throw("cond_new failed: %s", strerror(result));
    }
    return result;
}

/* Signal condition variable */
int ut_cond_signal(
    ut_cond cond)
{
    int result = 0;
    if ((result = pthread_cond_signal(&cond->cond))) {
        ut_throw("cond_signal failed: %s", strerror(result));
    }
    return result;
}

/* Wait for condition variable */
int ut_cond_wait(
    ut_cond cond,
    ut_mutex mutex)
{
    int result = 0;
    if ((result = pthread_cond_wait(&cond->cond, &mutex->mutex))) {
        ut_throw("cond_wait failed: %s", strerror(result));
    }
    return result;
}

/* Free condition variable */
int ut_cond_free(
    ut_cond cond)
{
    int result = 0;
    if ((result = pthread_cond_destroy(&cond->cond))) {
        ut_throw("cond_free failed: %s", strerror(result));
    }
    return result;
}

/* Create new semaphore */
ut_sem ut_sem_new(
    unsigned int initValue)
{
    ut_sem semaphore;

    semaphore = malloc (sizeof(ut_sem_s));
    memset(semaphore, 0, sizeof(ut_sem_s));
    if (semaphore) {
        pthread_mutex_init(&semaphore->mutex, NULL);
        pthread_cond_init(&semaphore->cond, NULL);
        semaphore->value = initValue;
    }

    return (ut_sem)semaphore;
}

/* Post to semaphore */
int ut_sem_post(
    ut_sem semaphore)
{
    pthread_mutex_lock(&semaphore->mutex);
    semaphore->value++;
    if(semaphore->value > 0) {
        pthread_cond_signal(&semaphore->cond);
    }
    pthread_mutex_unlock(&semaphore->mutex);
    return 0;
}

/* Wait for semaphore */
int ut_sem_wait(
    ut_sem semaphore)
{
    pthread_mutex_lock(&semaphore->mutex);
    while(semaphore->value <= 0) {
        pthread_cond_wait(&semaphore->cond, &semaphore->mutex);
    }
    semaphore->value--;
    pthread_mutex_unlock(&semaphore->mutex);
    return 0;
}

/* Trywait for semaphore */
int ut_sem_tryWait(
    ut_sem semaphore)
{
    int result;

    pthread_mutex_lock(&semaphore->mutex);
    if(semaphore->value > 0) {
        semaphore->value--;
        result = 0;
    } else {
        errno = EAGAIN;
        result = -1;
    }
    pthread_mutex_unlock(&semaphore->mutex);
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
    pthread_cond_destroy(&semaphore->cond);
    pthread_mutex_destroy(&semaphore->mutex);
    free(semaphore);
    return 0;
}

int ut_ainc(int* count) {
    int value;
#ifdef __GNUC__
    value = __sync_add_and_fetch (count, 1);
    return value;
#else
    AtomicModify( count, &value, 0, 1 );
    return( value + 1 );
#endif
}

int ut_adec(int* count) {
    int value;
#ifdef __GNUC__
    value = __sync_sub_and_fetch (count, 1);
    return value;
#else
    AtomicModify( count, &value, 0, -1 );
    return( value - 1 );
#endif
}
