#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <float.h>
#include <math.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "store.h"
#include "user.h"
#include "utility.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class VariationsFixture : public CommonFixture {
};

TEST_F(VariationsFixture, BoolVariation) {
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    LDBoolean actual;
    struct LDDetails details;
    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewBool(LDBooleanFalse));
    addVariation(flag, LDNewBool(LDBooleanTrue));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDBoolVariation(
            client, user, "validFeatureKey", LDBooleanFalse, &details);
    /* validate */
    ASSERT_EQ(actual, LDBooleanTrue);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, IntVariation) {
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    int actual;
    struct LDDetails details;
    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewNumber(-1));
    addVariation(flag, LDNewNumber(100));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDIntVariation(client, user, "validFeatureKey", 1000, &details);
    /* validate */
    ASSERT_EQ(actual, 100);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, DoubleVariation) {
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    double actual;
    struct LDDetails details;
    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewNumber(-1));
    addVariation(flag, LDNewNumber(100.01));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDDoubleVariation(client, user, "validFeatureKey", 0.0, &details);
    /* validate */
    ASSERT_EQ(actual, 100.01);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

class VariationsDoubleFixture : public CommonFixture, public testing::WithParamInterface<std::pair<int, double>> {
};

TEST_P(VariationsDoubleFixture, DoubleVariationAsInt) {
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    int actual;

    const std::pair<int, double> &param = GetParam();

    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(
            flag = makeMinimalFlag(
                    "validFeatureKey", 1, LDBooleanTrue, LDBooleanFalse));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(param.second));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDIntVariation(client, user, "validFeatureKey", 0, NULL);
    /* validate */
    ASSERT_EQ(actual, param.first);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
}

INSTANTIATE_TEST_SUITE_P(
        DoubleVariationsAsInt,
        VariationsDoubleFixture,
        ::testing::Values(
                std::pair<int, double>(100, 100.01),
                std::pair<int, double>(99, 99.99),
                std::pair<int, double>(-1, -1.1)
        ));


TEST_F(VariationsFixture, StringVariation) {
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    char *actual;
    struct LDDetails details;
    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewText("a"));
    addVariation(flag, LDNewText("b"));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDStringVariation(client, user, "validFeatureKey", "a", &details);
    /* validate */
    ASSERT_STREQ(actual, "b");
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    /* cleanup */
    LDFree(actual);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, VariationNullFallback) {
    struct LDClient *client;
    struct LDUser *user;
    char *actual;
    struct LDDetails details;
    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    /* run */
    actual =
            LDStringVariation(client, user, "invalidFeatureKey", NULL, &details);
    /* validate */
    ASSERT_EQ(actual, nullptr);
    ASSERT_EQ(details.reason, LD_ERROR);
    ASSERT_EQ(details.extra.errorKind, LD_FLAG_NOT_FOUND);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, JSONVariation) {
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *actual, *flag, *expected, *other, *def;
    struct LDDetails details;
    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* values */
    ASSERT_TRUE(expected = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(expected, "field2", LDNewText("value2")));
    ASSERT_TRUE(other = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(other, "field1", LDNewText("value1")));
    ASSERT_TRUE(def = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(def, "default", LDNewText("default")));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, other);
    addVariation(flag, expected);
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDJSONVariation(client, user, "validFeatureKey", def, &details);
    /* validate */
    ASSERT_TRUE(LDJSONCompare(actual, expected));
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    /* cleanup */
    LDJSONFree(def);
    LDJSONFree(actual);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, JSONVariationNullFallback) {
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *actual;
    struct LDDetails details;
    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    /* run */
    actual = LDJSONVariation(client, user, "invalidFeatureKey", NULL, &details);
    /* validate */
    ASSERT_EQ(actual, nullptr);
    ASSERT_EQ(details.reason, LD_ERROR);
    ASSERT_EQ(details.extra.errorKind, LD_FLAG_NOT_FOUND);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, FallthroughWithNoVariationOrRollout) {
    struct LDJSON *flag, *fallthrough, *variation, *rollout, *variations;
    struct LDClient *client;
    struct LDUser *user;
    char *actual;
    struct LDDetails details;

    struct LDJSON *events;

    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNull()));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(3)));

    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "fallthrough", fallthrough));

    addVariation(flag, LDNewText("ExpectedPrefix_A"));
    addVariation(flag, LDNewText("ExpectedPrefix_B"));
    addVariation(flag, LDNewText("ExpectedPrefix_C"));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDStringVariation(client, user, "feature0", NULL, &details);
    /* The schema will fail, so the fallback value will be used. */
    /* validate */
    ASSERT_FALSE(actual);
    ASSERT_EQ(details.reason, LD_ERROR);

    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, OffWithNullOffVariation) {
    struct LDJSON *flag, *fallthrough, *variation, *rollout, *variations;
    struct LDClient *client;
    struct LDUser *user;
    char *actual;
    struct LDDetails details;

    struct LDJSON *events;

    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNull()));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(3)));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDStringVariation(client, user, "feature0", "test", &details);
    /* It is valid to have a null off variation, and we should return the default value. */
    /* validate */
    ASSERT_STREQ("test", actual);
    LDFree(actual);
    ASSERT_EQ(details.reason, LD_OFF);

    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(VariationsFixture, OffWithUndefinedOffVariation) {
    struct LDJSON *flag, *fallthrough, *variation, *rollout, *variations;
    struct LDClient *client;
    struct LDUser *user;
    char *actual;
    struct LDDetails details;

    struct LDJSON *events;

    /* setup */
    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));
    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(3)));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDStringVariation(client, user, "feature0", "test", &details);
    /* It is valid to have an undefined off variation, and we should return the default value. */
    /* validate */
    ASSERT_STREQ("test", actual);
    LDFree(actual);
    ASSERT_EQ(details.reason, LD_OFF);

    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}


TEST_F(VariationsFixture, TestNullOffVariationInPrerequisiteDoesNotCauseUseAfterFree) {
    struct LDJSON *flag;
    struct LDDetails details;
    struct LDClient *client;
    struct LDUser *user = LDUserNew("foo");

    ASSERT_TRUE(client = makeTestClient());

    LDDetailsInit(&details);

    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "clientSide", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "debugEventsUntilDate", LDNewNull()));
    ASSERT_TRUE(LDObjectSetKey(flag, "deleted", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("streaming.03.featureKey")));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNull()));
    ASSERT_TRUE(LDObjectSetKey(flag, "trackEvents", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "trackEventsFallthrough", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "targets", LDNewArray()));
    ASSERT_TRUE(LDObjectSetKey(flag, "rules", LDNewArray()));
    ASSERT_TRUE(LDObjectSetKey(flag, "version", LDNewNumber(0)));


    struct LDJSON *variations = LDNewArray();
    ASSERT_TRUE(LDArrayPush(variations, LDNewText("A")));
    ASSERT_TRUE(LDArrayPush(variations, LDNewText("B")));

    ASSERT_TRUE(LDObjectSetKey(flag, "variations", variations));

    struct LDJSON *fallthrough;
    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(fallthrough, "variation", LDNewNumber(0)));
    ASSERT_TRUE(LDObjectSetKey(flag, "fallthrough", fallthrough));

    struct LDJSON *prerequisites = LDNewArray();
    struct LDJSON *prereq = LDNewObject();
    ASSERT_TRUE(LDObjectSetKey(prereq, "key", LDNewText("streaming.03.prereqFeatureKey")));
    ASSERT_TRUE(LDObjectSetKey(prereq, "variation", LDNewNumber(0)));
    ASSERT_TRUE(LDArrayPush(prerequisites, prereq));

    ASSERT_TRUE(LDObjectSetKey(flag, "prerequisites", prerequisites));

    struct LDJSON *prerequisite = LDNewObject();
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "clientSide", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "debugEventsUntilDate", LDNewNull()));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "deleted", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "key", LDNewText("streaming.03.prereqFeatureKey")));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "offVariation", LDNewNull()));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "on", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "prerequisites", LDNewArray()));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "rules", LDNewArray()));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "salt", LDNewText("")));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "trackEvents", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "trackEventsFallthrough", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "targets", LDNewArray()));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "version", LDNewNumber(0)));

    struct LDJSON *pfallthrough;
    ASSERT_TRUE(pfallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(pfallthrough, "variation", LDNewNumber(0)));
    ASSERT_TRUE(LDObjectSetKey(prerequisite, "fallthrough", pfallthrough));

    struct LDJSON *pvariations = LDNewArray();
    LDArrayPush(pvariations, LDNewText("first"));
    LDArrayPush(pvariations, LDNewText("second"));

    LDObjectSetKey(prerequisite, "variations", pvariations);

    ASSERT_TRUE(LDStoreInitEmpty(client->store));

    LDStoreUpsert(client->store, LD_FLAG, prerequisite);
    LDStoreUpsert(client->store, LD_FLAG, flag);

    char *result = LDStringVariation(client, user, "streaming.03.featureKey", "default", &details);

    LDUserFree(user);
    LDDetailsClear(&details);
    LDFree(result);
    LDClientClose(client);

}
