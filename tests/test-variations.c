#include <float.h>
#include <math.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "store.h"
#include "user.h"
#include "utility.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"

static void
testBoolVariation()
{
    struct LDJSON *  flag;
    struct LDClient *client;
    struct LDUser *  user;
    LDBoolean        actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewBool(LDBooleanFalse));
    addVariation(flag, LDNewBool(LDBooleanTrue));
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDBoolVariation(
        client, user, "validFeatureKey", LDBooleanFalse, &details);
    /* validate */
    LD_ASSERT(actual == LDBooleanTrue);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testIntVariation()
{
    struct LDJSON *  flag;
    struct LDClient *client;
    struct LDUser *  user;
    int              actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewNumber(-1));
    addVariation(flag, LDNewNumber(100));
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
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
    struct LDJSON *  flag;
    struct LDClient *client;
    struct LDUser *  user;
    double           actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewNumber(-1));
    addVariation(flag, LDNewNumber(100.01));
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
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
testDoubleVariationAsIntHelper(const int expected, const double flagValue)
{
    struct LDJSON *  flag;
    struct LDClient *client;
    struct LDUser *  user;
    int              actual;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(
        flag = makeMinimalFlag(
            "validFeatureKey", 1, LDBooleanTrue, LDBooleanFalse))
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(flagValue));
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDIntVariation(client, user, "validFeatureKey", 0, NULL);
    /* validate */
    LD_ASSERT(actual == expected);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
}

static void
testDoubleVariationAsInt()
{
    testDoubleVariationAsIntHelper(100, 100.01);
    testDoubleVariationAsIntHelper(99, 99.99);
    testDoubleVariationAsIntHelper(-1, -1.1);
}

static void
testStringVariation()
{
    struct LDJSON *  flag;
    struct LDClient *client;
    struct LDUser *  user;
    char *           actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("validFeatureKey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, LDNewText("a"));
    addVariation(flag, LDNewText("b"));
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDStringVariation(client, user, "validFeatureKey", "a", &details);
    /* validate */
    LD_ASSERT(strcmp(actual, "b") == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    /* cleanup */
    LDFree(actual);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testStringVariationNullFallback()
{
    struct LDClient *client;
    struct LDUser *  user;
    char *           actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    LD_ASSERT(LDStoreInitEmpty(client->store));
    /* run */
    actual =
        LDStringVariation(client, user, "invalidFeatureKey", NULL, &details);
    /* validate */
    LD_ASSERT(actual == NULL);
    LD_ASSERT(details.reason == LD_ERROR);
    LD_ASSERT(details.extra.errorKind == LD_FLAG_NOT_FOUND);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testJSONVariation()
{
    struct LDClient *client;
    struct LDUser *  user;
    struct LDJSON *  actual, *flag, *expected, *other, *def;
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
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 1);
    addVariation(flag, other);
    addVariation(flag, expected);
    LD_ASSERT(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
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

static void
testJSONVariationNullFallback()
{
    struct LDClient *client;
    struct LDUser *  user;
    struct LDJSON *  actual;
    struct LDDetails details;
    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    LD_ASSERT(LDStoreInitEmpty(client->store));
    /* run */
    actual = LDJSONVariation(client, user, "invalidFeatureKey", NULL, &details);
    /* validate */
    LD_ASSERT(actual == NULL);
    LD_ASSERT(details.reason == LD_ERROR);
    LD_ASSERT(details.extra.errorKind == LD_FLAG_NOT_FOUND);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testFallthroughWithNoVariationOrRollout()
{
    struct LDJSON *flag, *fallthrough, *variation, *rollout, *variations;
    struct LDClient *client;
    struct LDUser *  user;
    char              *actual;
    struct LDDetails details;

    struct LDJSON *events;

    /* setup */
    LD_ASSERT(client = makeTestClient());
    LD_ASSERT(user = LDUserNew("userkey"));
    /* flag */
    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNull()));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(3)));

    LD_ASSERT(fallthrough = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", fallthrough));

    addVariation(flag, LDNewText("ExpectedPrefix_A"));
    addVariation(flag, LDNewText("ExpectedPrefix_B"));
    addVariation(flag, LDNewText("ExpectedPrefix_C"));

    LD_ASSERT(LDStoreInitEmpty(client->store));
    LDStoreUpsert(client->store, LD_FLAG, flag);
    /* run */
    actual = LDStringVariation(client, user, "feature0", NULL, &details);
    /* The schema will fail, so the fallback value will be used. */
    /* validate */
    LD_ASSERT(!actual);
    LD_ASSERT(details.reason == LD_ERROR);

    // Need a way here to check if an event was generated.

    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testBoolVariation();
    testIntVariation();
    testDoubleVariation();
    testDoubleVariationAsInt();
    testStringVariation();
    testStringVariationNullFallback();
    testJSONVariation();
    testJSONVariationNullFallback();
    testFallthroughWithNoVariationOrRollout();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
