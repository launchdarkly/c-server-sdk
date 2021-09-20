#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <string.h>

#include <launchdarkly/api.h>

#include "utility.h"

#include "test-utils/client.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class OfflineFixture : public CommonFixture {
};

TEST_F(OfflineFixture, BoolVariationDefaultValueOffline) {
    LDBoolean value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    ASSERT_TRUE(user = LDUserNew("abc"));
    ASSERT_TRUE(client = makeOfflineClient());
    /* test */
    value =
            LDBoolVariation(client, user, "featureKey", LDBooleanTrue, &details);
    /* validate */
    ASSERT_TRUE(value);
    ASSERT_EQ(details.reason, LD_ERROR);
    ASSERT_EQ(details.extra.errorKind, LD_CLIENT_NOT_READY);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(OfflineFixture, IntVariationDefaultValueOffline) {
    int value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    ASSERT_TRUE(user = LDUserNew("abc"));
    ASSERT_TRUE(client = makeOfflineClient());
    /* test */
    value = LDIntVariation(client, user, "featureKey", 100, &details);
    /* validate */
    ASSERT_EQ(value, 100);
    ASSERT_EQ(details.reason, LD_ERROR);
    ASSERT_EQ(details.extra.errorKind, LD_CLIENT_NOT_READY);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(OfflineFixture, DoubleVariationDefaultValueOffline) {
    double value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    ASSERT_TRUE(user = LDUserNew("abc"));
    ASSERT_TRUE(client = makeOfflineClient());
    /* test */
    value = LDDoubleVariation(client, user, "featureKey", 102.1, &details);
    /* validate */
    ASSERT_EQ(value, 102.1);
    ASSERT_EQ(details.reason, LD_ERROR);
    ASSERT_EQ(details.extra.errorKind, LD_CLIENT_NOT_READY);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(OfflineFixture, StringVariationDefaultValueOffline) {
    char *value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    ASSERT_TRUE(user = LDUserNew("abc"));
    ASSERT_TRUE(client = makeOfflineClient());
    /* test */
    value = LDStringVariation(client, user, "featureKey", "default", &details);
    /* validate */
    ASSERT_STREQ(value, "default");
    ASSERT_EQ(details.reason, LD_ERROR);
    ASSERT_EQ(details.extra.errorKind, LD_CLIENT_NOT_READY);
    /* cleanup */
    free(value);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(OfflineFixture, JSONVariationDefaultValueOffline) {
    struct LDJSON *actual, *expected;
    struct LDUser *user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    ASSERT_TRUE(user = LDUserNew("abc"));
    ASSERT_TRUE(client = makeOfflineClient());
    ASSERT_TRUE(expected = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(expected, "key1", LDNewNumber(3)));
    ASSERT_TRUE(LDObjectSetKey(expected, "key2", LDNewNumber(5)));
    /* test */
    actual = LDJSONVariation(client, user, "featureKey", expected, &details);
    /* validate */
    ASSERT_TRUE(LDJSONCompare(actual, expected));
    ASSERT_EQ(details.reason, LD_ERROR);
    ASSERT_EQ(details.extra.errorKind, LD_CLIENT_NOT_READY);
    /* cleanup */
    LDJSONFree(actual);
    LDJSONFree(expected);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

TEST_F(OfflineFixture, OfflineClientReturnsAsOffline) {
    struct LDClient *client;

    ASSERT_TRUE(client = makeOfflineClient());

    ASSERT_TRUE(LDClientIsOffline(client));

    LDClientClose(client);
}
