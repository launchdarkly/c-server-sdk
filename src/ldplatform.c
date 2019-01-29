#include "string.h"

#include "ldinternal.h"

bool
LDi_createthread(ld_thread_t *const thread, THREAD_RETURN (*const routine)(void *), void *const argument)
{
    #ifdef _WIN32
        ld_thread_t attempt = CreateThread(NULL, 0, routine, argument, 0, NULL);

        if (attempt == NULL) {
            return false;
        } else {
            *thread = attempt;

            return true;
        }
    #else
        int status;

        if ((status = pthread_create(thread, NULL, routine, argument)) != 0) {
            LDi_log(LD_LOG_CRITICAL, "pthread_create failed with: %s", strerror(status));
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
            LDi_log(LD_LOG_CRITICAL, "pthread_join failed with: %s", strerror(status));
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
            LDi_log(LD_LOG_CRITICAL, "pthread_rwlock_init failed with: %s", strerror(status));
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
            LDi_log(LD_LOG_CRITICAL, "pthread_rwlock_destroy failed with: %s", strerror(status));
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
            LDi_log(LD_LOG_CRITICAL, "pthread_rwlock_rdlock failed with: %s", strerror(status));
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
            LDi_log(LD_LOG_CRITICAL, "pthread_rwlock_wrlock failed with: %s", strerror(status));
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
            LDi_log(LD_LOG_CRITICAL, "pthread_rwlock_unlock failed with: %s", strerror(status));
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
            LDi_log(LD_LOG_CRITICAL, "pthread_rwlock_unlock failed with: %s", strerror(status));
        }

        return status == 0;
    #endif
}
