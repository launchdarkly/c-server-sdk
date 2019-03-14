#include "ldjson.h"
#include "ldoperators.h"
#include "ldinternal.h"

static struct LDJSON *tests;

static const char *const dateStr1 = "2017-12-06T00:00:00.000-07:00";
static const char *const dateStr2 = "2017-12-06T00:01:01.000-07:00";
static const unsigned int dateMs1 = 10000000;
static const unsigned int dateMs2 = 10000001;
static const char *const invalidDate = "hey what's this?";

void
addTest(const char *const op, struct LDJSON *const uvalue,
    struct LDJSON *const cvalue, const bool expect)
{
    struct LDJSON *test = NULL;

    LD_ASSERT(test = LDNewObject());
    LD_ASSERT(LDObjectSetKey(test, "op", LDNewText(op)));
    LD_ASSERT(LDObjectSetKey(test, "uvalue", uvalue));
    LD_ASSERT(LDObjectSetKey(test, "cvalue", cvalue));
    LD_ASSERT(LDObjectSetKey(test, "expect", LDNewBool(expect)));

    LD_ASSERT(LDArrayPush(tests, test));
}

void
testParseDateZero()
{
    struct LDJSON *jtimestamp;
    timestamp_t ttimestamp;
    struct LDJSON *jexpected;
    timestamp_t texpected;

    LD_ASSERT(jexpected = LDNewNumber(0.0));
    LD_ASSERT(parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("1970-01-01T00:00:00Z"));
    LD_ASSERT(parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

void
testParseUTCTimestamp()
{
    struct LDJSON *jtimestamp;
    timestamp_t ttimestamp;
    struct LDJSON *jexpected;
    timestamp_t texpected;

    LD_ASSERT(jexpected = LDNewNumber(1460847451684));
    LD_ASSERT(parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("2016-04-16T22:57:31.684Z"));
    LD_ASSERT(parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

void
testParseTimezone()
{
    struct LDJSON *jtimestamp;
    timestamp_t ttimestamp;
    struct LDJSON *jexpected;
    timestamp_t texpected;

    LD_ASSERT(jexpected = LDNewNumber(1460851752759));
    LD_ASSERT(parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("2016-04-16T17:09:12.759-07:00"));
    LD_ASSERT(parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

void
testParseTimezoneNoMillis()
{
    struct LDJSON *jtimestamp;
    timestamp_t ttimestamp;
    struct LDJSON *jexpected;
    timestamp_t texpected;

    LD_ASSERT(jexpected = LDNewNumber(1460851752000));
    LD_ASSERT(parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("2016-04-16T17:09:12-07:00"));
    LD_ASSERT(parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

void
testTimeCompareSimilar()
{
    struct LDJSON *jtimestamp1;
    timestamp_t ttimestamp1;
    struct LDJSON *jtimestamp2;
    timestamp_t ttimestamp2;

    LD_ASSERT(jtimestamp1 = LDNewNumber(1000));
    LD_ASSERT(parseTime(jtimestamp1, &ttimestamp1));

    LD_ASSERT(jtimestamp2 = LDNewNumber(1001));
    LD_ASSERT(parseTime(jtimestamp2, &ttimestamp2));

    LD_ASSERT(timestamp_compare(&ttimestamp1, &ttimestamp2) == -1);

    LDJSONFree(jtimestamp1);
    LDJSONFree(jtimestamp2);
}

/*
void
testParseTimestampBeforeEpoch()
{
    struct LDJSON *jtimestamp;
    timestamp_t ttimestamp;
    struct LDJSON *jexpected;
    timestamp_t texpected;

    LD_ASSERT(jexpected = LDNewNumber(-123456));
    LD_ASSERT(parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("1969-12-31T23:57:56.544-00:00"));
    LD_ASSERT(parseTime(jtimestamp, &ttimestamp));

    LD_LOG(LD_LOG_TRACE, "dec %d", timestamp_compare(&ttimestamp, &texpected));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}
*/

int
main()
{
    struct LDJSON *iter;

    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    LD_ASSERT(tests = LDNewArray());

    /* numeric operators */
    addTest("in", LDNewNumber(99), LDNewNumber(99), true);
    addTest("in", LDNewNumber(99.0001), LDNewNumber(99.0001), true);
    addTest("lessThan", LDNewNumber(1), LDNewNumber(1.99999), true);
    addTest("lessThan", LDNewNumber(1.99999), LDNewNumber(1), false);
    addTest("lessThan", LDNewNumber(1), LDNewNumber(2), true);
    addTest("lessThanOrEqual", LDNewNumber(1), LDNewNumber(1), true);
    addTest("greaterThan", LDNewNumber(2), LDNewNumber(1.99999), true);
    addTest("greaterThan", LDNewNumber(1.99999), LDNewNumber(2), false);
    addTest("greaterThan", LDNewNumber(2), LDNewNumber(1), true);
    addTest("greaterThanOrEqual", LDNewNumber(1), LDNewNumber(1), true);

    /* string operators */
    addTest("in", LDNewText("x"), LDNewText("x"), true);
    addTest("in", LDNewText("x"), LDNewText("xyz"), false);
    addTest("startsWith", LDNewText("xyz"), LDNewText("x"), true);
    addTest("startsWith", LDNewText("x"), LDNewText("xyz"), false);
    addTest("endsWith", LDNewText("xyz"), LDNewText("z"), true);
    addTest("endsWith", LDNewText("z"), LDNewText("xyz"), false);
    addTest("contains", LDNewText("xyz"), LDNewText("y"), true);
    addTest("contains", LDNewText("y"), LDNewText("yz"), false);

    /* mixed strings and numbers */
    addTest("in", LDNewText("99"), LDNewNumber(99), false);
    addTest("in", LDNewNumber(99), LDNewText("99"), false);
    addTest("contains", LDNewText("99"), LDNewNumber(99), false);
    addTest("startsWith", LDNewText("99"), LDNewNumber(99), false);
    addTest("endsWith", LDNewText("99"), LDNewNumber(99), false);
    addTest("lessThanOrEqual", LDNewText("99"), LDNewNumber(99), false);
    addTest("lessThanOrEqual", LDNewNumber(99), LDNewText("99"), false);
    addTest("greaterThanOrEqual", LDNewText("99"), LDNewNumber(99), false);
    addTest("greaterThanOrEqual", LDNewNumber(99), LDNewText("99"), false);

    /* date operators */
    addTest("before", LDNewText(dateStr1), LDNewText(dateStr2), true);
    addTest("before", LDNewNumber(dateMs1), LDNewNumber(dateMs2), true);
    addTest("before", LDNewText(dateStr2), LDNewText(dateStr1), false);
    addTest("before", LDNewNumber(dateMs2), LDNewNumber(dateMs1), false);
    addTest("before", LDNewText(dateStr1), LDNewText(dateStr1), false);
    addTest("before", LDNewNumber(dateMs1), LDNewNumber(dateMs1), false);
    addTest("before", LDNewText(""), LDNewText(dateStr1), false);
    addTest("before", LDNewText(dateStr1), LDNewText(invalidDate), false);
    addTest("after", LDNewText(dateStr2), LDNewText(dateStr1), true);
    addTest("after", LDNewNumber(dateMs2), LDNewNumber(dateMs1), true);
    addTest("after", LDNewText(dateStr1), LDNewText(dateStr2), false);
    addTest("after", LDNewNumber(dateMs1), LDNewNumber(dateMs2), false);
    addTest("after", LDNewText(dateStr1), LDNewText(dateStr1), false);
    addTest("after", LDNewNumber(dateMs1), LDNewNumber(dateMs1), false);
    addTest("after", LDNewText(""), LDNewText(dateStr1), false);
    addTest("after", LDNewText(dateStr1), LDNewText(invalidDate), false);

    /* regex */
    addTest("matches", LDNewText("hello world"), LDNewText("hello.*rld"), true);
    addTest("matches", LDNewText("hello world"), LDNewText("hello.*orl"), true);
    addTest("matches", LDNewText("hello world"), LDNewText("l+"), true);
    addTest("matches", LDNewText("hello world"),
        LDNewText("(world|planet)"), true);
    addTest("matches", LDNewText("hello world"), LDNewText("aloha"), false);
    addTest("matches", LDNewText("hello world"), LDNewText("***bad rg"), false);

    /* semver operators */
    addTest("semVerEqual", LDNewText("2.0.0"), LDNewText("2.0.0"), true);
    addTest("semVerEqual", LDNewText("2.0"), LDNewText("2.0.0"), true);
    addTest("semVerEqual", LDNewText("2-rc1"), LDNewText("2.0.0-rc1"), true);
    addTest("semVerEqual", LDNewText("2+build2"),
        LDNewText("2.0.0+build2"), true);
    addTest("semVerEqual", LDNewText("2.0.0"), LDNewText("2.0.1"), false);
    addTest("semVerLessThan", LDNewText("2.0.0"), LDNewText("2.0.1"), true);
    addTest("semVerLessThan", LDNewText("2.0"), LDNewText("2.0.1"), true);
    addTest("semVerLessThan", LDNewText("2.0.1"), LDNewText("2.0.0"), false);
    addTest("semVerLessThan", LDNewText("2.0.1"), LDNewText("2.0"), false);
    addTest("semVerLessThan", LDNewText("2.0.1"), LDNewText("xbad%ver"), false);
    addTest("semVerLessThan", LDNewText("2.0.0-rc"),
        LDNewText("2.0.0-rc.beta"), true);
    addTest("semVerGreaterThan", LDNewText("2.0.1"), LDNewText("2.0"), true);
    addTest("semVerGreaterThan", LDNewText("2.0.1"), LDNewText("2.0"), true);
    addTest("semVerGreaterThan", LDNewText("2.0.0"),
        LDNewText("2.0.1"), false);
    addTest("semVerGreaterThan", LDNewText("2.0"), LDNewText("2.0.1"), false);
    addTest("semVerGreaterThan", LDNewText("2.0.1"),
        LDNewText("xbad%ver"), false);
    addTest("semVerGreaterThan", LDNewText("2.0.0-rc.1"),
        LDNewText("2.0.0-rc.0"), true);
    addTest("semVerEqual", LDNewText("02.0.0"), LDNewText("2.0.0"), false);
    addTest("semVerEqual", LDNewText("v2.0.0"), LDNewText("2.0.0"), false);
    addTest("semVerEqual", LDNewText("2.01.0"), LDNewText("2.1.0"), false);
    addTest("semVerEqual", LDNewText("2.0.01"), LDNewText("2.0.1"), false);

    for (iter = LDGetIter(tests); iter; iter = LDIterNext(iter)) {
        OpFn opfn;
        struct LDJSON *op;
        struct LDJSON *uvalue;
        struct LDJSON *cvalue;
        struct LDJSON *expect;

        char *serializeduvalue;
        char *serializedcvalue;

        LD_ASSERT(op = LDObjectLookup(iter, "op"));
        LD_ASSERT(uvalue = LDObjectLookup(iter, "uvalue"));
        LD_ASSERT(cvalue = LDObjectLookup(iter, "cvalue"));
        LD_ASSERT(expect = LDObjectLookup(iter, "expect"));

        LD_ASSERT(serializeduvalue = LDJSONSerialize(uvalue));
        LD_ASSERT(serializedcvalue = LDJSONSerialize(cvalue));

        /*
        LD_LOG(LD_LOG_TRACE, "%s %s %s %u", LDGetText(op),
            serializeduvalue, serializedcvalue, LDGetBool(expect));
        */

        LD_ASSERT(opfn = lookupOperation(LDGetText(op)));

        LD_ASSERT(opfn(uvalue, cvalue) == LDGetBool(expect));

        LDFree(serializeduvalue);
        LDFree(serializedcvalue);
    }

    testParseDateZero();
    testParseUTCTimestamp();
    testParseTimezone();
    testParseTimezoneNoMillis();
    testTimeCompareSimilar();
    /* testParseTimestampBeforeEpoch(); */

    LDJSONFree(tests);

    return 0;
}
