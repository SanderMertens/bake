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

#ifndef UT_THREAD_H
#define UT_THREAD_H


#ifdef __cplusplus
extern "C" {
#endif

#define UT_LOCK_BUSY (1)

typedef struct ut_mutex_s* ut_mutex;
typedef struct ut_rwmutex_s* ut_rwmutex;
typedef struct ut_cond_s* ut_cond;
typedef struct ut_sem_s* ut_sem;


/* -- Thread management -- */
#ifdef _WIN32
typedef HANDLE ut_thread;
#else
typedef unsigned long int ut_thread;
#endif
typedef void* (*ut_thread_cb)(void*);

/** Create new thread.
 *
 * @param func Pointer to function to run in thread.
 * @param arg Argument to pass to thread function.
 * @return Handle to thread, 0 if failed.
 */
UT_API
ut_thread ut_thread_new(
    ut_thread_cb func,
    void* arg);

/** Join a thread.
 * Blocks until the specified thread exits.
 *
 * @param thread Handle to thread to join.
 * @param arg Pointer that will be set to return value of thread.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_thread_join(
    ut_thread thread,
    void **arg);

/** Detach a thread.
 * Enables automatic cleanup of resources once a thread exits and removes the
 * need to call ut_thread_join.
 *
 * @param thread Handle to thread to detach.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_thread_detach(
    ut_thread thread);

/** Set priority of a thread.
 *
 * @param thread Handle to thread to detach.
 * @param priority Priority to set.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_thread_setPriority(
    ut_thread thread,
    int priority);

/** Get priority of a thread.
 *
 * @param thread Handle to thread to detach.
 * @return Priority of thread.
 */
UT_API
int ut_thread_getPriority(
    ut_thread thread);

/** Return handle of current thread.
 *
 * @return handle to thread.
 */
UT_API
ut_thread ut_thread_self(void);



/* -- Thread local storage -- */

/** Generate a new tls key.
 *
 * @param key_out Pointer to variable that will hold new tls key.
 * @param destructor Function that will be invoked when tls data is cleaned up.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_tls_new(
    ut_tls* key_out,
    void(*destructor)(void*));

/** Set thread specific data for tls key.
 *
 * @param key Key for which to set the tls data.
 * @param value Value to assign to tls storage.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int _ut_tls_set(
    ut_tls key,
    const char *key_str,
    void* value);

#define ut_tls_set(key, value) _ut_tls_set(key, #key, value)

/** Get thread specific data for tls key.
 *
 * @param key Key for which to get the tls data.
 * @return tls storage corresponding to key. NULL if not set.
 */
UT_API
void* _ut_tls_get(
    ut_tls key,
    const char* key_str);

#define ut_tls_get(key) _ut_tls_get(key, #key)

/** Free tls data in main thread.
 * Sometimes thread libraries do not cleanup tls data automatically in the
 * mainthread. To ensure that there are no memory leaks, call this function
 * before exitting the application to ensure memory logs are clean.
 *
 * The ut_deinit function calls this function.
 */
UT_API
void ut_tls_free(void);



/* -- Mutex operations -- */


/** Create new mutex.
 *
 * @param mutex Pointer to uninitialized ut_mutex_s structure.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_mutex_new(
    struct ut_mutex_s *mutex);

/** Lock mutex
 *
 * @param mutex Mutex to lock.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_mutex_lock(
    ut_mutex mutex);

/** Unlock mutex
 *
 * @param mutex Mutex to unlock.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_mutex_unlock(
    ut_mutex mutex);

/** Free mutex resources.
 *
 * @param mutex Mutex to free.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_mutex_free(
    ut_mutex mutex);

/** Try to lock mutex.
 * If another thread already holds the mutex, the function will fail.
 *
 * @param mutex Mutex to free.
 * @return 0 if mutex is free, UT_LOCK_BUSY if occupied, other value if failed.
 */
UT_API
int ut_mutex_try(
    ut_mutex mutex);

/** Try to lock mutex for specified amount of time.
 * If a lock cannot be obtained within the specified timeout period, acquiring
 * the mutex will fail.
 *
 * @param mutex Mutex to lock.
 * @param timeout The maximum amount of time to try to lock the mutex.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_mutex_lockTimed(
    ut_mutex mutex,
    struct timespec timeout);



/* -- Read-write mutex -- */


/** Create new read-write mutex.
 *
 * @param mutex Pointer to uninitialized ut_rwmutex_s structure.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_rwmutex_new(
    struct ut_rwmutex_s *mutex);

/** Read read-write mutex.
 * Multiple threads may simultaneously read a resource protected by a read-write
 * mutex.
 *
 * @param mutex Mutex to read.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_rwmutex_read(
    ut_rwmutex mutex);

/** Write read-write mutex.
 * Only one thread may simultaneously write a resource protected by a read-write
 * mutex.
 *
 * @param mutex Mutex to read.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_rwmutex_write(
    ut_rwmutex mutex);

/** Try reading read-write mutex.
 * If another thread is already writing the mutex, this function will fail.
 *
 * @param mutex Mutex to read.
 * @return 0 if success, UT_LOCK_BUSY if busy, other value if failed.
 */
UT_API
int ut_rwmutex_tryRead(
    ut_rwmutex mutex);

/** Try writing read-write mutex.
 * If another thread is already reading or writing the mutex, this function will
 * fail.
 *
 * @param mutex Mutex to write.
 * @return 0 if success, UT_LOCK_BUSY if busy, other value if failed.
 */
UT_API
int ut_rwmutex_tryWrite(
    ut_rwmutex mutex);

/** Unlock read-write mutex.
 * Needs to be called after successfully obtaining a read or write lock.
 *
 * @param mutex Mutex to unlock.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_rwmutex_unlock(
    ut_rwmutex mutex);

/** Free read-write mutex resources.
 *
 * @param mutex Mutex to free.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_rwmutex_free(
    ut_rwmutex mutex);



/* -- Condition variables -- */


/** Create new condition variable.
 *
 * @param cond Pointer to uninitialized ut_cond_s structure.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_cond_new(
    struct ut_cond_s *cond);

/** Signal condition variable.
 *
 * @param cond Pointer to initialized condition variable.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_cond_signal(
    ut_cond cond);

/** Broadcast condition variable.
 *
 * @param cond Pointer to initialized condition variable.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_cond_broadcast(
    ut_cond cond);

/** Wait for condition variable.
 *
 * @param cond Pointer to initialized condition variable.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_cond_wait(
    ut_cond cond,
    ut_mutex mutex);

/** Create new read-write mutex.
 *
 * @param mutex Pointer to initialized ut_cond_s structure.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_cond_free(
    ut_cond cond);

/* -- Semaphores -- */


/** Create new semaphore.
 *
 * @param initValue Initial value of semaphore.
 * @return Handle to new semaphore. NULL if failed.
 */
UT_API
ut_sem ut_sem_new(
    unsigned int initValue);

/** Increase value of semaphore.
 *
 * @param sem Handle to semaphore.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_sem_post(
    ut_sem sem);

/** Decrease value of semaphore, block if zero.
 *
 * @param sem Handle to semaphore.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_sem_wait(
    ut_sem sem);

/** Try decreasing value of semaphore, do not block if zero.
 *
 * @param sem Handle to semaphore.
 * @return 0 if success, UT_LOCK_BUSY if empty, non-zero if failed.
 */
UT_API
int ut_sem_tryWait(
    ut_sem sem);

/** Get current value of semaphore.
 *
 * @param sem Handle to semaphore.
 * @return Value of current semaphore.
 */
UT_API
int ut_sem_value(
    ut_sem sem);

/** Free semaphore resources.
 *
 * @param sem Handle to semaphore.
 * @return 0 if success, non-zero if failed.
 */
UT_API
int ut_sem_free(
    ut_sem sem);



/* -- Atomic increments and decrements -- */

/** Atomically increment integer value.
 * Even if multiple threads simultaneously are calling ut_ainc, it is
 * guaranteed that count will be increased by the number of times ut_ainc
 * is invoked.
 *
 * @param count Value to increment.
 * @return Value after incrementing.
 */
UT_API
int ut_ainc(
    int* count);

/** Atomically decrement integer value.
 * Even if multiple threads simultaneously are calling ut_adec, it is
 * guaranteed that count will be increased by the number of times ut_ainc
 * is invoked.
 *
 * @param count Value to decrement.
 * @return Value after decrementing.
 */
UT_API
int ut_adec(
    int* count);

/** Atomic compare and swap.
 * Only set ptr to new value if the value of ptr equals old value.
 */
#define ut_cas(ptr, old, new) __sync_bool_compare_and_swap(ptr, old, new)

#ifdef __cplusplus
}
#endif

#endif
