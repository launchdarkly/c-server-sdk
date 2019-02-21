#include "ldjson.h"
#include "ldoperators.h"
#include "ldinternal.h"
#include "ldevaluate.h"

static void
testExplicitIncludeUser()
{
    struct LDUser *user;
    struct LDJSON *segment;
    struct LDJSON *tmp;

    /* user */
    LD_ASSERT(user = LDUserNew("foo"));

    /* segment */
    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("test")));
    LD_ASSERT(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", LDNewBool(false)));

    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("foo")));
    LD_ASSERT(LDObjectSetKey(segment, "included", tmp));

    /* run */
    LD_ASSERT(segmentMatchesUser(segment, user) == EVAL_MATCH);

    LDUserFree(user);
}

static void
testExplicitExcludeUser()
{
    struct LDUser *user;
    struct LDJSON *segment;
    struct LDJSON *tmp;

    /* user */
    LD_ASSERT(user = LDUserNew("foo"));

    /* segment */
    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("test")));
    LD_ASSERT(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", LDNewBool(false)));

    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("foo")));
    LD_ASSERT(LDObjectSetKey(segment, "excluded", tmp));

    /* run */
    LD_ASSERT(segmentMatchesUser(segment, user) == EVAL_MISS);

    LDUserFree(user);
}

static void
testExplicitIncludeHasPrecedence()
{
    struct LDUser *user;
    struct LDJSON *segment;
    struct LDJSON *tmp;

    /* user */
    LD_ASSERT(user = LDUserNew("foo"));

    /* segment */
    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("test")));
    LD_ASSERT(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", LDNewBool(false)));

    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("foo")));
    LD_ASSERT(LDObjectSetKey(segment, "excluded", tmp));

    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDArrayAppend(tmp, LDNewText("foo")));
    LD_ASSERT(LDObjectSetKey(segment, "included", tmp));

    /* run */
    LD_ASSERT(segmentMatchesUser(segment, user) == EVAL_MATCH);

    LDUserFree(user);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    testExplicitIncludeUser();
    testExplicitExcludeUser();
    testExplicitIncludeHasPrecedence();

    return 0;
}
