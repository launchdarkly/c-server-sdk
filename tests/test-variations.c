#include <math.h>
#include <float.h>

#include <launchdarkly/api.h>

#include "user.h"
#include "config.h"
#include "client.h"
#include "evaluate.h"
#include "misc.h"
#include "util-flags.h"

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
testBoolVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    bool actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewBool(false));
    addVariation(flag, LDNewBool(true));
    LD_ASSERT(LDStoreInitEmpty(client->config->store));
    LDStoreUpsert(client->config->store, LD_FLAG, flag);
    /* run */
    actual = LDBoolVariation(client, user, "validFeatureKey", false, &details);
    /* validate */
    LD_ASSERT(actual == true);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testIntVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    int actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewNumber(-1));
    addVariation(flag, LDNewNumber(100));
    LD_ASSERT(LDStoreInitEmpty(client->config->store));
    LDStoreUpsert(client->config->store, LD_FLAG, flag);
    /* run */
    actual = LDIntVariation(client, user, "validFeatureKey", 1000, &details);
    /* validate */
    LD_ASSERT(actual == 100);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testDoubleVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    double actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewNumber(-1));
    addVariation(flag, LDNewNumber(100.01));
    LD_ASSERT(LDStoreInitEmpty(client->config->store));
    LDStoreUpsert(client->config->store, LD_FLAG, flag);
    /* run */
    actual = LDDoubleVariation(client, user, "validFeatureKey", 0.0, &details);
    /* validate */
    LD_ASSERT(actual == 100.01);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testStringVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    char *actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewText("a"));
    addVariation(flag, LDNewText("b"));
    LD_ASSERT(LDStoreInitEmpty(client->config->store));
    LDStoreUpsert(client->config->store, LD_FLAG, flag);
    /* run */
    actual = LDStringVariation(client, user, "validFeatureKey", "a", &details);
    /* validate */
    LD_ASSERT(strcmp(actual, "b") == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    /* cleanup */
    free(actual);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testJSONVariation()
{
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *actual, *flag, *expected, *other, *def;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* values */
    LD_ASSERT(expected = LDNewObject());
    LD_ASSERT(LDObjectSetKey(expected, "field2", LDNewText("value2")))
    LD_ASSERT(other = LDNewObject());
    LD_ASSERT(LDObjectSetKey(other, "field1", LDNewText("value1")))
    LD_ASSERT(def = LDNewObject());
    LD_ASSERT(LDObjectSetKey(def, "default", LDNewText("default")));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    setFallthrough(flag, 1);
    addVariation(flag, other);
    addVariation(flag, expected);
    LD_ASSERT(LDStoreInitEmpty(client->config->store));
    LDStoreUpsert(client->config->store, LD_FLAG, flag);
    /* run */
    actual = LDJSONVariation(client, user, "validFeatureKey", def, &details);
    /* validate */
    LD_ASSERT(LDJSONCompare(actual, expected));
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    /* cleanup */
    LDJSONFree(def);
    LDJSONFree(actual);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testBoolVariation();
    testIntVariation();
    testDoubleVariation();
    testStringVariation();
    testJSONVariation();

    return 0;
}
