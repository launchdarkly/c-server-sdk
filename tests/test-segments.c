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

static void
testMatchingRuleWithZeroRollout()
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
    LD_ASSERT(LDObjectSetKey(rule, "weight", LDNewNumber(0)));

    LD_ASSERT(rules = LDNewArray());
    LD_ASSERT(LDArrayAppend(rules, rule));

    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("test")));
    LD_ASSERT(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    LD_ASSERT(LDObjectSetKey(segment, "rules", rules));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", LDNewBool(false)));

    /* run */
    LD_ASSERT(segmentMatchesUser(segment, user) == EVAL_MISS);

    LDUserFree(user);
}

static void
testMatchingRuleWithMultipleClauses()
{
    struct LDUser *user;
    struct LDJSON *segment;
    struct LDJSON *rules;
    struct LDJSON *rule;
    struct LDJSON *clauses;
    struct LDJSON *clause1;
    struct LDJSON *clause2;
    struct LDJSON *values1;
    struct LDJSON *values2;

    /* user */
    LD_ASSERT(user = LDUserNew("foo"));
    LD_ASSERT(LDUserSetName(user, "bob"));
    LD_ASSERT(LDUserSetEmail(user, "test@example.com"));

    /* segment */
    LD_ASSERT(values1 = LDNewArray());
    LD_ASSERT(LDArrayAppend(values1, LDNewText("test@example.com")));

    LD_ASSERT(clause1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause1, "attribute", LDNewText("email")));
    LD_ASSERT(LDObjectSetKey(clause1, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause1, "values", values1));
    LD_ASSERT(LDObjectSetKey(clause1, "negate", LDNewBool(false)));

    LD_ASSERT(values2 = LDNewArray());
    LD_ASSERT(LDArrayAppend(values2, LDNewText("bob")));

    LD_ASSERT(clause2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause2, "attribute", LDNewText("name")));
    LD_ASSERT(LDObjectSetKey(clause2, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause2, "values", values2));
    LD_ASSERT(LDObjectSetKey(clause2, "negate", LDNewBool(false)));

    LD_ASSERT(clauses = LDNewArray());
    LD_ASSERT(LDArrayAppend(clauses, clause1));
    LD_ASSERT(LDArrayAppend(clauses, clause2));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "clauses", clauses));

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

static void
testNonMatchingRuleWithMultipleClauses()
{
    struct LDUser *user;
    struct LDJSON *segment;
    struct LDJSON *rules;
    struct LDJSON *rule;
    struct LDJSON *clauses;
    struct LDJSON *clause1;
    struct LDJSON *clause2;
    struct LDJSON *values1;
    struct LDJSON *values2;

    /* user */
    LD_ASSERT(user = LDUserNew("foo"));
    LD_ASSERT(LDUserSetName(user, "bob"));
    LD_ASSERT(LDUserSetEmail(user, "test@example.com"));

    /* segment */
    LD_ASSERT(values1 = LDNewArray());
    LD_ASSERT(LDArrayAppend(values1, LDNewText("test@example.com")));

    LD_ASSERT(clause1 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause1, "attribute", LDNewText("email")));
    LD_ASSERT(LDObjectSetKey(clause1, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause1, "values", values1));
    LD_ASSERT(LDObjectSetKey(clause1, "negate", LDNewBool(false)));

    LD_ASSERT(values2 = LDNewArray());
    LD_ASSERT(LDArrayAppend(values2, LDNewText("bill")));

    LD_ASSERT(clause2 = LDNewObject());
    LD_ASSERT(LDObjectSetKey(clause2, "attribute", LDNewText("name")));
    LD_ASSERT(LDObjectSetKey(clause2, "op", LDNewText("in")));
    LD_ASSERT(LDObjectSetKey(clause2, "values", values2));
    LD_ASSERT(LDObjectSetKey(clause2, "negate", LDNewBool(false)));

    LD_ASSERT(clauses = LDNewArray());
    LD_ASSERT(LDArrayAppend(clauses, clause1));
    LD_ASSERT(LDArrayAppend(clauses, clause2));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "clauses", clauses));

    LD_ASSERT(rules = LDNewArray());
    LD_ASSERT(LDArrayAppend(rules, rule));

    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("test")));
    LD_ASSERT(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    LD_ASSERT(LDObjectSetKey(segment, "rules", rules));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", LDNewBool(false)));

    /* run */
    LD_ASSERT(segmentMatchesUser(segment, user) == EVAL_MISS);

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
    testMatchingRuleWithZeroRollout();
    testMatchingRuleWithMultipleClauses();
    testNonMatchingRuleWithMultipleClauses();

    return 0;
}
