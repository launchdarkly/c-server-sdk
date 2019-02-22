#include "ldapi.h"
#include "ldinternal.h"
#include "lduser.h"
#include "ldevaluate.h"
#include "ldstore.h"
#include "ldvariations.h"
#include "ldconfig.h"

#include <math.h>
#include <float.h>

static void
setFallthrough(struct LDJSON *const flag, const unsigned int variation)
{
    struct LDJSON *tmp;

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(variation)));
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", tmp));
}

static void
addVariation(struct LDJSON *const flag, struct LDJSON *const variation)
{
    struct LDJSON *variations;

    if (!(variations = LDObjectLookup(flag, "variations"))) {
        LD_ASSERT(variations = LDNewArray());
        LDObjectSetKey(flag, "variations", variations);
    }

    LD_ASSERT(LDArrayAppend(variations, variation));
}

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
    struct LDJSON *reason = NULL;
    bool actual;

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
    LDStoreUpsert(client->config->store, "flags", flag);

    /* run */
    actual = LDBoolVariation(client, user, "validFeatureKey", false, &reason);

    /* validate */
    LD_ASSERT(actual == true);
    LD_ASSERT(reason);
    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(reason, "kind"))) == 0);

    LDUserFree(user);
    LDClientClose(client);
}

static void
testIntVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *reason = NULL;
    int actual;

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
    LDStoreUpsert(client->config->store, "flags", flag);

    /* run */
    actual = LDIntVariation(client, user, "validFeatureKey", 1000, &reason);

    /* validate */
    LD_ASSERT(actual == 100);
    LD_ASSERT(reason);
    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(reason, "kind"))) == 0);

    LDUserFree(user);
    LDClientClose(client);
}

static void
testDoubleVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *reason = NULL;
    double actual;

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
    LDStoreUpsert(client->config->store, "flags", flag);

    /* run */
    actual = LDDoubleVariation(client, user, "validFeatureKey", 0.0, &reason);

    /* validate */
    LD_ASSERT(actual == 100.01);
    LD_ASSERT(reason);
    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(reason, "kind"))) == 0);

    LDUserFree(user);
    LDClientClose(client);
}

static void
testStringVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *reason = NULL;
    char *actual;

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
    LDStoreUpsert(client->config->store, "flags", flag);

    /* run */
    actual = LDStringVariation(client, user, "validFeatureKey", "a", &reason);

    /* validate */
    LD_ASSERT(strcmp(actual, "b") == 0);
    LD_ASSERT(reason);
    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(reason, "kind"))) == 0);

    free(actual);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testJSONVariation()
{
    struct LDJSON *flag;
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *reason;
    struct LDJSON *actual;

    struct LDJSON *expected;
    struct LDJSON *other;
    struct LDJSON *def;

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
    LDStoreUpsert(client->config->store, "flags", flag);

    /* run */
    actual = LDJSONVariation(client, user, "validFeatureKey", def, &reason);

    /* validate */
    LD_ASSERT(LDJSONCompare(actual, expected));
    LD_ASSERT(reason);
    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(reason, "kind"))) == 0);

    LDJSONFree(def);
    LDJSONFree(actual);
    LDUserFree(user);
    LDClientClose(client);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    testBoolVariation();
    testIntVariation();
    testDoubleVariation();
    testStringVariation();
    testJSONVariation();

    return 0;
}