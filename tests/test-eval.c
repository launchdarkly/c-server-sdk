#include <float.h>
#include <math.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "evaluate.h"
#include "store.h"
#include "test-utils/flags.h"
#include "utility.h"

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore * store;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew(""));
    LD_ASSERT(store = LDStoreNew(config));
    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(LDStoreInitEmpty(store));
    LD_ASSERT(LDStoreInitialized(store));

    LDConfigFree(config);

    return store;
}

static void
addPrerequisite(
    struct LDJSON *const flag,
    const char *const    key,
    const unsigned int   variation)
{
    struct LDJSON *tmp, *prerequisites;

    if (!(prerequisites = LDObjectLookup(flag, "prerequisites"))) {
        LD_ASSERT(prerequisites = LDNewArray());
        LDObjectSetKey(flag, "prerequisites", prerequisites);
    }

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(tmp, "key", LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(variation)));

    LD_ASSERT(LDArrayPush(prerequisites, tmp));
}

static struct LDJSON *
booleanFlagWithClause(struct LDJSON *const clause)
{
    struct LDJSON *flag, *rule, *clauses, *rules;

    LD_ASSERT(clauses = LDNewArray());
    LD_ASSERT(LDArrayPush(clauses, clause));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "id", LDNewText("rule-id")));
    LD_ASSERT(LDObjectSetKey(rule, "clauses", clauses));
    LD_ASSERT(LDObjectSetKey(rule, "variation", LDNewNumber(1)));

    LD_ASSERT(rules = LDNewArray());
    LD_ASSERT(LDArrayPush(rules, rule));

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", rules));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewBool(LDBooleanFalse));
    addVariation(flag, LDNewBool(LDBooleanTrue));

    return flag;
}

static void
returnsOffVariationIfFlagIsOff()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    addVariations1(flag);
    setFallthrough(flag, 0);

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *)1,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MISS);

    /* validation */
    LD_ASSERT(strcmp("off", LDGetText(result)) == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 1);
    LD_ASSERT(details.reason == LD_OFF);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagReturnsNilIfFlagIsOffAndOffVariationIsUnspecified()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 0);
    addVariations1(flag);

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *)1,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MISS);

    /* validation */
    LD_ASSERT(!result);
    LD_ASSERT(!details.hasVariation);
    LD_ASSERT(details.reason == LD_OFF);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagReturnsFallthroughIfFlagIsOnAndThereAreNoRules()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *events;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    LD_ASSERT(config = LDConfigNew("abc"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", LDNewArray()));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 0);
    addVariations1(flag);

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            client,
            flag,
            user,
            (struct LDStore *)1,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "fall") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

static void
testFlagReturnsErrorForFallthroughWithNoVariationAndNoRollout()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *events;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    LD_ASSERT(config = LDConfigNew("abc"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", LDNewArray()));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));

    /* Set a fallthrough which has no variation or rollout. */
    struct LDJSON *fallthrough;
    LD_ASSERT(fallthrough = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", fallthrough));

    /* run */
    LD_ASSERT(
            LDi_evaluate(
                    client,
                    flag,
                    user,
                    (struct LDStore *)1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse) == EVAL_SCHEMA);

    LD_ASSERT(!details.hasVariation);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

static void
testFlagReturnsOffVariationIfPrerequisiteIsOff()
{
    struct LDUser *  user;
    struct LDStore * store;
    struct LDJSON *  flag1, *flag2, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    LD_ASSERT(config = LDConfigNew("abc"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanFalse)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    addVariations2(flag2);

    /* store setup */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, flag2));

    /* run */
    LD_ASSERT(LDi_evaluate(
        client,
        flag1,
        user,
        store,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "off") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 1);
    LD_ASSERT(details.reason == LD_PREREQUISITE_FAILED);
    LD_ASSERT(strcmp("feature1", details.extra.prerequisiteKey) == 0)

    LD_ASSERT(events);
    LD_ASSERT(LDCollectionGetSize(events) == 1);
    LD_ASSERT(eventsiter = LDGetIter(events));
    LD_ASSERT(
        strcmp("feature1", LDGetText(LDObjectLookup(eventsiter, "key"))) == 0);
    LD_ASSERT(
        strcmp("go", LDGetText(LDObjectLookup(eventsiter, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "variation")) == 1);
    LD_ASSERT(
        strcmp("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf"))) ==
        0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

static void
testFlagReturnsOffVariationIfPrerequisiteIsNotMet()
{
    struct LDUser *  user;
    struct LDStore * store;
    struct LDJSON *  flag1, *flag2, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    LD_ASSERT(config = LDConfigNew("abc"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(2)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    addVariations2(flag2);
    setFallthrough(flag2, 0);

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, flag2));

    /* run */
    LD_ASSERT(LDi_evaluate(
        client,
        flag1,
        user,
        store,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "off") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 1);
    LD_ASSERT(details.reason == LD_PREREQUISITE_FAILED);

    LD_ASSERT(events);
    LD_ASSERT(LDCollectionGetSize(events) == 1);
    LD_ASSERT(eventsiter = LDGetIter(events));
    LD_ASSERT(
        strcmp("feature1", LDGetText(LDObjectLookup(eventsiter, "key"))) == 0);
    LD_ASSERT(
        strcmp("nogo", LDGetText(LDObjectLookup(eventsiter, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "version")) == 2);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "variation")) == 0);
    LD_ASSERT(
        strcmp("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf"))) ==
        0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

static void
testFlagReturnsFallthroughVariationIfPrerequisiteIsMetAndThereAreNoRules()
{
    struct LDUser *  user;
    struct LDStore * store;
    struct LDJSON *  flag1, *flag2, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    LD_ASSERT(config = LDConfigNew("abc"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    setFallthrough(flag2, 1);
    addVariations2(flag2);

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, flag2));

    /* run */
    LD_ASSERT(LDi_evaluate(
        client,
        flag1,
        user,
        store,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "fall") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);

    LD_ASSERT(events);
    LD_ASSERT(LDCollectionGetSize(events) == 1);
    LD_ASSERT(eventsiter = LDGetIter(events));
    LD_ASSERT(
        strcmp("feature1", LDGetText(LDObjectLookup(eventsiter, "key"))) == 0);
    LD_ASSERT(
        strcmp("go", LDGetText(LDObjectLookup(eventsiter, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "variation")) == 1);
    LD_ASSERT(
        strcmp("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf"))) ==
        0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

static void
testMultipleLevelsOfPrerequisiteProduceMultipleEvents()
{
    struct LDUser *  user;
    struct LDStore * store;
    struct LDJSON *  flag1, *flag2, *flag3, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    LD_ASSERT(config = LDConfigNew("abc"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    addPrerequisite(flag2, "feature2", 1);
    setFallthrough(flag2, 1);
    addVariations2(flag2);

    /* flag3 */
    LD_ASSERT(flag3 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag3, "key", LDNewText("feature2")));
    LD_ASSERT(LDObjectSetKey(flag3, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag3, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag3, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag3, "salt", LDNewText("abc")));
    setFallthrough(flag3, 1);
    addVariations2(flag3);

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, flag2));
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, flag3));

    /* run */
    LD_ASSERT(LDi_evaluate(
        client,
        flag1,
        user,
        store,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "fall") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);

    LD_ASSERT(events);
    LD_ASSERT(LDCollectionGetSize(events) == 2);

    LD_ASSERT(eventsiter = LDGetIter(events));
    LD_ASSERT(
        strcmp("feature2", LDGetText(LDObjectLookup(eventsiter, "key"))) == 0);
    LD_ASSERT(
        strcmp("go", LDGetText(LDObjectLookup(eventsiter, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "variation")) == 1);
    LD_ASSERT(
        strcmp("feature1", LDGetText(LDObjectLookup(eventsiter, "prereqOf"))) ==
        0);

    LD_ASSERT(eventsiter = LDIterNext(eventsiter));
    LD_ASSERT(
        strcmp("feature1", LDGetText(LDObjectLookup(eventsiter, "key"))) == 0);
    LD_ASSERT(
        strcmp("go", LDGetText(LDObjectLookup(eventsiter, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(eventsiter, "variation")) == 1);
    LD_ASSERT(
        strcmp("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf"))) ==
        0);

    LDJSONFree(flag1);
    LDJSONFree(events);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

static void
testFlagMatchesUserFromTarget()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userkey"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 0);
    addVariations1(flag);

    {
        struct LDJSON *targetsets;
        struct LDJSON *targetset;
        struct LDJSON *list;

        LD_ASSERT(list = LDNewArray());
        LD_ASSERT(LDArrayPush(list, LDNewText("whoever")));
        LD_ASSERT(LDArrayPush(list, LDNewText("userkey")));

        LD_ASSERT(targetset = LDNewObject());
        LD_ASSERT(LDObjectSetKey(targetset, "values", list));
        LD_ASSERT(LDObjectSetKey(targetset, "variation", LDNewNumber(2)));

        LD_ASSERT(targetsets = LDNewArray());
        LD_ASSERT(LDArrayPush(targetsets, targetset));
        LD_ASSERT(LDObjectSetKey(flag, "targets", targetsets));
    }

    /* run */
    LD_ASSERT(LDi_evaluate(
        NULL,
        flag,
        user,
        (struct LDStore *)1,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "on") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 2);
    LD_ASSERT(details.reason == LD_TARGET_MATCH);
    LD_ASSERT(!events)

    LDJSONFree(flag);
    LDJSONFree(events);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagMatchesUserFromRules()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *variation, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userkey"));

    /* flag */
    LD_ASSERT(variation = LDNewObject());
    LD_ASSERT(LDObjectSetKey(variation, "variation", LDNewNumber(2)))
    flag = makeFlagToMatchUser("userkey", variation);

    /* run */
    LD_ASSERT(LDi_evaluate(
        NULL,
        flag,
        user,
        (struct LDStore *)1,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "on") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 2);
    LD_ASSERT(details.reason == LD_RULE_MATCH);
    LD_ASSERT(details.extra.rule.ruleIndex == 0);
    LD_ASSERT(strcmp(details.extra.rule.id, "rule-id") == 0);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(events);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseCanMatchBuiltInAttribute()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(LDUserSetName(user, "Bob"));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewText("Bob")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("name")));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(LDi_evaluate(
        NULL,
        flag,
        user,
        (struct LDStore *)1,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanTrue);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseCanMatchCustomAttribute()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *clause, *values, *custom, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(custom = LDNewObject());
    LD_ASSERT(LDObjectSetKey(custom, "legs", LDNewNumber(4)));
    LDUserSetCustom(user, custom);

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewNumber(4)));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("legs")));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(LDi_evaluate(
        NULL,
        flag,
        user,
        (struct LDStore *)1,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanTrue);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseReturnsFalseForMissingAttribute()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(LDUserSetName(user, "Bob"));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewNumber(4)));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("legs")));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(LDi_evaluate(
        NULL,
        flag,
        user,
        (struct LDStore *)1,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanFalse);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseCanBeNegated()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(LDUserSetName(user, "Bob"));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewText("Bob")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("name")));
    LD_ASSERT(LDObjectSetKey(clause, "negate", LDNewBool(LDBooleanTrue)));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(LDi_evaluate(
        NULL,
        flag,
        user,
        (struct LDStore *)1,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanFalse);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseForMissingAttributeIsFalseEvenIfNegate()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(LDUserSetName(user, "Bob"));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewNumber(4)));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("legs")));
    LD_ASSERT(LDObjectSetKey(clause, "negate", LDNewBool(LDBooleanTrue)));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(LDi_evaluate(
        NULL,
        flag,
        user,
        (struct LDStore *)1,
        &details,
        &events,
        &result,
        LDBooleanFalse));

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanFalse);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseWithUnknownOperatorDoesNotMatch()
{
    struct LDUser *  user;
    struct LDJSON *  flag, *result, *clause, *values, *events;
    struct LDDetails details;

    result = NULL;
    events = NULL;
    LDDetailsInit(&details);

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(LDUserSetName(user, "Bob"));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewText("Bob")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("unsupported")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("name")));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *)1,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanFalse);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testSegmentMatchClauseRetrievesSegmentFromStore()
{
    struct LDUser * user;
    struct LDStore *store;
    struct LDJSON * segment, *flag, *result, *included, *values, *clause,
        *events;
    struct LDDetails details;

    result = NULL;
    events = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("foo"));

    /* segment */
    LD_ASSERT(included = LDNewArray());
    LD_ASSERT(LDArrayPush(included, LDNewText("foo")));

    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("segkey")));
    LD_ASSERT(LDObjectSetKey(segment, "included", included));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(3)));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewText("segkey")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("")));
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("segmentMatch")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, segment));

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            store,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanTrue);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testSegmentMatchClauseFallsThroughIfSegmentNotFound()
{
    struct LDUser *  user;
    struct LDStore * store;
    struct LDJSON *  flag, *result, *values, *clause, *events;
    struct LDDetails details;

    result = NULL;
    events = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("foo"));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewText("segkey")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("")));
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("segmentMatch")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* store */
    LD_ASSERT(store = prepareEmptyStore());

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            store,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanFalse);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testCanMatchJustOneSegmentFromList()
{
    struct LDStore *store;
    struct LDUser * user;
    struct LDJSON * segment, *flag, *result, *included, *values, *clause,
        *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("foo"));

    /* segment */
    LD_ASSERT(included = LDNewArray());
    LD_ASSERT(LDArrayPush(included, LDNewText("foo")));

    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("segkey")));
    LD_ASSERT(LDObjectSetKey(segment, "included", included));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(3)));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayPush(values, LDNewText("unknownsegkey")));
    LD_ASSERT(LDArrayPush(values, LDNewText("segkey")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("")));
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("segmentMatch")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, segment));

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            store,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == LDBooleanTrue);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static LDBoolean
floateq(const float left, const float right)
{
    return fabs(left - right) < FLT_EPSILON;
}

static void
testBucketUser()
{
    float          bucket;
    struct LDUser *user;

    LD_ASSERT(user = LDUserNew("userKeyA"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    LD_ASSERT(floateq(0.42157587, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyB"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    LD_ASSERT(floateq(0.6708485, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyC"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    LD_ASSERT(floateq(0.10343106, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyC"));
    LD_ASSERT(
        !LDi_bucketUser(user, "hashKey", "unknown", "saltyA", NULL, &bucket));
    LD_ASSERT(floateq(0.0, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("primaryKey"));
    LD_ASSERT(LDUserSetSecondary(user, "secondaryKey"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    LD_ASSERT(floateq(0.100876, bucket));
    LDUserFree(user);
}

static void
testBucketUserWithSeed()
{
    float          bucket;
    struct LDUser *user;
    int            seed;

    seed = 61;

    LD_ASSERT(user = LDUserNew("userKeyA"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    LD_ASSERT(floateq(0.09801207, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyB"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    LD_ASSERT(floateq(0.14483777, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyC"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    LD_ASSERT(floateq(0.9242641, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("primaryKey"));
    LD_ASSERT(LDUserSetSecondary(user, "secondaryKey"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    LD_ASSERT(floateq(0.0742077678, bucket));
    LDUserFree(user);
}

static void
testInExperimentExplanation()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *events, *fallthrough, *rollout, *variations,
        *variation;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    addVariations1(flag);

    LD_ASSERT(fallthrough = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", fallthrough));
    LD_ASSERT(rollout = LDNewObject());
    LD_ASSERT(LDObjectSetKey(fallthrough, "rollout", rollout));
    LD_ASSERT(LDObjectSetKey(rollout, "kind", LDNewText("experiment")));
    LD_ASSERT(variations = LDNewArray());
    LD_ASSERT(LDObjectSetKey(rollout, "variations", variations));
    LD_ASSERT(variation = LDNewObject());
    LD_ASSERT(LDArrayPush(variations, variation));
    LD_ASSERT(LDObjectSetKey(variation, "weight", LDNewNumber(100000)));
    LD_ASSERT(LDObjectSetKey(variation, "variation", LDNewNumber(0)));

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *)1,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validation */
    LD_ASSERT(strcmp("fall", LDGetText(result)) == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.extra.fallthrough.inExperiment);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testNotInExperimentExplanation()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *events, *fallthrough, *rollout, *variations,
        *variation;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    addVariations1(flag);

    LD_ASSERT(fallthrough = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", fallthrough));
    LD_ASSERT(rollout = LDNewObject());
    LD_ASSERT(LDObjectSetKey(fallthrough, "rollout", rollout));
    LD_ASSERT(LDObjectSetKey(rollout, "kind", LDNewText("experiment")));
    LD_ASSERT(variations = LDNewArray());
    LD_ASSERT(LDObjectSetKey(rollout, "variations", variations));
    LD_ASSERT(variation = LDNewObject());
    LD_ASSERT(LDArrayPush(variations, variation));
    LD_ASSERT(LDObjectSetKey(variation, "weight", LDNewNumber(100000)));
    LD_ASSERT(LDObjectSetKey(variation, "untracked", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(variation, "variation", LDNewNumber(0)));

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *)1,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validation */
    LD_ASSERT(strcmp("fall", LDGetText(result)) == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.extra.fallthrough.inExperiment == LDBooleanFalse);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testRolloutCustomSeed()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *events, *fallthrough, *rollout, *variations,
        *variation;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    addVariations1(flag);

    LD_ASSERT(fallthrough = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", fallthrough));
    LD_ASSERT(rollout = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rollout, "seed", LDNewNumber(50)));
    LD_ASSERT(LDObjectSetKey(fallthrough, "rollout", rollout));
    LD_ASSERT(LDObjectSetKey(rollout, "kind", LDNewText("experiment")));
    LD_ASSERT(variations = LDNewArray());
    LD_ASSERT(LDObjectSetKey(rollout, "variations", variations));
    LD_ASSERT(variation = LDNewObject());
    LD_ASSERT(LDArrayPush(variations, variation));
    LD_ASSERT(LDObjectSetKey(variation, "weight", LDNewNumber(100000)));
    LD_ASSERT(LDObjectSetKey(variation, "untracked", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(variation, "variation", LDNewNumber(0)));

    /* run */
    LD_ASSERT(
        LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *)1,
            &details,
            &events,
            &result,
            LDBooleanFalse) == EVAL_MATCH);

    /* validation */
    LD_ASSERT(strcmp("fall", LDGetText(result)) == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.extra.fallthrough.inExperiment == LDBooleanFalse);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.reason == LD_FALLTHROUGH);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    returnsOffVariationIfFlagIsOff();
    testFlagReturnsNilIfFlagIsOffAndOffVariationIsUnspecified();
    testFlagReturnsFallthroughIfFlagIsOnAndThereAreNoRules();
    testFlagReturnsOffVariationIfPrerequisiteIsOff();
    testFlagReturnsOffVariationIfPrerequisiteIsNotMet();
    testFlagReturnsFallthroughVariationIfPrerequisiteIsMetAndThereAreNoRules();
    testMultipleLevelsOfPrerequisiteProduceMultipleEvents();
    testFlagMatchesUserFromTarget();
    testFlagMatchesUserFromRules();
    testClauseCanMatchBuiltInAttribute();
    testClauseCanMatchCustomAttribute();
    testClauseReturnsFalseForMissingAttribute();
    testClauseCanBeNegated();
    testClauseForMissingAttributeIsFalseEvenIfNegate();
    testClauseWithUnknownOperatorDoesNotMatch();
    testSegmentMatchClauseRetrievesSegmentFromStore();
    testSegmentMatchClauseFallsThroughIfSegmentNotFound();
    testCanMatchJustOneSegmentFromList();
    testBucketUser();
    testBucketUserWithSeed();
    testInExperimentExplanation();
    testNotInExperimentExplanation();
    testRolloutCustomSeed();

    LDBasicLoggerThreadSafeShutdown();
    testFlagReturnsErrorForFallthroughWithNoVariationAndNoRollout();

    return 0;
}
