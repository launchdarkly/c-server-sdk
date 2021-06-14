#include <launchdarkly/api.h>

#include "assertion.h"
#include "operators.h"
#include "utility.h"

static struct LDJSON *tests;

static const char *const  dateStr1    = "2017-12-06T00:00:00.000-07:00";
static const char *const  dateStr2    = "2017-12-06T00:01:01.000-07:00";
static const unsigned int dateMs1     = 10000000;
static const unsigned int dateMs2     = 10000001;
static const char *const  invalidDate = "hey what's this?";

static void
addTest(
    const char *const    op,
    struct LDJSON *const uvalue,
    struct LDJSON *const cvalue,
    const LDBoolean      expect)
{
    struct LDJSON *test = NULL;

    LD_ASSERT(test = LDNewObject());
    LD_ASSERT(LDObjectSetKey(test, "op", LDNewText(op)));
    LD_ASSERT(LDObjectSetKey(test, "uvalue", uvalue));
    LD_ASSERT(LDObjectSetKey(test, "cvalue", cvalue));
    LD_ASSERT(LDObjectSetKey(test, "expect", LDNewBool(expect)));

    LD_ASSERT(LDArrayPush(tests, test));
}

static void
testParseDateZero()
{
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t    texpected, ttimestamp;

    LD_ASSERT(jexpected = LDNewNumber(0.0));
    LD_ASSERT(LDi_parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("1970-01-01T00:00:00Z"));
    LD_ASSERT(LDi_parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

static void
testParseUTCTimestamp()
{
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t    texpected, ttimestamp;

    LD_ASSERT(jexpected = LDNewNumber(1460847451684));
    LD_ASSERT(LDi_parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("2016-04-16T22:57:31.684Z"));
    LD_ASSERT(LDi_parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

static void
testParseTimezone()
{
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t    texpected, ttimestamp;

    LD_ASSERT(jexpected = LDNewNumber(1460851752759));
    LD_ASSERT(LDi_parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("2016-04-16T17:09:12.759-07:00"));
    LD_ASSERT(LDi_parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

static void
testParseTimezoneNoMillis()
{
    struct LDJSON *jtimestamp, *jexpected;
    timestamp_t    texpected, ttimestamp;

    LD_ASSERT(jexpected = LDNewNumber(1460851752000));
    LD_ASSERT(LDi_parseTime(jexpected, &texpected));

    LD_ASSERT(jtimestamp = LDNewText("2016-04-16T17:09:12-07:00"));
    LD_ASSERT(LDi_parseTime(jtimestamp, &ttimestamp));

    LD_ASSERT(timestamp_compare(&ttimestamp, &texpected) == 0);

    LDJSONFree(jtimestamp);
    LDJSONFree(jexpected);
}

static void
testTimeCompareSimilar()
{
    struct LDJSON *jtimestamp1, *jtimestamp2;
    timestamp_t    ttimestamp2, ttimestamp1;

    LD_ASSERT(jtimestamp1 = LDNewNumber(1000));
    LD_ASSERT(LDi_parseTime(jtimestamp1, &ttimestamp1));

    LD_ASSERT(jtimestamp2 = LDNewNumber(1001));
    LD_ASSERT(LDi_parseTime(jtimestamp2, &ttimestamp2));

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

    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    LD_ASSERT(tests = LDNewArray());

    /* numeric operators */
    addTest("in", LDNewNumber(99), LDNewNumber(99), LDBooleanTrue);
    addTest("in", LDNewNumber(99.0001), LDNewNumber(99.0001), LDBooleanTrue);
    addTest("lessThan", LDNewNumber(1), LDNewNumber(1.99999), LDBooleanTrue);
    addTest("lessThan", LDNewNumber(1.99999), LDNewNumber(1), LDBooleanFalse);
    addTest("lessThan", LDNewNumber(1), LDNewNumber(2), LDBooleanTrue);
    addTest("lessThanOrEqual", LDNewNumber(1), LDNewNumber(1), LDBooleanTrue);
    addTest("greaterThan", LDNewNumber(2), LDNewNumber(1.99999), LDBooleanTrue);
    addTest(
        "greaterThan", LDNewNumber(1.99999), LDNewNumber(2), LDBooleanFalse);
    addTest("greaterThan", LDNewNumber(2), LDNewNumber(1), LDBooleanTrue);
    addTest(
        "greaterThanOrEqual", LDNewNumber(1), LDNewNumber(1), LDBooleanTrue);

    /* string operators */
    addTest("in", LDNewText("x"), LDNewText("x"), LDBooleanTrue);
    addTest("in", LDNewText("x"), LDNewText("xyz"), LDBooleanFalse);
    addTest("startsWith", LDNewText("xyz"), LDNewText("x"), LDBooleanTrue);
    addTest("startsWith", LDNewText("x"), LDNewText("xyz"), LDBooleanFalse);
    addTest("endsWith", LDNewText("xyz"), LDNewText("z"), LDBooleanTrue);
    addTest("endsWith", LDNewText("z"), LDNewText("xyz"), LDBooleanFalse);
    addTest("contains", LDNewText("xyz"), LDNewText("y"), LDBooleanTrue);
    addTest("contains", LDNewText("y"), LDNewText("yz"), LDBooleanFalse);

    /* mixed strings and numbers */
    addTest("in", LDNewText("99"), LDNewNumber(99), LDBooleanFalse);
    addTest("in", LDNewNumber(99), LDNewText("99"), LDBooleanFalse);
    addTest("contains", LDNewText("99"), LDNewNumber(99), LDBooleanFalse);
    addTest("startsWith", LDNewText("99"), LDNewNumber(99), LDBooleanFalse);
    addTest("endsWith", LDNewText("99"), LDNewNumber(99), LDBooleanFalse);
    addTest(
        "lessThanOrEqual", LDNewText("99"), LDNewNumber(99), LDBooleanFalse);
    addTest(
        "lessThanOrEqual", LDNewNumber(99), LDNewText("99"), LDBooleanFalse);
    addTest(
        "greaterThanOrEqual", LDNewText("99"), LDNewNumber(99), LDBooleanFalse);
    addTest(
        "greaterThanOrEqual", LDNewNumber(99), LDNewText("99"), LDBooleanFalse);

    /* date operators */
    addTest("before", LDNewText(dateStr1), LDNewText(dateStr2), LDBooleanTrue);
    addTest(
        "before", LDNewNumber(dateMs1), LDNewNumber(dateMs2), LDBooleanTrue);
    addTest("before", LDNewText(dateStr2), LDNewText(dateStr1), LDBooleanFalse);
    addTest(
        "before", LDNewNumber(dateMs2), LDNewNumber(dateMs1), LDBooleanFalse);
    addTest("before", LDNewText(dateStr1), LDNewText(dateStr1), LDBooleanFalse);
    addTest(
        "before", LDNewNumber(dateMs1), LDNewNumber(dateMs1), LDBooleanFalse);
    addTest("before", LDNewText(""), LDNewText(dateStr1), LDBooleanFalse);
    addTest(
        "before", LDNewText(dateStr1), LDNewText(invalidDate), LDBooleanFalse);
    addTest("after", LDNewText(dateStr2), LDNewText(dateStr1), LDBooleanTrue);
    addTest("after", LDNewNumber(dateMs2), LDNewNumber(dateMs1), LDBooleanTrue);
    addTest("after", LDNewText(dateStr1), LDNewText(dateStr2), LDBooleanFalse);
    addTest(
        "after", LDNewNumber(dateMs1), LDNewNumber(dateMs2), LDBooleanFalse);
    addTest("after", LDNewText(dateStr1), LDNewText(dateStr1), LDBooleanFalse);
    addTest(
        "after", LDNewNumber(dateMs1), LDNewNumber(dateMs1), LDBooleanFalse);
    addTest("after", LDNewText(""), LDNewText(dateStr1), LDBooleanFalse);
    addTest(
        "after", LDNewText(dateStr1), LDNewText(invalidDate), LDBooleanFalse);

    /* regex */
    addTest(
        "matches",
        LDNewText("hello world"),
        LDNewText("hello.*rld"),
        LDBooleanTrue);
    addTest(
        "matches",
        LDNewText("hello world"),
        LDNewText("hello.*orl"),
        LDBooleanTrue);
    addTest(
        "matches", LDNewText("hello world"), LDNewText("l+"), LDBooleanTrue);
    addTest(
        "matches",
        LDNewText("hello world"),
        LDNewText("(world|planet)"),
        LDBooleanTrue);
    addTest(
        "matches",
        LDNewText("hello world"),
        LDNewText("aloha"),
        LDBooleanFalse);
    addTest(
        "matches",
        LDNewText("hello world"),
        LDNewText("***bad rg"),
        LDBooleanFalse);

    /* semver operators */
    addTest(
        "semVerEqual", LDNewText("2.0.0"), LDNewText("2.0.0"), LDBooleanTrue);
    addTest("semVerEqual", LDNewText("2.0"), LDNewText("2.0.0"), LDBooleanTrue);
    addTest(
        "semVerEqual",
        LDNewText("2-rc1"),
        LDNewText("2.0.0-rc1"),
        LDBooleanTrue);
    addTest(
        "semVerEqual",
        LDNewText("2+build2"),
        LDNewText("2.0.0+build2"),
        LDBooleanTrue);
    addTest(
        "semVerEqual", LDNewText("2.0.0"), LDNewText("2.0.1"), LDBooleanFalse);
    addTest(
        "semVerLessThan",
        LDNewText("2.0.0"),
        LDNewText("2.0.1"),
        LDBooleanTrue);
    addTest(
        "semVerLessThan", LDNewText("2.0"), LDNewText("2.0.1"), LDBooleanTrue);
    addTest(
        "semVerLessThan",
        LDNewText("2.0.1"),
        LDNewText("2.0.0"),
        LDBooleanFalse);
    addTest(
        "semVerLessThan", LDNewText("2.0.1"), LDNewText("2.0"), LDBooleanFalse);
    addTest(
        "semVerLessThan",
        LDNewText("2.0.1"),
        LDNewText("xbad%ver"),
        LDBooleanFalse);
    addTest(
        "semVerLessThan",
        LDNewText("2.0.0-rc"),
        LDNewText("2.0.0-rc.beta"),
        LDBooleanTrue);
    addTest(
        "semVerGreaterThan",
        LDNewText("2.0.1"),
        LDNewText("2.0"),
        LDBooleanTrue);
    addTest(
        "semVerGreaterThan",
        LDNewText("2.0.1"),
        LDNewText("2.0"),
        LDBooleanTrue);
    addTest(
        "semVerGreaterThan",
        LDNewText("2.0.0"),
        LDNewText("2.0.1"),
        LDBooleanFalse);
    addTest(
        "semVerGreaterThan",
        LDNewText("2.0"),
        LDNewText("2.0.1"),
        LDBooleanFalse);
    addTest(
        "semVerGreaterThan",
        LDNewText("2.0.1"),
        LDNewText("xbad%ver"),
        LDBooleanFalse);
    addTest(
        "semVerGreaterThan",
        LDNewText("2.0.0-rc.1"),
        LDNewText("2.0.0-rc.0"),
        LDBooleanTrue);
    addTest(
        "semVerEqual", LDNewText("02.0.0"), LDNewText("2.0.0"), LDBooleanFalse);
    addTest(
        "semVerEqual", LDNewText("v2.0.0"), LDNewText("2.0.0"), LDBooleanFalse);
    addTest(
        "semVerEqual", LDNewText("2.01.0"), LDNewText("2.1.0"), LDBooleanFalse);
    addTest(
        "semVerEqual", LDNewText("2.0.01"), LDNewText("2.0.1"), LDBooleanFalse);

    for (iter = LDGetIter(tests); iter; iter = LDIterNext(iter)) {
        OpFn           opfn;
        struct LDJSON *op, *uvalue, *cvalue, *expect;
        char *         serializeduvalue, *serializedcvalue;

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

        LD_ASSERT(opfn = LDi_lookupOperation(LDGetText(op)));

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

    LDBasicLoggerThreadSafeShutdown();

    return 0;
}
