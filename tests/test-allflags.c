#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "store.h"
#include "utility.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"

static void
testAllFlags()
{
    struct LDJSON *  flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *  user;

    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("flag1")));
    LD_ASSERT(LDObjectSetKey(flag1, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("flag2")));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
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

/* If there is a problem with a single flag, then that should not prevent returning other flags.
 * In this test one of the flags will have an invalid fallthrough that contains neither a variation
 * nor rollout.
 */
static void
testAllFlagsWithFlagWithFallthroughWithNoVariationAndNoRollout()
{
    struct LDJSON *  flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *  user;

    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("flag1")));
    LD_ASSERT(LDObjectSetKey(flag1, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("flag2")));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag2, "salt", LDNewText("abc")));

    /* Set a fallthrough which has no variation or rollout. */
    struct LDJSON *fallthrough;
    LD_ASSERT(fallthrough = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "fallthrough", fallthrough));

    /* store */
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag1));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag2));

    /* test */
    LD_ASSERT(allFlags = LDAllFlags(client, user));

    /* validation */
    LD_ASSERT(LDCollectionGetSize(allFlags) == 1);
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(allFlags, "flag1")), "b") == 0);

    /* cleanup */
    LDJSONFree(allFlags);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testAllFlagsNoFlagsInStore()
{
    struct LDJSON *  flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *  user;

    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));

    /* store */
    LD_ASSERT(LDStoreInitEmpty(client->store));

    /* test */
    LD_ASSERT(allFlags = LDAllFlags(client, user));

    /* validation */
    LD_ASSERT(LDCollectionGetSize(allFlags) == 0);

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
    testAllFlagsWithFlagWithFallthroughWithNoVariationAndNoRollout();
    testAllFlagsNoFlagsInStore();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
