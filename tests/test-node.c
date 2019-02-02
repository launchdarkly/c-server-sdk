#include "ldinternal.h"

static void
allocCheckAndFreeNull()
{
    char *resultJSON = NULL;
    const char *const expectedJSON = "null";
    /* construction test */
    struct LDNode *node = LDNodeNewNull();
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeNull);
    /* encoding test */
    resultJSON = LDNodeToJSONString(node);
    LD_ASSERT(strcmp(resultJSON, expectedJSON) == 0);
    LDNodeFree(node);
    free(resultJSON);
    /* decoding test */
    node = LDNodeFromJSONString("null");
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeNull);
    LDNodeFree(node);
}

static void
allocCheckAndFreeBool()
{
    char *resultJSON = NULL;
    const char *const expectedJSON = "true";
    const bool expectedValue = true;
    /* construction test */
    struct LDNode *node = LDNodeNewBool(expectedValue);
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeBool);
    LD_ASSERT(LDNodeGetBool(node) == expectedValue);
    /* encoding test */
    resultJSON = LDNodeToJSONString(node);
    LD_ASSERT(strcmp(resultJSON, expectedJSON) == 0);
    LDNodeFree(node);
    free(resultJSON);
    /* decoding test */
    node = LDNodeFromJSONString(expectedJSON);
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeBool);
    LD_ASSERT(LDNodeGetBool(node) == true);
    LDNodeFree(node);
}

static void
allocCheckAndFreeNumber()
{
    char *resultJSON = NULL;
    const char *const expectedJSON = "42";
    const int expectedValue = 42;
    /* construction test */
    struct LDNode *node = LDNodeNewNumber(expectedValue);
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeNumber);
    LD_ASSERT(LDNodeGetNumber(node) == expectedValue);
    /* encoding test */
    resultJSON = LDNodeToJSONString(node);
    LD_ASSERT(strcmp(resultJSON, expectedJSON) == 0);
    LDNodeFree(node);
    free(resultJSON);
    /* decoding test */
    node = LDNodeFromJSONString(expectedJSON);
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeNumber);
    LD_ASSERT(LDNodeGetNumber(node) == expectedValue);
    LDNodeFree(node);
}

static void
allocCheckAndFreeText()
{
    char *resultJSON = NULL;
    const char *const expectedJSON = "\"hello world!\"";
    const char *const expectedValue = "hello world!";
    /* construction test */
    struct LDNode *node = LDNodeNewText(expectedValue);
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeText);
    LD_ASSERT(strcmp(LDNodeGetText(node), expectedValue) == 0);
    /* encoding test */
    resultJSON = LDNodeToJSONString(node);
    LD_ASSERT(strcmp(resultJSON, expectedJSON) == 0);
    LDNodeFree(node);
    free(resultJSON);
    /* decoding test */
    node = LDNodeFromJSONString(expectedJSON);
    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeText);
    LD_ASSERT(strcmp(LDNodeGetText(node), expectedValue) == 0);
    LDNodeFree(node);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    allocCheckAndFreeNull();
    allocCheckAndFreeBool();
    allocCheckAndFreeNumber();
    allocCheckAndFreeText();

    return 0;
}
