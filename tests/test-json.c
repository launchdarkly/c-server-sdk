#include "ldapi.h"
#include "ldinternal.h"

static void
testNull()
{
    struct LDJSON *json = LDNewNull();

    LD_ASSERT(json);

    LD_ASSERT(LDJSONGetType(json) == LDNull);

    LDJSONFree(json);
}

static void
testBool()
{
    const bool value = true;

    struct LDJSON *json = LDNewBool(value);

    LD_ASSERT(json);

    LD_ASSERT(LDJSONGetType(json) == LDBool);

    LD_ASSERT(LDGetBool(json) == value);

    LDJSONFree(json);
}

static void
testNumber()
{
    const double value = 3.33;

    struct LDJSON *json = LDNewNumber(value);

    LD_ASSERT(json);

    LD_ASSERT(LDJSONGetType(json) == LDNumber);

    LD_ASSERT(LDGetNumber(json) == value);

    LDJSONFree(json);
}

static void
testText()
{
    const char *const value = "hello world!";

    struct LDJSON *json = LDNewText(value);

    LD_ASSERT(json);

    LD_ASSERT(LDJSONGetType(json) == LDText);

    LD_ASSERT(strcmp(LDGetText(json), value) == 0);

    LDJSONFree(json);
}

static void
testArray()
{
    struct LDJSON *json = NULL, *tmp = NULL, *iter = NULL;

    LD_ASSERT(json = LDNewArray());

    LD_ASSERT(tmp = LDNewBool(true));
    LD_ASSERT(LDArrayAppend(json, tmp));

    LD_ASSERT(tmp = LDNewBool(false));
    LD_ASSERT(LDArrayAppend(json, tmp));

    LD_ASSERT(iter = LDGetIter(json));
    LD_ASSERT(LDGetBool(iter) == true);

    LD_ASSERT(iter = LDIterNext(iter));
    LD_ASSERT(LDGetBool(iter) == false);

    LD_ASSERT(LDArrayGetSize(json) == 2);

    LDJSONFree(json);
}

static void
testObject()
{
    struct LDJSON *json = NULL, *tmp = NULL, *iter = NULL;

    LD_ASSERT(json = LDNewObject());

    LD_ASSERT(tmp = LDNewBool(true));
    LD_ASSERT(LDObjectSetKey(json, "a", tmp));

    LD_ASSERT(tmp = LDNewBool(false));
    LD_ASSERT(LDObjectSetKey(json, "b", tmp));

    LD_ASSERT(iter = LDGetIter(json));
    LD_ASSERT(strcmp(LDIterKey(iter), "a") == 0);
    LD_ASSERT(LDGetBool(iter) == true);

    LD_ASSERT(iter = LDIterNext(iter));
    LD_ASSERT(strcmp(LDIterKey(iter), "b") == 0);
    LD_ASSERT(LDGetBool(iter) == false);

    LD_ASSERT(tmp = LDObjectLookup(json, "b"));
    LD_ASSERT(LDGetBool(tmp) == false);

    LD_ASSERT(tmp = LDObjectLookup(json, "a"));
    LD_ASSERT(LDGetBool(tmp) == true);

    LDJSONFree(json);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    LDi_log(LD_LOG_INFO, "testNull()"); testNull();
    LDi_log(LD_LOG_INFO, "testBool()"); testBool();
    LDi_log(LD_LOG_INFO, "testNumber()"); testNumber();
    LDi_log(LD_LOG_INFO, "testText()"); testText();
    LDi_log(LD_LOG_INFO, "testArray()"); testArray();
    LDi_log(LD_LOG_INFO, "testObject()"); testObject();

    return 0;
}
