#include "ldnetwork.h"
#include "ldinternal.h"

void
testParsePathFlags()
{
    char *kind;
    char *key;

    LD_ASSERT(parsePath("/flags/abcd", &kind, &key));

    LD_ASSERT(strcmp(kind, "flags") == 0);
    LD_ASSERT(strcmp(key, "abcd") == 0);

    free(kind);
    free(key);
}

void
testParsePathSegments()
{
    char *kind;
    char *key;

    LD_ASSERT(parsePath("/segments/xyz", &kind, &key));

    LD_ASSERT(strcmp(kind, "segments") == 0);
    LD_ASSERT(strcmp(key, "xyz") == 0);

    free(kind);
    free(key);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    testParsePathFlags();
    testParsePathSegments();

    return 0;
}
