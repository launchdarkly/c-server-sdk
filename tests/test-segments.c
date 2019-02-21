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

static void
testMatchingRuleWithFullRollout()
{
    struct LDUser *user;
    struct LDJSON *segment;
    struct LDJSON *rules;
    struct LDJSON *rule;
    struct LDJSON *clauses;
    struct LDJSON *clause;
    struct LDJSON *values;

    /* user */
    LD_ASSERT(user = LDUserNew("foo"));
    LD_ASSERT(LDUserSetEmail(user, "test@example.com"));

    /* segment */
    LD_ASSERT(values = LDNewArray());
    LD_ASSERT(LDArrayAppend(values, LDNewText("test@example.com")));

    LD_ASSERT(clause = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause, "attribute", LDNewText("email")));
    LD_ASSERT(LDObjectSetKey(clause, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause, "values", values));
    LD_ASSERT(LDObjectSetKey(clause, "negate", LDNewBool(false)));

    LD_ASSERT(clauses = LDNewArray());
    LD_ASSERT(LDArrayAppend(clauses, clause));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "clauses", clauses));
    LD_ASSERT(LDObjectSetKey(rule, "weight", LDNewNumber(100000)));

    LD_ASSERT(rules = LDNewArray());
    LD_ASSERT(LDArrayAppend(rules, rule));

    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("test")));
    LD_ASSERT(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    LD_ASSERT(LDObjectSetKey(segment, "rules", rules));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", LDNewBool(false)));

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
    testMatchingRuleWithFullRollout();

    return 0;
}
