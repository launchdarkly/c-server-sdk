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
    struct LDJSON *reason;
    /* prep */
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDBoolVariation(client, user, "featureKey", true, &details);
    /* validate */
    LD_ASSERT(value == true);
    LD_ASSERT(reason = LDObjectLookup(details, "reason"));
    LD_ASSERT(LDJSONGetType(reason) == LDObject);
    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(reason, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(reason, "errorKind"))) == 0);
    /* cleanup */
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
    struct LDJSON *reason;
    /* prep */
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDIntVariation(client, user, "featureKey", 100, &details);
    /* validate */
    LD_ASSERT(value == 100);
    LD_ASSERT(reason = LDObjectLookup(details, "reason"));
    LD_ASSERT(LDJSONGetType(reason) == LDObject);
    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(reason, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(reason, "errorKind"))) == 0);
    /* cleanup */
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
    struct LDJSON *reason;
    /* prep */
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDDoubleVariation(client, user, "featureKey", 102.1, &details);
    /* validate */
    LD_ASSERT(value == 102.1);
    LD_ASSERT(reason = LDObjectLookup(details, "reason"));
    LD_ASSERT(LDJSONGetType(reason) == LDObject);
    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(reason, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(reason, "errorKind"))) == 0);
    /* cleanup */
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
    struct LDJSON *reason;
    /* prep */
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    /* test */
    value = LDStringVariation(client, user, "featureKey", "default", &details);
    /* validate */
    LD_ASSERT(strcmp(value, "default") == 0);
    LD_ASSERT(reason = LDObjectLookup(details, "reason"));
    LD_ASSERT(LDJSONGetType(reason) == LDObject);
    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(reason, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(reason, "errorKind"))) == 0);
    /* cleanup */
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
    struct LDJSON *reason;
    /* prep */
    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(expected = LDNewObject());
    LD_ASSERT(LDObjectSetKey(expected, "key1", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(expected, "key2", LDNewNumber(5)));
    /* test */
    actual = LDJSONVariation(client, user, "featureKey", expected, &details);
    /* validate */
    LD_ASSERT(LDJSONCompare(actual, expected));
    LD_ASSERT(reason = LDObjectLookup(details, "reason"));
    LD_ASSERT(LDJSONGetType(reason) == LDObject);
    LD_ASSERT(strcmp("ERROR", LDGetText(LDObjectLookup(reason, "kind"))) == 0);
    LD_ASSERT(strcmp("CLIENT_NOT_READY",
        LDGetText(LDObjectLookup(reason, "errorKind"))) == 0);
    /* cleanup */
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
