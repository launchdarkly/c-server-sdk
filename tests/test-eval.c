#include "ldapi.h"
#include "ldinternal.h"
#include "lduser.h"
#include "ldevaluate.h"
#include "ldstore.h"
#include "ldvariations.h"

#include <math.h>
#include <float.h>

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore *store;
    struct LDJSON *sets, *tmp;

    LD_ASSERT(store = LDMakeInMemoryStore());
    LD_ASSERT(!LDStoreInitialized(store));

    LD_ASSERT(sets = LDNewObject());

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(sets, "segments", tmp));

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(sets, "flags", tmp));

    LD_ASSERT(LDStoreInit(store, sets));
    LD_ASSERT(LDStoreInitialized(store));

    return store;
}

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

    LD_ASSERT(LDArrayPush(variations, variation));
}

static void
addPrerequisite(struct LDJSON *const flag, const char *const key,
    const unsigned int variation)
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

static void
addVariations1(struct LDJSON *const flag)
{
    addVariation(flag, LDNewText("fall"));
    addVariation(flag, LDNewText("off"));
    addVariation(flag, LDNewText("on"));
}

static void
addVariations2(struct LDJSON *const flag)
{
    addVariation(flag, LDNewText("nogo"));
    addVariation(flag, LDNewText("go"));
}

static struct LDJSON *
makeFlagToMatchUser(const char *const key,
    struct LDJSON *const variationOrRollout)
{
    struct LDJSON *flag, *clause, *tmp, *rule;

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("key")));
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayPush(tmp, LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(clause, "values", tmp));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "id", LDNewText("rule-id")));
    LD_ASSERT(LDObjectMerge(rule, variationOrRollout));
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayPush(tmp, clause));
    LD_ASSERT(LDObjectSetKey(rule, "clauses", tmp));

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    addVariations1(flag);
    setFallthrough(flag, 0);
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayPush(tmp, rule));
    LD_ASSERT(LDObjectSetKey(flag, "rules", tmp));

    LDJSONFree(variationOrRollout);

    return flag;
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
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", rules));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewBool(false));
    addVariation(flag, LDNewBool(true));

    return flag;
}

static void
returnsOffVariationIfFlagIsOff()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));
    addVariations1(flag);
    setFallthrough(flag, 0);

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result) == EVAL_MISS);

    /* validation */
    LD_ASSERT(strcmp("off", LDGetText(result)) == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 1);
    LD_ASSERT(details.kind == LD_OFF);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagReturnsNilIfFlagIsOffAndOffVariationIsUnspecified()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));
    setFallthrough(flag, 0);
    addVariations1(flag);

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result) == EVAL_MISS);

    /* validation */
    LD_ASSERT(!result);
    LD_ASSERT(!details.hasVariation);
    LD_ASSERT(details.kind == LD_OFF);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagReturnsFallthroughIfFlagIsOnAndThereAreNoRules()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", LDNewArray()));
    setFallthrough(flag, 0);
    addVariations1(flag);

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result) == EVAL_MATCH);

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "fall") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.kind == LD_FALLTHROUGH);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagReturnsOffVariationIfPrerequisiteIsOff()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(false)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    addVariations2(flag2);

    /* store setup */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, "flags", flag2));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag1, user, store, &details, &events,
        &result));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "off") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 1);
    LD_ASSERT(details.kind == LD_PREREQUISITE_FAILED);
    LD_ASSERT(strcmp("feature1", details.extra.prerequisiteKey) == 0)

    LD_ASSERT(events);
    LD_ASSERT(LDCollectionGetSize(events) == 1);
    LD_ASSERT(events = LDGetIter(events));
    LD_ASSERT(strcmp("feature1",
        LDGetText(LDObjectLookup(events, "key"))) == 0);
    LD_ASSERT(strcmp("go",
        LDGetText(LDObjectLookup(events, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "variation")) == 1);
    LD_ASSERT(strcmp("feature0",
        LDGetText(LDObjectLookup(events, "prereqOf"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagReturnsOffVariationIfPrerequisiteIsNotMet()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(2)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    addVariations2(flag2);
    setFallthrough(flag2, 0);

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, "flags", flag2));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag1, user, store, &details, &events,
        &result));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "off") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 1);
    LD_ASSERT(details.kind == LD_PREREQUISITE_FAILED);

    LD_ASSERT(events);
    LD_ASSERT(LDCollectionGetSize(events) == 1);
    LD_ASSERT(events = LDGetIter(events));
    LD_ASSERT(strcmp("feature1",
        LDGetText(LDObjectLookup(events, "key"))) == 0);
    LD_ASSERT(strcmp("nogo",
        LDGetText(LDObjectLookup(events, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "version")) == 2);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "variation")) == 0);
    LD_ASSERT(strcmp("feature0",
        LDGetText(LDObjectLookup(events, "prereqOf"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagReturnsFallthroughVariationIfPrerequisiteIsMetAndThereAreNoRules()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    setFallthrough(flag2, 1);
    addVariations2(flag2);

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, "flags", flag2));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag1, user, store, &details, &events,
        &result));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "fall") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.kind == LD_FALLTHROUGH);

    LD_ASSERT(events);
    LD_ASSERT(LDCollectionGetSize(events) == 1);
    LD_ASSERT(events = LDGetIter(events));
    LD_ASSERT(strcmp("feature1",
        LDGetText(LDObjectLookup(events, "key"))) == 0);
    LD_ASSERT(strcmp("go",
        LDGetText(LDObjectLookup(events, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "variation")) == 1);
    LD_ASSERT(strcmp("feature0",
        LDGetText(LDObjectLookup(events, "prereqOf"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testMultipleLevelsOfPrerequisiteProduceMultipleEvents()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *flag3, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag1 */
    LD_ASSERT(flag1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    addPrerequisite(flag2, "feature2", 1);
    setFallthrough(flag2, 1);
    addVariations2(flag2);

    /* flag3 */
    LD_ASSERT(flag3 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag3, "key", LDNewText("feature2")));
    LD_ASSERT(LDObjectSetKey(flag3, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag3, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag3, "offVariation", LDNewNumber(1)));
    setFallthrough(flag3, 1);
    addVariations2(flag3);

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, "flags", flag2));
    LD_ASSERT(LDStoreUpsert(store, "flags", flag3));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag1, user, store, &details, &events,
        &result));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "fall") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 0);
    LD_ASSERT(details.kind == LD_FALLTHROUGH);

    LD_ASSERT(events);;
    LD_ASSERT(LDCollectionGetSize(events) == 2);

    LD_ASSERT(events = LDGetIter(events));
    LD_ASSERT(strcmp("feature2",
        LDGetText(LDObjectLookup(events, "key"))) == 0);
    LD_ASSERT(strcmp("go",
        LDGetText(LDObjectLookup(events, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "variation")) == 1);
    LD_ASSERT(strcmp("feature1",
        LDGetText(LDObjectLookup(events, "prereqOf"))) == 0);

    LD_ASSERT(events = LDIterNext(events));
    LD_ASSERT(strcmp("feature1",
        LDGetText(LDObjectLookup(events, "key"))) == 0);
    LD_ASSERT(strcmp("go",
        LDGetText(LDObjectLookup(events, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "version")) == 3);
    LD_ASSERT(LDGetNumber(LDObjectLookup(events, "variation")) == 1);
    LD_ASSERT(strcmp("feature0",
        LDGetText(LDObjectLookup(events, "prereqOf"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(events);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testFlagMatchesUserFromTarget()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    LD_ASSERT(user = LDUserNew("userkey"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
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
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "on") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 2);
    LD_ASSERT(details.kind == LD_TARGET_MATCH);
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
    struct LDUser *user;
    struct LDJSON *flag, *result, *variation, *events;
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
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(result), "on") == 0);
    LD_ASSERT(details.hasVariation);
    LD_ASSERT(details.variationIndex == 2);
    LD_ASSERT(details.kind == LD_RULE_MATCH);
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
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
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
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result));

    /* validate */
    LD_ASSERT(LDGetBool(result) == true);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseCanMatchCustomAttribute()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *custom, *events;
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
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result));

    /* validate */
    LD_ASSERT(LDGetBool(result) == true);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseReturnsFalseForMissingAttribute()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
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
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result));

    /* validate */
    LD_ASSERT(LDGetBool(result) == false);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseCanBeNegated()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
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
    LD_ASSERT(LDObjectSetKey(clause, "negate", LDNewBool(true)));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result));

    /* validate */
    LD_ASSERT(LDGetBool(result) == false);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseForMissingAttributeIsFalseEvenIfNegate()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
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
    LD_ASSERT(LDObjectSetKey(clause, "negate", LDNewBool(true)));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result));

    /* validate */
    LD_ASSERT(LDGetBool(result) == false);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testClauseWithUnknownOperatorDoesNotMatch()
{
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
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
    LD_ASSERT(LDi_evaluate(NULL, flag, user, (struct LDStore *)1, &details,
        &events, &result) == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == false);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static void
testSegmentMatchClauseRetrievesSegmentFromStore()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *segment, *flag, *result, *included, *values, *clause,
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
    LD_ASSERT(LDStoreUpsert(store, "segments", segment));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag, user, store, &details, &events, &result)
        == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == true);
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
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag, *result, *values, *clause, *events;
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
    LD_ASSERT(LDi_evaluate(NULL, flag, user, store, &details, &events, &result)
        == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == false);
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
    struct LDUser *user;
    struct LDJSON *segment, *flag, *result, *included, *values, *clause,
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
    LD_ASSERT(LDStoreUpsert(store, "segments", segment));

    /* run */
    LD_ASSERT(LDi_evaluate(NULL, flag, user, store, &details, &events, &result)
        == EVAL_MATCH);

    /* validate */
    LD_ASSERT(LDGetBool(result) == true);
    LD_ASSERT(!events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static bool
floateq(const float left, const float right)
{
    return fabs(left - right) < FLT_EPSILON;
}

static void
testLDi_bucketUserByKey()
{
    float bucket;
    struct LDUser *user;

    LD_ASSERT(user = LDUserNew("userKeyA"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.42157587, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyB"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.6708485, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyC"));
    LD_ASSERT(LDi_bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.10343106, bucket));
    LDUserFree(user);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
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

    testLDi_bucketUserByKey();

    return 0;
}
