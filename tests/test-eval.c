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

static struct LDJSON *
testFlag1()
{
    struct LDJSON *flag;
    struct LDJSON *tmp;

    LD_ASSERT(flag = LDNewObject());

    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature0")));

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(0)));
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", tmp));

    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("fall")));
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("off")));
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("on")));
    LD_ASSERT(LDObjectSetKey(flag, "variations", tmp));

    return flag;
}

static void
returnsOffVariationIfFlagIsOff()
{
    struct LDUser *user;

    struct LDJSON *flag = NULL;
    struct LDJSON *result = NULL;

    LD_ASSERT(flag = testFlag1());
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));

    LD_ASSERT(user = LDUserNew("userKeyA"));

    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

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

    LD_ASSERT(flag = testFlag1());
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));

    LD_ASSERT(user = LDUserNew("userKeyA"));

    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

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

    LD_ASSERT(flag = testFlag1());
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", LDNewArray()));

    LD_ASSERT(user = LDUserNew("userKeyA"));

    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(result, "value")), "fall") == 0);

    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 0);

    LD_ASSERT(strcmp("FALLTHROUGH", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
}

static void
testFlagReturnsOffVariationAndEventIfPrerequisiteIsOff()
{
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1;
    struct LDJSON *flag2;
    struct LDJSON *result = NULL;
    struct LDJSON *tmpcollection;
    struct LDJSON *tmp;

    LD_ASSERT(user = LDUserNew("userKeyA"));
    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(flag1 = testFlag1());
    LD_ASSERT(LDObjectSetKey(flag1, "on", LDNewBool(true)));
    LD_ASSERT(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));

    {
        LD_ASSERT(tmpcollection = LDNewArray());

        LD_ASSERT(tmp = LDNewObject());
        LD_ASSERT(LDObjectSetKey(tmp, "key", LDNewText("feature1")));
        LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(1)));
        LD_ASSERT(LDArrayAppend(tmpcollection, tmp));

        LD_ASSERT(LDObjectSetKey(flag1, "prerequisites", tmpcollection));
    }

    LD_ASSERT(flag2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    LD_ASSERT(LDObjectSetKey(flag2, "on", LDNewBool(false)));
    LD_ASSERT(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    LD_ASSERT(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));

    {
        LD_ASSERT(tmp = LDNewArray());
        LD_ASSERT(LDArrayAppend(tmp, LDNewText("nogo")));
        LD_ASSERT(LDArrayAppend(tmp, LDNewText("go")));
        LD_ASSERT(LDObjectSetKey(flag2, "variations", tmp));
    }

    LD_ASSERT(LDStoreUpsert(store, "flags", flag2));

    LD_ASSERT(evaluate(flag1, user, store, &result));

    LD_ASSERT(strcmp("off", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 1);

    LD_ASSERT(strcmp("PREREQUISITE_FAILED", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDStoreDestroy(store);
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
    testFlagReturnsOffVariationAndEventIfPrerequisiteIsOff();

    testBucketUserByKey();

    return 0;
}
