#include <launchdarkly/api.h>

#include "assertion.h"
#include "utility.h"

#include "test-utils/store.h"

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore * store;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew(""));
    LD_ASSERT(store = LDStoreNew(config));
    LD_ASSERT(!LDStoreInitialized(store));

    LDConfigFree(config);

    return store;
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    runSharedStoreTests(prepareEmptyStore);

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
