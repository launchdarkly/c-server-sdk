#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>

#include "assertion.h"
#include "evaluate.h"
#include "utility.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class SegmentsFixture : public CommonFixture {
};


static struct LDJSON *
makeTestSegment(struct LDJSON *const rules) {
    struct LDJSON *segment;

    LD_ASSERT(segment = LDNewObject());
    LD_ASSERT(LDObjectSetKey(segment, "key", LDNewText("test")));
    LD_ASSERT(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    LD_ASSERT(LDObjectSetKey(segment, "rules", rules));
    LD_ASSERT(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", LDNewBool(LDBooleanFalse)));

    return segment;
}

TEST_F(SegmentsFixture, ExplicitIncludeUser) {
    struct LDUser *user;
    struct LDJSON *segment, *tmp;

    /* user */
    ASSERT_TRUE(user = LDUserNew("foo"));

    /* segment */
    ASSERT_TRUE(segment = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(segment, "key", LDNewText("test")));
    ASSERT_TRUE(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    ASSERT_TRUE(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(segment, "deleted", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(tmp = LDNewArray());
    ASSERT_TRUE(LDArrayPush(tmp, LDNewText("foo")));
    ASSERT_TRUE(LDObjectSetKey(segment, "included", tmp));

    /* run */
    ASSERT_EQ(LDi_segmentMatchesUser(segment, user), EVAL_MATCH);

    LDJSONFree(segment);
    LDUserFree(user);
}

TEST_F(SegmentsFixture, ExplicitExcludeUser) {
    struct LDUser *user;
    struct LDJSON *segment, *tmp;

    /* user */
    ASSERT_TRUE(user = LDUserNew("foo"));

    /* segment */
    ASSERT_TRUE(segment = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(segment, "key", LDNewText("test")));
    ASSERT_TRUE(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    ASSERT_TRUE(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(segment, "deleted", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(tmp = LDNewArray());
    ASSERT_TRUE(LDArrayPush(tmp, LDNewText("foo")));
    ASSERT_TRUE(LDObjectSetKey(segment, "excluded", tmp));

    /* run */
    ASSERT_EQ(LDi_segmentMatchesUser(segment, user), EVAL_MISS);

    LDJSONFree(segment);
    LDUserFree(user);
}

TEST_F(SegmentsFixture, ExplicitIncludeHasPrecedence) {
    struct LDUser *user;
    struct LDJSON *segment, *tmp;

    /* user */
    ASSERT_TRUE(user = LDUserNew("foo"));

    /* segment */
    ASSERT_TRUE(segment = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(segment, "key", LDNewText("test")));
    ASSERT_TRUE(LDObjectSetKey(segment, "salt", LDNewText("abcdef")));
    ASSERT_TRUE(LDObjectSetKey(segment, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(segment, "deleted", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(tmp = LDNewArray());
    ASSERT_TRUE(LDArrayPush(tmp, LDNewText("foo")));
    ASSERT_TRUE(LDObjectSetKey(segment, "excluded", tmp));

    ASSERT_TRUE(tmp = LDNewArray());
    ASSERT_TRUE(LDArrayPush(tmp, LDNewText("foo")));
    ASSERT_TRUE(LDObjectSetKey(segment, "included", tmp));

    /* run */
    ASSERT_EQ(LDi_segmentMatchesUser(segment, user), EVAL_MATCH);

    LDJSONFree(segment);
    LDUserFree(user);
}

TEST_F(SegmentsFixture, MatchingRuleWithFullRollout) {
    struct LDUser *user;
    struct LDJSON *segment, *rules, *rule, *clauses, *clause, *values;

    /* user */
    ASSERT_TRUE(user = LDUserNew("foo"));
    ASSERT_TRUE(LDUserSetEmail(user, "test@example.com"));

    /* segment */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("test@example.com")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("email")));
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "negate", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(clauses = LDNewArray());
    ASSERT_TRUE(LDArrayPush(clauses, clause));

    ASSERT_TRUE(rule = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(rule, "clauses", clauses));
    ASSERT_TRUE(LDObjectSetKey(rule, "weight", LDNewNumber(100000)));

    ASSERT_TRUE(rules = LDNewArray());
    ASSERT_TRUE(LDArrayPush(rules, rule));

    ASSERT_TRUE(segment = makeTestSegment(rules));

    /* run */
    ASSERT_EQ(LDi_segmentMatchesUser(segment, user), EVAL_MATCH);

    LDJSONFree(segment);
    LDUserFree(user);
}

TEST_F(SegmentsFixture, MatchingRuleWithZeroRollout) {
    struct LDUser *user;
    struct LDJSON *segment, *rules, *rule, *clauses, *clause, *values;

    /* user */
    ASSERT_TRUE(user = LDUserNew("foo"));
    ASSERT_TRUE(LDUserSetEmail(user, "test@example.com"));

    /* segment */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("test@example.com")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("email")));
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "negate", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(clauses = LDNewArray());
    ASSERT_TRUE(LDArrayPush(clauses, clause));

    ASSERT_TRUE(rule = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(rule, "clauses", clauses));
    ASSERT_TRUE(LDObjectSetKey(rule, "weight", LDNewNumber(0)));

    ASSERT_TRUE(rules = LDNewArray());
    ASSERT_TRUE(LDArrayPush(rules, rule));

    ASSERT_TRUE(segment = makeTestSegment(rules));

    /* run */
    ASSERT_EQ(LDi_segmentMatchesUser(segment, user), EVAL_MISS);

    LDJSONFree(segment);
    LDUserFree(user);
}

TEST_F(SegmentsFixture, MatchingRuleWithMultipleClauses) {
    struct LDUser *user;
    struct LDJSON *segment, *rules, *rule, *clauses, *clause1, *clause2,
            *values1, *values2;

    /* user */
    ASSERT_TRUE(user = LDUserNew("foo"));
    ASSERT_TRUE(LDUserSetName(user, "bob"));
    ASSERT_TRUE(LDUserSetEmail(user, "test@example.com"));

    /* segment */
    ASSERT_TRUE(values1 = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values1, LDNewText("test@example.com")));

    ASSERT_TRUE(clause1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause1, "attribute", LDNewText("email")));
    ASSERT_TRUE(LDObjectSetKey(clause1, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause1, "values", values1));
    ASSERT_TRUE(LDObjectSetKey(clause1, "negate", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(values2 = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values2, LDNewText("bob")));

    ASSERT_TRUE(clause2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause2, "attribute", LDNewText("name")));
    ASSERT_TRUE(LDObjectSetKey(clause2, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause2, "values", values2));
    ASSERT_TRUE(LDObjectSetKey(clause2, "negate", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(clauses = LDNewArray());
    ASSERT_TRUE(LDArrayPush(clauses, clause1));
    ASSERT_TRUE(LDArrayPush(clauses, clause2));

    ASSERT_TRUE(rule = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(rule, "clauses", clauses));

    ASSERT_TRUE(rules = LDNewArray());
    ASSERT_TRUE(LDArrayPush(rules, rule));

    ASSERT_TRUE(segment = makeTestSegment(rules));

    /* run */
    ASSERT_EQ(LDi_segmentMatchesUser(segment, user), EVAL_MATCH);

    LDJSONFree(segment);
    LDUserFree(user);
}

TEST_F(SegmentsFixture, NonMatchingRuleWithMultipleClauses) {
    struct LDUser *user;
    struct LDJSON *segment, *rules, *rule, *clauses, *clause1, *clause2,
            *values1, *values2;

    /* user */
    ASSERT_TRUE(user = LDUserNew("foo"));
    ASSERT_TRUE(LDUserSetName(user, "bob"));
    ASSERT_TRUE(LDUserSetEmail(user, "test@example.com"));

    /* segment */
    ASSERT_TRUE(values1 = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values1, LDNewText("test@example.com")));

    ASSERT_TRUE(clause1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause1, "attribute", LDNewText("email")));
    ASSERT_TRUE(LDObjectSetKey(clause1, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause1, "values", values1));
    ASSERT_TRUE(LDObjectSetKey(clause1, "negate", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(values2 = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values2, LDNewText("bill")));

    ASSERT_TRUE(clause2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause2, "attribute", LDNewText("name")));
    ASSERT_TRUE(LDObjectSetKey(clause2, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause2, "values", values2));
    ASSERT_TRUE(LDObjectSetKey(clause2, "negate", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(clauses = LDNewArray());
    ASSERT_TRUE(LDArrayPush(clauses, clause1));
    ASSERT_TRUE(LDArrayPush(clauses, clause2));

    ASSERT_TRUE(rule = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(rule, "clauses", clauses));

    ASSERT_TRUE(rules = LDNewArray());
    ASSERT_TRUE(LDArrayPush(rules, rule));

    ASSERT_TRUE(segment = makeTestSegment(rules));

    /* run */
    ASSERT_EQ(LDi_segmentMatchesUser(segment, user), EVAL_MISS);

    LDJSONFree(segment);
    LDUserFree(user);
}
