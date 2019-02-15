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

    /* string operators */
    addTest("in",         LDNewText("x"),   LDNewText("x"),   true);
    addTest("in",         LDNewText("x"),   LDNewText("xyz"), false);
    addTest("startsWith", LDNewText("xyz"), LDNewText("x"),   true);
    addTest("startsWith", LDNewText("x"),   LDNewText("xyz"), false);
    addTest("endsWith",   LDNewText("xyz"), LDNewText("z"),   true);
    addTest("endsWith",   LDNewText("z"),   LDNewText("xyz"), false);
    addTest("contains",   LDNewText("xyz"), LDNewText("y"),   true);
    addTest("contains",   LDNewText("y"),   LDNewText("yz"),  false);

    for (iter = LDGetIter(tests); iter; iter = LDIterNext(iter)) {
        OpFn opfn;
        struct LDJSON *op;
        struct LDJSON *uvalue;
        struct LDJSON *cvalue;
        struct LDJSON *expect;

        LD_ASSERT(op = LDObjectLookup(iter, "op"));
        LD_ASSERT(uvalue = LDObjectLookup(iter, "uvalue"));
        LD_ASSERT(cvalue = LDObjectLookup(iter, "cvalue"));
        LD_ASSERT(expect = LDObjectLookup(iter, "expect"));

        LD_LOG(LD_LOG_TRACE, "%s", LDGetText(op));

        LD_ASSERT(opfn = lookupOperation(LDGetText(op)));

        LD_ASSERT(opfn(uvalue, cvalue) == LDGetBool(expect));
    }

    LDJSONFree(tests);

    return 0;
}
