#include "ldinternal.h"

void
allocCheckAndFreeNull()
{
    struct LDNode *const node = LDNodeNewNull();

    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeNull);

    LDNodeFree(node);
}

void
allocCheckAndFreeText()
{
    const char *const text = "hello world!";

    struct LDNode *const node = LDNodeNewText(text);

    LD_ASSERT(node);
    LD_ASSERT(LDNodeGetType(node) == LDNodeText);
    LD_ASSERT(strcmp(LDNodeGetText(node), text) == 0);

    LDNodeFree(node);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    allocCheckAndFreeNull();
    allocCheckAndFreeText();

    return 0;
}
