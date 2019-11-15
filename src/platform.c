#include <string.h>

#include <launchdarkly/api.h>

#include "network.h"
#include "misc.h"

#ifdef _WIN32
    #include <windows.h>
    #define _CRT_RAND_S
#else
    #include <time.h>
    #include <unistd.h>
#endif

int
LDi_strncasecmp(const char *const s1, const char *const s2, const size_t n)
{
    #ifdef _WIN32
        return _strnicmp(s1, s2, n);
    #else
        return strncasecmp(s1, s2, n);
    #endif
}

bool
LDi_random(unsigned int *const result)
{
    #ifdef _WIN32
        LD_ASSERT(result);
        return rand_s(result) == 0;
    #else
        static unsigned int state;
        static bool init = false;
        static ld_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

        LD_ASSERT(result);

        LD_ASSERT(LDi_mtxlock(&lock));

        if (!init) {
            state = time(NULL);
            init  = true;
        }

        *result = rand_r(&state);

        LD_ASSERT(LDi_mtxunlock(&lock));

        return true;
    #endif
}

bool
LDi_sleepMilliseconds(const unsigned long milliseconds)
{
    #ifdef _WIN32
        Sleep(milliseconds);
        return true;
    #else
        int status;

        if ((status = usleep(1000 * milliseconds)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg), "usleep failed with: %s",
                strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);

            return false;
        }

        return true;
    #endif
}

#ifndef _WIN32
    static bool
    getTime(unsigned long *const resultMilliseconds, clockid_t clockid)
    {
        int status;
        struct timespec spec;

        LD_ASSERT(resultMilliseconds);

        if ((status = clock_gettime(clockid, &spec)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "clock_gettime failed with: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);

            return false;
        }

        *resultMilliseconds = (spec.tv_sec * 1000) + (spec.tv_nsec / 1000000);

        return true;
    }
#endif

bool
LDi_getMonotonicMilliseconds(unsigned long *const resultMilliseconds)
{
    #ifdef _WIN32
        *resultMilliseconds = GetTickCount64();
        return true;
    #else
        return getTime(resultMilliseconds, CLOCK_MONOTONIC);
    #endif
}

bool
LDi_getUnixMilliseconds(unsigned long *const resultMilliseconds)
{
    #ifdef _WIN32
        *resultMilliseconds = (double)time(NULL) * 1000.0;
        return true;
    #else
        return getTime(resultMilliseconds, CLOCK_REALTIME);
    #endif
}

bool
LDi_createthread(ld_thread_t *const thread,
    THREAD_RETURN (*const routine)(void *), void *const argument)
{
    #ifdef _WIN32
        ld_thread_t attempt = CreateThread(NULL, 0,
            (LPTHREAD_START_ROUTINE)routine, argument, 0, NULL);

        if (attempt == NULL) {
            return false;
        } else {
            *thread = attempt;

            return true;
        }
    #else
        int status;

        if ((status = pthread_create(thread, NULL, routine, argument)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg), "pthread_create failed: %s",
                strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_jointhread(ld_thread_t thread)
{
    #ifdef _WIN32
        return WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0;
    #else
        int status;

        if ((status = pthread_join(thread, NULL)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg), "pthread_join failed: %s",
                strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_mtxinit(ld_mutex_t *const mutex)
{
    #ifdef _WIN32
        LD_ASSERT(mutex);

        InitializeCriticalSection(mutex);

        return true;
    #else
        int status;

        LD_ASSERT(mutex);

        if ((status = pthread_mutex_init(mutex, NULL)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_mutex_init failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_mtxdestroy(ld_mutex_t *const mutex)
{
    #ifdef _WIN32
        LD_ASSERT(mutex);

        DeleteCriticalSection(mutex);

        return true;
    #else
        int status;

        LD_ASSERT(mutex);

        if ((status = pthread_mutex_destroy(mutex)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_mutex_destroy failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_mtxlock(ld_mutex_t *const mutex)
{
    #ifdef _WIN32
        LD_ASSERT(mutex);

        EnterCriticalSection(mutex);

        return true;
    #else
        int status;

        LD_ASSERT(mutex);

        if ((status = pthread_mutex_lock(mutex)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_mutex_lock failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_mtxunlock(ld_mutex_t *const mutex)
{
    #ifdef _WIN32
        LD_ASSERT(mutex);

        LeaveCriticalSection(mutex);

        return true;
    #else
        int status;

        LD_ASSERT(mutex);

        if ((status = pthread_mutex_unlock(mutex)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_mutex_unlock failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_rwlockinit(ld_rwlock_t *const lock)
{
    #ifdef _WIN32
        LD_ASSERT(lock);

        InitializeSRWLock(lock);

        return true;
    #else
        int status;

        LD_ASSERT(lock);

        if ((status = pthread_rwlock_init(lock, NULL)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_rwlock_init failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_rwlockdestroy(ld_rwlock_t *const lock)
{
    #ifdef _WIN32
        LD_ASSERT(lock);

        return true;
    #else
        int status;

        LD_ASSERT(lock);

        if ((status = pthread_rwlock_destroy(lock)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_rwlock_destroy failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_rdlock(ld_rwlock_t *const lock)
{
    #ifdef _WIN32
        LD_ASSERT(lock);

        AcquireSRWLockShared(lock);

        return true;
    #else
        int status;

        LD_ASSERT(lock);

        if ((status = pthread_rwlock_rdlock(lock)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_rwlock_rdlock failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_wrlock(ld_rwlock_t *const lock)
{
    #ifdef _WIN32
        LD_ASSERT(lock);

        AcquireSRWLockExclusive(lock);

        return true;
    #else
        int status;

        LD_ASSERT(lock);

        if ((status = pthread_rwlock_wrlock(lock)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_rwlock_wrlock failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_rdunlock(ld_rwlock_t *const lock)
{
    #ifdef _WIN32
        LD_ASSERT(lock);

        ReleaseSRWLockShared(lock);

        return true;
    #else
        int status;

        LD_ASSERT(lock);

        if ((status = pthread_rwlock_unlock(lock)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_rwlock_unlock failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

bool
LDi_wrunlock(ld_rwlock_t *const lock)
{
    #ifdef _WIN32
        LD_ASSERT(lock);

        ReleaseSRWLockExclusive(lock);

        return true;
    #else
        int status;

        LD_ASSERT(lock);

        if ((status = pthread_rwlock_unlock(lock)) != 0) {
            char msg[256];

            LD_ASSERT(snprintf(msg, sizeof(msg),
                "pthread_rwlock_unlock failed: %s", strerror(status)) >= 0);

            LD_LOG(LD_LOG_CRITICAL, msg);
        }

        return status == 0;
    #endif
}

#ifdef _WIN32

void
LDi_condwait(CONDITION_VARIABLE *cond, CRITICAL_SECTION *mtx, int ms)
{
    SleepConditionVariableCS(cond, mtx, ms);

}

void
LDi_condsignal(CONDITION_VARIABLE *cond)
{
    WakeAllConditionVariable(cond);
}

#else

void
LDi_condwait(pthread_cond_t *cond, pthread_mutex_t *mtx, int ms)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += ms / 1000;
    ts.tv_nsec += (ms % 1000) * 1000 * 1000;
    if (ts.tv_nsec > 1000 * 1000 * 1000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000 * 1000 * 1000;
    }

    pthread_cond_timedwait(cond, mtx, &ts);
}

void
LDi_condsignal(pthread_cond_t *cond)
{
    pthread_cond_broadcast(cond);
}

#endif
