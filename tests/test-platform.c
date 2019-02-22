#include "ldinternal.h"

static THREAD_RETURN
threadDoNothing(void *const empty)
{
    LD_ASSERT(!empty);

    return THREAD_RETURN_DEFAULT;
}

static void
testMonotonic()
{
    unsigned int past;
    unsigned int present;

    LD_ASSERT(getMonotonicMilliseconds(&past));
    LD_ASSERT(getMonotonicMilliseconds(&present));

    LD_ASSERT(present >= past);
}

static void
testSleepMinimum()
{
    unsigned int past;
    unsigned int present;

    LD_ASSERT(getMonotonicMilliseconds(&past));

    LD_ASSERT(sleepMilliseconds(250));

    LD_ASSERT(getMonotonicMilliseconds(&present));

    LD_ASSERT(present - past >= 250);
}

static void
testThreadStartJoin()
{
    ld_thread_t thread;

    LD_ASSERT(LDi_createthread(&thread, threadDoNothing, NULL));
    LD_ASSERT(LDi_jointhread(thread));
}

static void
testRWLock()
{
    ld_rwlock_t lock;

    LD_ASSERT(LDi_rwlockinit(&lock));

    LD_ASSERT(LDi_rdlock(&lock));
    LD_ASSERT(LDi_rdunlock(&lock));

    LD_ASSERT(LDi_wrlock(&lock));
    LD_ASSERT(LDi_wrunlock(&lock))

    LD_ASSERT(LDi_rwlockdestroy(&lock));
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    testMonotonic();
    testSleepMinimum();
    testThreadStartJoin();
    testRWLock();

    return 0;
}
