#include <launchdarkly/api.h>

#include "assertion.h"
#include "utility.h"

static void
testGenerateUUIDv4()
{
    char buffer[LD_UUID_SIZE];
    /* This is mainly called as something for Valgrind to analyze */
    LD_ASSERT(LDi_UUIDv4(buffer));
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testGenerateUUIDv4();

    LDBasicLoggerThreadSafeShutdown();
}
