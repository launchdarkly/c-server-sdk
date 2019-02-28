#include "ldconfig.h"
#include "ldclient.h"
#include "ldvariations.h"
#include "ldinternal.h"
#include "ldjson.h"

struct LDClient *
makeOfflineClient()
{
    struct LDConfig *config;
    struct LDClient *client;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigSetOffline(config, true);

    LD_ASSERT(client = LDClientInit(config, 0));

    return client;
}

static void
testBoolVariationDefaultValueOffline()
{
    bool value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *details;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());

    value = LDBoolVariation(client, user, "featureKey", true, &details);

    LD_ASSERT(value == true);

    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(details, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(details, "errorKind"))) == 0);

    LDJSONFree(details);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testIntVariationDefaultValueOffline()
{
    int value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *details;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());

    value = LDIntVariation(client, user, "featureKey", 100, &details);

    LD_ASSERT(value == 100);

    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(details, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(details, "errorKind"))) == 0);

    LDJSONFree(details);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testDoubleVariationDefaultValueOffline()
{
    double value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *details;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());

    value = LDDoubleVariation(client, user, "featureKey", 102.1, &details);

    LD_ASSERT(value == 102.1);

    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(details, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(details, "errorKind"))) == 0);

    LDJSONFree(details);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testStringVariationDefaultValueOffline()
{
    char *value;
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *details;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());

    value = LDStringVariation(client, user, "featureKey", "default", &details);

    LD_ASSERT(strcmp(value, "default") == 0);

    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(details, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(details, "errorKind"))) == 0);

    LDJSONFree(details);
    free(value);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testJSONVariationDefaultValueOffline()
{
    struct LDJSON *actual;
    struct LDJSON *expected;
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *details;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(expected = LDNewObject());
    LD_ASSERT(LDObjectSetKey(expected, "key1", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(expected, "key2", LDNewNumber(5)));

    actual = LDJSONVariation(client, user, "featureKey", expected, &details);

    LD_ASSERT(LDJSONCompare(actual, expected));

    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(details, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(details, "errorKind"))) == 0);

    LDJSONFree(details);
    LDJSONFree(actual);
    LDJSONFree(expected);
    LDUserFree(user);
    LDClientClose(client);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testBoolVariationDefaultValueOffline();
    testIntVariationDefaultValueOffline();
    testDoubleVariationDefaultValueOffline();
    testStringVariationDefaultValueOffline();
    testJSONVariationDefaultValueOffline();

    return 0;
}
