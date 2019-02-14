#include "ldapi.h"
#include "ldinternal.h"
#include "lduser.h"
#include "ldevaluate.h"
#include "ldstore.h"

#include <math.h>
#include <float.h>

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore *store;
    struct LDJSON *sets;
    struct LDJSON *tmp;

    LD_ASSERT(store = makeInMemoryStore());
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

    LD_ASSERT(LDArrayAppend(variations, variation));
}

static void
addPrerequisite(struct LDJSON *const flag, const char *const key,
    const unsigned int variation)
{
    struct LDJSON *tmp;
    struct LDJSON *prerequisites;

    if (!(prerequisites = LDObjectLookup(flag, "prerequisites"))) {
        LD_ASSERT(prerequisites = LDNewArray());
        LDObjectSetKey(flag, "prerequisites", prerequisites);
    }

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(tmp, "key", LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(variation)));

    LD_ASSERT(LDArrayAppend(prerequisites, tmp));
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
    struct LDJSON *flag;
    struct LDJSON *clause;
    struct LDJSON *tmp;
    struct LDJSON *rule;

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("key")));
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(clause, "values", tmp));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "id", LDNewText("rule-id")));
    LD_ASSERT(LDObjectMerge(rule, variationOrRollout));
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, clause));
    LD_ASSERT(LDObjectSetKey(rule, "clauses", tmp));

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    addVariations1(flag);
    setFallthrough(flag, 0);
    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, rule));
    LD_ASSERT(LDObjectSetKey(flag, "rules", tmp));

    LDJSONFree(variationOrRollout);

    return flag;
}

static struct LDJSON *
booleanFlagWithClause(struct LDJSON *const clause)
{
    struct LDJSON *flag;
    struct LDJSON *rule;
    struct LDJSON *clauses;
    struct LDJSON *rules;

    LD_ASSERT(clauses = LDNewArray());
    LD_ASSERT(LDArrayAppend(clauses, clause));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "clauses", clauses));
    LD_ASSERT(LDObjectSetKey(rule, "variation", LDNewNumber(1)));

    LD_ASSERT(rules = LDNewArray());
    LD_ASSERT(LDArrayAppend(rules, rule));

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

    struct LDJSON *flag = NULL;
    struct LDJSON *result = NULL;

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));
    addVariations1(flag);
    setFallthrough(flag, 0);

    /* run */
    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    /* validation */
    LD_ASSERT(strcmp("off", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 1);
    LD_ASSERT(strcmp("OFF", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
}

static void
testFlagReturnsNilIfFlagIsOffAndOffVariationIsUnspecified()
{
    struct LDUser *user;

    struct LDJSON *flag = NULL;
    struct LDJSON *result = NULL;

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));
    setFallthrough(flag, 0);
    addVariations1(flag);

    /* run */
    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    /* validation */
    LD_ASSERT(LDJSONGetType(LDObjectLookup(result, "value")) == LDNull);
    LD_ASSERT(LDJSONGetType(
        LDObjectLookup(result, "variationIndex")) == LDNull);
    LD_ASSERT(strcmp("OFF", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
}

static void
testFlagReturnsFallthroughIfFlagIsOnAndThereAreNoRules()
{
    struct LDUser *user;

    struct LDJSON *flag = NULL;
    struct LDJSON *result = NULL;

    LD_ASSERT(user = LDUserNew("userKeyA"));

    /* flag */
    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", LDNewArray()));
    setFallthrough(flag, 0);
    addVariations1(flag);

    /* run */
    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    /* validate */
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(result, "value")), "fall") == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 0);
    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
}

static void
testFlagReturnsOffVariationIfPrerequisiteIsOff()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1;
    struct LDJSON *flag2;
    struct LDJSON *result;

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
    LD_ASSERT(evaluate(flag1, user, store, &result));

    /* validate */
    LD_ASSERT(strcmp("off", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 1);
    LD_ASSERT(strcmp("PREREQUISITE_FAILED", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
}

static void
testFlagReturnsOffVariationIfPrerequisiteIsNotMet()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1;
    struct LDJSON *flag2;
    struct LDJSON *result;

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
    addVariations2(flag2);
    setFallthrough(flag2, 0);

    /* store */
    LD_ASSERT(store = prepareEmptyStore());
    LD_ASSERT(LDStoreUpsert(store, "flags", flag2));

    /* run */
    LD_ASSERT(evaluate(flag1, user, store, &result));

    /* validate */
    LD_ASSERT(strcmp("off", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 1);
    LD_ASSERT(strcmp("PREREQUISITE_FAILED", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
}

static void
testFlagReturnsFallthroughVariationIfPrerequisiteIsMetAndThereAreNoRules()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1;
    struct LDJSON *flag2;
    struct LDJSON *result;

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
    LD_ASSERT(evaluate(flag1, user, store, &result));

    /* validate */
    LD_ASSERT(strcmp("fall", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 0);
    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
}

static void
testFlagMatchesUserFromTarget()
{
    struct LDUser *user;
    struct LDJSON *flag;
    struct LDJSON *result;

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
        LD_ASSERT(LDArrayAppend(list, LDNewText("whoever")));
        LD_ASSERT(LDArrayAppend(list, LDNewText("userkey")));

        LD_ASSERT(targetset = LDNewObject());
        LD_ASSERT(LDObjectSetKey(targetset, "values", list));
        LD_ASSERT(LDObjectSetKey(targetset, "variation", LDNewNumber(2)));

        LD_ASSERT(targetsets = LDNewArray());
        LD_ASSERT(LDArrayAppend(targetsets, targetset));
        LD_ASSERT(LDObjectSetKey(flag, "targets", targetsets));
    }

    /* run */
    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    /* validate */
    LD_ASSERT(strcmp("on", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 2);
    LD_ASSERT(strcmp("TARGET_MATCH", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
}

static void
testFlagMatchesUserFromRules()
{
    struct LDUser *user;
    struct LDJSON *flag;
    struct LDJSON *result;
    struct LDJSON *variation;

    LD_ASSERT(user = LDUserNew("userkey"));

    /* flag */
    LD_ASSERT(variation = LDNewObject());
    LD_ASSERT(LDObjectSetKey(variation, "variation", LDNewNumber(2)))
    flag = makeFlagToMatchUser("userkey", variation);

    /* run */
    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    /* validate */
    LD_ASSERT(strcmp("on", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 2);
    LD_ASSERT(strcmp("RULE_MATCH", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
}

void
testClauseCanMatchBuiltInAttribute()
{
    struct LDJSON *result;
    struct LDUser *user;
    struct LDJSON *flag;
    struct LDJSON *clause;
    struct LDJSON *values;

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(LDUserSetName(user, "Bob"));

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayAppend(values, LDNewText("Bob")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("name")));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    /* validate */
    LD_ASSERT(LDGetBool(LDObjectLookup(result, "value")) == true);

    LDJSONFree(flag);
    LDUserFree(user);
}

void
testClauseCanMatchCustomAttribute()
{
    struct LDJSON *result;
    struct LDUser *user;
    struct LDJSON *flag;
    struct LDJSON *clause;
    struct LDJSON *values;
    struct LDJSON *custom;

    /* user */
    LD_ASSERT(user = LDUserNew("key"));
    LD_ASSERT(custom = LDNewObject());
    LD_ASSERT(LDObjectSetKey(custom, "legs", LDNewNumber(4)));
    LDUserSetCustom(user, custom);

    /* flag */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayAppend(values, LDNewNumber(4)));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("legs")));

    LD_ASSERT(flag = booleanFlagWithClause(clause));

    /* run */
    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    /* validate */
    LD_ASSERT(LDGetBool(LDObjectLookup(result, "value")) == true);

    LDJSONFree(flag);
    LDUserFree(user);
}

static bool
floateq(const float left, const float right)
{
    return fabs(left - right) < FLT_EPSILON;
}

static void
testBucketUserByKey()
{
    float bucket;
    struct LDUser *user;

    LD_ASSERT(user = LDUserNew("userKeyA"));
    LD_ASSERT(bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.42157587, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyB"));
    LD_ASSERT(bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.6708485, bucket));
    LDUserFree(user);

    LD_ASSERT(user = LDUserNew("userKeyC"));
    LD_ASSERT(bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.10343106, bucket));
    LDUserFree(user);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    returnsOffVariationIfFlagIsOff();
    testFlagReturnsNilIfFlagIsOffAndOffVariationIsUnspecified();
    testFlagReturnsFallthroughIfFlagIsOnAndThereAreNoRules();
    testFlagReturnsOffVariationIfPrerequisiteIsOff();
    testFlagReturnsOffVariationIfPrerequisiteIsNotMet();
    testFlagReturnsFallthroughVariationIfPrerequisiteIsMetAndThereAreNoRules();
    testFlagMatchesUserFromTarget();
    testFlagMatchesUserFromRules();
    testClauseCanMatchBuiltInAttribute();
    testClauseCanMatchCustomAttribute();

    testBucketUserByKey();

    return 0;
}
