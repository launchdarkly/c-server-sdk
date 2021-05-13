#include <launchdarkly/api.h>

#include "assertion.h"
#include "lru.h"
#include "utility.h"

void
testInsertExisting()
{
    struct LDLRU *lru;

    LD_ASSERT(lru = LDLRUInit(10));

    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "abc"));
    LD_ASSERT(LDLRUSTATUS_EXISTED == LDLRUInsert(lru, "abc"));

    LDLRUFree(lru);
}

void
testMaxCapacity()
{
    struct LDLRU *lru;

    LD_ASSERT(lru = LDLRUInit(2));

    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "123"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "456"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "789"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "123"));
    LD_ASSERT(LDLRUSTATUS_EXISTED == LDLRUInsert(lru, "789"));

    LDLRUFree(lru);
}

void
testAccessBumpsPosition()
{
    struct LDLRU *lru;

    LD_ASSERT(lru = LDLRUInit(3));

    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "123"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "456"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "789"));
    LD_ASSERT(LDLRUSTATUS_EXISTED == LDLRUInsert(lru, "123"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "ABC"));
    LD_ASSERT(LDLRUSTATUS_EXISTED == LDLRUInsert(lru, "123"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "456"));

    LDLRUFree(lru);
}

void
testZeroCapacityAlwaysNew()
{
    struct LDLRU *lru;

    LD_ASSERT(lru = LDLRUInit(0));

    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "123"));
    LD_ASSERT(LDLRUSTATUS_NEW == LDLRUInsert(lru, "123"));

    LDLRUFree(lru);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testInsertExisting();
    testMaxCapacity();
    testAccessBumpsPosition();
    testZeroCapacityAlwaysNew();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
