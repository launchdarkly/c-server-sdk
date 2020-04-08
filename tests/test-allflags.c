#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "utility.h"
#include "util-flags.h"
#include "store.h"

static struct LDClient *
makeTestClient()
{
    struct LDClient *client;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew("key"));
    LD_ASSERT(client = LDClientInit(config, 0));

    return client;
}

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

static void
testAllFlagsReturnsNilIfUserKeyIsNil()
{
    struct LDJSON *flag1, *flag2;
    struct LDClient *client;

    LD_ASSERT(client = makeTestClient());

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

    /* test / validation */
    LD_ASSERT(!LDAllFlags(client, NULL));

    /* cleanup */
    LDClientClose(client);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testAllFlags();
    testAllFlagsReturnsNilIfUserKeyIsNil();

    return 0;
}
