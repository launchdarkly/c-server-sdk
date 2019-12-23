#include <launchdarkly/api.h>

#include "misc.h"

#include "util-store.h"

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore *store;

    LD_ASSERT(store = LDStoreNew(NULL));
    LD_ASSERT(!LDStoreInitialized(store));

    return store;
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    runSharedStoreTests(prepareEmptyStore);

    return 0;
}
