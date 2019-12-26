#include <launchdarkly/api.h>

#include "misc.h"

static void
testMonotonic()
{
    unsigned long past, present;

    LD_ASSERT(LDi_getMonotonicMilliseconds(&past));
    LD_ASSERT(LDi_getMonotonicMilliseconds(&present));

    LD_ASSERT(present >= past);
}

static void
testGetUnixMilliseconds()
{
    unsigned long now;
    LD_ASSERT(LDi_getUnixMilliseconds(&now));
}

static void
testSleepMinimum()
{
    unsigned long past, present;

    LD_ASSERT(LDi_getMonotonicMilliseconds(&past));

    LD_ASSERT(LDi_sleepMilliseconds(50));

    LD_ASSERT(LDi_getMonotonicMilliseconds(&present));
    /* monotonic clock accurate to within 10 ms */
    LD_ASSERT(present - past >= 40);
}

static THREAD_RETURN
threadDoNothing(void *const empty)
{
    LD_ASSERT(!empty);

    return THREAD_RETURN_DEFAULT;
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

struct Context {
    ld_rwlock_t lock;
    bool flag;
};

static THREAD_RETURN
threadGoAwait(void *const rawcontext)
{
    struct Context *context = (struct Context *)rawcontext;

    while (true) {
        LD_ASSERT(LDi_wrlock(&context->lock));
        if (context->flag) {
            context->flag = false;
            LD_ASSERT(LDi_wrunlock(&context->lock));
            break;
        }
        LD_ASSERT(LDi_wrunlock(&context->lock));

        LD_ASSERT(LDi_sleepMilliseconds(1));
    }

    return THREAD_RETURN_DEFAULT;
}

static void
testConcurrency()
{
    ld_thread_t thread;

    struct Context context;
    context.flag = false;

    LD_ASSERT(LDi_rwlockinit(&context.lock));
    LD_ASSERT(LDi_createthread(&thread, threadGoAwait, &context));

    LD_ASSERT(LDi_sleepMilliseconds(25));
    LD_ASSERT(LDi_wrlock(&context.lock));
    context.flag = true;
    LD_ASSERT(LDi_wrunlock(&context.lock));

    while (true) {
        bool status;

        LD_ASSERT(LDi_wrlock(&context.lock));
        status = context.flag;
        LD_ASSERT(LDi_wrunlock(&context.lock));

        if (!status) {
            break;
        }

        LD_ASSERT(LDi_sleepMilliseconds(1));
    }

    LD_ASSERT(LDi_jointhread(thread));
    LD_ASSERT(LDi_rwlockdestroy(&context.lock));
}

/* possible failure but very unlikely */
static void
testRNG()
{
    unsigned int rng1, rng2;

    rng1 = 0;
    rng2 = 0;

    LD_ASSERT(LDi_random(&rng1));
    LD_ASSERT(rng1 != 0);
    LD_ASSERT(LDi_random(&rng2));
    LD_ASSERT(rng2 != 0);
    LD_ASSERT(rng1 != rng2);
}

static ld_mutex_t conditionTestLock;

static THREAD_RETURN
threadSignalCondition(void *const condition)
{
    LD_ASSERT(condition);

    LD_ASSERT(LDi_mtxlock(&conditionTestLock));
    LD_ASSERT(LDi_mtxunlock(&conditionTestLock));

    LDi_condsignal((ld_cond_t *)condition);

    return THREAD_RETURN_DEFAULT;
}

static void
testConditionVars()
{
    ld_cond_t condition;
    ld_thread_t thread;

    LD_ASSERT(LDi_mtxinit(&conditionTestLock));
    LDi_condinit(&condition);

    LD_ASSERT(LDi_mtxlock(&conditionTestLock));
    LD_ASSERT(LDi_createthread(&thread, threadSignalCondition, &condition));

    LD_ASSERT(LDi_condwait(&condition, &conditionTestLock, 1000));
    LD_ASSERT(LDi_mtxunlock(&conditionTestLock));

    LDi_conddestroy(&condition);
    LD_ASSERT(LDi_mtxdestroy(&conditionTestLock));
    LD_ASSERT(LDi_jointhread(thread));
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    testMonotonic();
    testGetUnixMilliseconds();
    testSleepMinimum();
    testThreadStartJoin();
    testRWLock();
    testConcurrency();
    testRNG();
    testConditionVars();

    return 0;
}
