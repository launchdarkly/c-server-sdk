#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <string.h>

#include <launchdarkly/api.h>
#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "store.h"
#include "utility.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class AllFlagsFixture : public CommonFixture {
};

TEST_F(AllFlagsFixture, AllFlagsValid) {
    struct LDJSON *flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *user;

    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("flag1")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("flag2")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    setFallthrough(flag2, 1);
    addVariation(flag2, LDNewText("c"));
    addVariation(flag2, LDNewText("d"));

    /* store */
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag1));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag2));

    /* test */
    ASSERT_TRUE(allFlags = LDAllFlags(client, user));

    /* validation */
    ASSERT_EQ(LDCollectionGetSize(allFlags), 2);
    ASSERT_STREQ(LDGetText(LDObjectLookup(allFlags, "flag1")), "b");
    ASSERT_STREQ(LDGetText(LDObjectLookup(allFlags, "flag2")), "d");

    /* cleanup */
    LDJSONFree(allFlags);
    LDUserFree(user);
    LDClientClose(client);
}

/* If there is a problem with a single flag, then that should not prevent returning other flags.
 * In this test one of the flags will have an invalid fallthrough that contains neither a variation
 * nor rollout.
 */
TEST_F(AllFlagsFixture, AllFlagsWithFlagWithFallthroughWithNoVariationAndNoRollout) {
    struct LDJSON *flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *user;

    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("flag1")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("flag2")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));

    /* Set a fallthrough which has no variation or rollout. */
    struct LDJSON *fallthrough;
    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "fallthrough", fallthrough));

    /* store */
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag1));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag2));

    /* test */
    ASSERT_TRUE(allFlags = LDAllFlags(client, user));

    /* validation */
    ASSERT_EQ(LDCollectionGetSize(allFlags), 1);
    ASSERT_STREQ(LDGetText(LDObjectLookup(allFlags, "flag1")), "b");

    /* cleanup */
    LDJSONFree(allFlags);
    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(AllFlagsFixture, AllFlagsNoFlagsInStore) {
    struct LDJSON *allFlags;
    struct LDClient *client;
    struct LDUser *user;

    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* store */
    ASSERT_TRUE(LDStoreInitEmpty(client->store));

    /* test */
    ASSERT_TRUE(allFlags = LDAllFlags(client, user));

    /* validation */
    ASSERT_EQ(LDCollectionGetSize(allFlags), 0);

    /* cleanup */
    LDJSONFree(allFlags);
    LDUserFree(user);
    LDClientClose(client);
}
