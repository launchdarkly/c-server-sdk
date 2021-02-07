#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "utility.h"

#include "test-utils/client.h"

static void
testBoolVariationDefaultValueOffline()
{
    bool             value;
    struct LDUser *  user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDBoolVariation(client, user, "featureKey", true, &details);
    /* validate */
    LD_ASSERT(value == true);
    LD_ASSERT(details.reason == LD_ERROR);
    LD_ASSERT(details.extra.errorKind == LD_CLIENT_NOT_READY);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testIntVariationDefaultValueOffline()
{
    int              value;
    struct LDUser *  user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDIntVariation(client, user, "featureKey", 100, &details);
    /* validate */
    LD_ASSERT(value == 100);
    LD_ASSERT(details.reason == LD_ERROR);
    LD_ASSERT(details.extra.errorKind == LD_CLIENT_NOT_READY);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testDoubleVariationDefaultValueOffline()
{
    double           value;
    struct LDUser *  user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDDoubleVariation(client, user, "featureKey", 102.1, &details);
    /* validate */
    LD_ASSERT(value == 102.1);
    LD_ASSERT(details.reason == LD_ERROR);
    LD_ASSERT(details.extra.errorKind == LD_CLIENT_NOT_READY);
    /* cleanup */
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testStringVariationDefaultValueOffline()
{
    char *           value;
    struct LDUser *  user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDStringVariation(client, user, "featureKey", "default", &details);
    /* validate */
    LD_ASSERT(strcmp(value, "default") == 0);
    LD_ASSERT(details.reason == LD_ERROR);
    LD_ASSERT(details.extra.errorKind == LD_CLIENT_NOT_READY);
    /* cleanup */
    free(value);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testJSONVariationDefaultValueOffline()
{
    struct LDJSON *  actual, *expected;
    struct LDUser *  user;
    struct LDClient *client;
    struct LDDetails details;
    /* prep */
    LDDetailsInit(&details);
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(expected = LDNewObject());
    LD_ASSERT(LDObjectSetKey(expected, "key1", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(expected, "key2", LDNewNumber(5)));
    /* test */
    actual = LDJSONVariation(client, user, "featureKey", expected, &details);
    /* validate */
    LD_ASSERT(LDJSONCompare(actual, expected));
    LD_ASSERT(details.reason == LD_ERROR);
    LD_ASSERT(details.extra.errorKind == LD_CLIENT_NOT_READY);
    /* cleanup */
    LDJSONFree(actual);
    LDJSONFree(expected);
    LDUserFree(user);
    LDClientClose(client);
    LDDetailsClear(&details);
}

static void
testOfflineClientReturnsAsOffline()
{
    struct LDClient *client;

    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(LDClientIsOffline(client) == true);

    LDClientClose(client);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testBoolVariationDefaultValueOffline();
    testIntVariationDefaultValueOffline();
    testDoubleVariationDefaultValueOffline();
    testStringVariationDefaultValueOffline();
    testJSONVariationDefaultValueOffline();
    testOfflineClientReturnsAsOffline();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
