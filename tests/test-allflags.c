#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "utility.h"
#include "store.h"

#include "test-utils/flags.h"
#include "test-utils/client.h"

static void
testAllFlags()
{
    struct LDJSON *flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *user;

    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("flag1")));
    LD_ASSERT(LDObjectSetKey(flag1, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(true)));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("flag2")));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(true)));
    setFallthrough(flag2, 1);
    addVariation(flag2, LDNewText("c"));
    addVariation(flag2, LDNewText("d"));

    /* store */
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag1));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag2));

    /* test */
    LD_ASSERT(allFlags = LDAllFlags(client, user));

    /* validation */
    LD_ASSERT(LDCollectionGetSize(allFlags) == 2);
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(allFlags, "flag1")), "b") == 0);
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(allFlags, "flag2")), "d") == 0);

    /* cleanup */
    LDJSONFree(allFlags);
    LDUserFree(user);
    LDClientClose(client);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testAllFlags();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
