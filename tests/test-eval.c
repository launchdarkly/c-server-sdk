#include "ldapi.h"
#include "ldinternal.h"
#include "lduser.h"
#include "ldevaluate.h"

#include <math.h>
#include <float.h>

static struct LDJSON *
testFlag1()
{
    struct LDJSON *flag = NULL;
    struct LDJSON *tmp = NULL;

    LD_ASSERT(flag = LDNewObject());

    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));

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

    LD_ASSERT(user = LDUserNew("userKeyA"));

    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    LD_ASSERT(strcmp("off", LDGetText(LDObjectLookup(result, "value"))) == 0);
    LD_ASSERT(LDGetNumber(LDObjectLookup(result, "variationIndex")) == 1);

    LD_ASSERT(strcmp("isOff", LDGetText(
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

    LD_ASSERT(user = LDUserNew("userKeyA"));

    LD_ASSERT(evaluate(flag, user, (struct LDStore *)1, &result));

    LD_ASSERT(LDJSONGetType(LDObjectLookup(result, "value")) == LDNull);

    LD_ASSERT(LDJSONGetType(
        LDObjectLookup(result, "variationIndex")) == LDNull);

    LD_ASSERT(strcmp("isOff", LDGetText(
        LDObjectLookup(LDObjectLookup(result, "reason"), "kind"))) == 0);

    LDJSONFree(flag);
    LDJSONFree(result);
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

    testBucketUserByKey();

    return 0;
}
