#include "ldapi.h"
#include "ldinternal.h"
#include "lduser.h"
#include "ldevaluate.h"

#include <math.h>
#include <float.h>

static void
returnsOffVariationIfFlagIsOff()
{
    struct LDJSON *flag = NULL, *tmp = NULL;

    LD_ASSERT(flag = LDNewObject());

    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("flagkey")));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(false)));
    LD_ASSERT(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(0)));
    LD_ASSERT(LDObjectSetKey(flag, "fallthrough", tmp));

    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("a")));
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("b")));
    LD_ASSERT(LDObjectSetKey(flag, "variations", tmp));
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

    LD_ASSERT(user = LDUserNew("userKeyB"));
    LD_ASSERT(bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.6708485, bucket));

    LD_ASSERT(user = LDUserNew("userKeyC"));
    LD_ASSERT(bucketUser(user, "hashKey", "key", "saltyA", &bucket))
    LD_ASSERT(floateq(0.10343106, bucket));
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    returnsOffVariationIfFlagIsOff();
    testBucketUserByKey();

    return 0;
}
