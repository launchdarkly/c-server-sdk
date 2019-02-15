#include "ldjson.h"
#include "ldoperators.h"
#include "ldinternal.h"

static struct LDJSON *tests;

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

    LD_ASSERT(LDArrayAppend(tests, test));
}

int
main()
{
    struct LDJSON *iter;

    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

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

    /* regex */
    addTest("matches", LDNewText("hello world"), LDNewText("hello.*rld"), true);
    addTest("matches", LDNewText("hello world"), LDNewText("hello.*orl"), true);
    addTest("matches", LDNewText("hello world"), LDNewText("l+"), true);
    addTest("matches", LDNewText("hello world"), LDNewText("(world|pl)"), true);
    addTest("matches", LDNewText("hello world"), LDNewText("aloha"), false);
    addTest("matches", LDNewText("hello world"), LDNewText("***bad rg"), false);

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

        LD_LOG(LD_LOG_TRACE, "%s %s %s %u", LDGetText(op),
            serializeduvalue, serializedcvalue, LDGetBool(expect));

        LD_ASSERT(opfn = lookupOperation(LDGetText(op)));

        LD_ASSERT(opfn(uvalue, cvalue) == LDGetBool(expect));

        free(serializeduvalue);
        free(serializedcvalue);
    }

    LDJSONFree(tests);

    return 0;
}
