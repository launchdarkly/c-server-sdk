#pragma once

#include <stdbool.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <pthread.h>
#endif

#ifdef _WIN32
    #define THREAD_RETURN DWORD
    #define THREAD_RETURN_DEFAULT 0
    #define ld_thread_t HANDLE

    #define ld_mutex_t CRITICAL_SECTION
    #define ld_cond_t CONDITION_VARIABLE

    #ifdef LAUNCHDARKLY_MUTEX_ONLY
        #define ld_rwlock_t ld_mutex_t
    #else
        #define ld_rwlock_t SRWLOCK
    #endif
#else
    #define THREAD_RETURN void *
    #define THREAD_RETURN_DEFAULT NULL
    #define ld_thread_t pthread_t

    #define ld_mutex_t pthread_mutex_t
    #define ld_cond_t pthread_cond_t

    #ifdef LAUNCHDARKLY_MUTEX_ONLY
        #define ld_rwlock_t ld_mutex_t
    #else
        #define ld_rwlock_t pthread_rwlock_t
    #endif
#endif

bool LDi_thread_join(ld_thread_t *const thread);
bool LDi_thread_create(ld_thread_t *const thread,
    THREAD_RETURN (*const routine)(void *), void *const argument);

bool LDi_mutex_init(ld_mutex_t *const mutex);
bool LDi_mutex_destroy(ld_mutex_t *const mutex);
bool LDi_mutex_lock(ld_mutex_t *const mutex);
bool LDi_mutex_unlock(ld_mutex_t *const mutex);

bool LDi_rwlock_init(ld_rwlock_t *const lock);
bool LDi_rwlock_destroy(ld_rwlock_t *const lock);
bool LDi_rwlock_rdlock(ld_rwlock_t *const lock);
bool LDi_rwlock_wrlock(ld_rwlock_t *const lock);
bool LDi_rwlock_rdunlock(ld_rwlock_t *const lock);
bool LDi_rwlock_wrunlock(ld_rwlock_t *const lock);

bool LDi_cond_init(ld_cond_t *const cond);
bool LDi_cond_wait(ld_cond_t *const cond, ld_mutex_t *const mutex,
    const int milliseconds);
bool LDi_cond_signal(ld_cond_t *const cond);
bool LDi_cond_destroy(ld_cond_t *const cond);
