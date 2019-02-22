#include "ldinternal.h"

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

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    testMonotonic();
    testSleepMinimum();

    return 0;
}
