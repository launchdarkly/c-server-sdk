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
        return pthread_create(thread, NULL, routine, argument) == 0;
    #endif
}

bool
LDi_jointhread(ld_thread_t thread)
{
    #ifdef _WIN32
        return WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0;
    #else
        return pthread_join(thread, NULL) == 0;
    #endif
}

bool
LDi_rwlockinit(ld_rwlock_t *const lock)
{
    LD_ASSERT(lock);

    #ifdef _WIN32
        InitializeSRWLock(lock);

        return true;
    #else
        return pthread_rwlock_init(lock, NULL) == 0;
    #endif
}

bool
LDi_rwlockdestroy(ld_rwlock_t *const lock)
{
    LD_ASSERT(lock);

    #ifdef _WIN32
        return true;
    #else
        return pthread_rwlock_destroy(lock) == 0;
    #endif
}

bool
LDi_rdlock(ld_rwlock_t *const lock)
{
    LD_ASSERT(lock);

    #ifdef _WIN32
        AcquireSRWLockShared(lock);

        return true;
    #else
        return pthread_rwlock_rdlock(lock) == 0;
    #endif
}

bool
LDi_wrlock(ld_rwlock_t *const lock)
{
    LD_ASSERT(lock);

    #ifdef _WIN32
        AcquireSRWLockExclusive(lock);

        return true;
    #else
        return pthread_rwlock_wrlock(lock) == 0;
    #endif
}

bool
LDi_rdunlock(ld_rwlock_t *const lock)
{
    LD_ASSERT(lock);

    #ifdef _WIN32
        ReleaseSRWLockShared(lock);

        return true;
    #else
        return pthread_rwlock_unlock(lock) == 0;
    #endif
}

bool
LDi_wrunlock(ld_rwlock_t *const lock)
{
    LD_ASSERT(lock);

    #ifdef _WIN32
        ReleaseSRWLockExclusive(lock);

        return true;
    #else
        return pthread_rwlock_unlock(lock) == 0;
    #endif
}
