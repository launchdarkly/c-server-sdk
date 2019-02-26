#include "ldnetwork.h"
#include "ldinternal.h"
#include "ldstreaming.h"
#include "ldstore.h"

static void
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

static void
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

static void
testParsePathUnknownKind()
{
    char *kind = NULL;
    char *key = NULL;

    LD_ASSERT(!parsePath("/unknown/123", &kind, &key));

    LD_ASSERT(!kind);
    LD_ASSERT(!key);
}

static void
testInitialPut(struct StreamContext *const context)
{
    struct LDJSON *flag;
    struct LDJSON *segment;

    const char *const event = "event: put";

    const char *const body =
        "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
        "{\"key\": \"my-flag\", \"version\": 2}},\"segments\": "
        "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}";

    LD_ASSERT(context);

    LD_ASSERT(onSSE(context, event));
    LD_ASSERT(onSSE(context, body));
    LD_ASSERT(onSSE(context, ""));

    LD_ASSERT(flag = LDStoreGet(
        context->client->config->store, "flags", "my-flag"));
    LD_ASSERT(LDGetNumber(LDObjectLookup(flag, "version")) == 2);

    LD_ASSERT(segment = LDStoreGet(
        context->client->config->store, "segments", "my-segment"));
    LD_ASSERT(LDGetNumber(LDObjectLookup(segment, "version")) == 5);

    LDJSONFree(flag);
    LDJSONFree(segment);
}

static void
testPatchFlag(struct StreamContext *const context)
{
    struct LDJSON *flag;

    const char *const event = "event: patch";

    const char *const body =
        "data: {\"path\": \"/flags/my-flag\", \"data\": "
        "{\"key\": \"my-flag\", \"version\": 3}}";

    LD_ASSERT(context);

    LD_ASSERT(onSSE(context, event));
    LD_ASSERT(onSSE(context, body));
    LD_ASSERT(onSSE(context, ""));

    LD_ASSERT(flag = LDStoreGet(
        context->client->config->store, "flags", "my-flag"));
    LD_ASSERT(LDGetNumber(LDObjectLookup(flag, "version")) == 3);

    LDJSONFree(flag);
}

static void
testDeleteFlag(struct StreamContext *const context)
{
    const char *const event = "event: delete";

    const char *const body =
        "data: {\"path\": \"/flags/my-flag\", \"version\": 4}";

    LD_ASSERT(context);

    LD_ASSERT(onSSE(context, event));
    LD_ASSERT(onSSE(context, body));
    LD_ASSERT(onSSE(context, ""));

    LD_ASSERT(!LDStoreGet(
        context->client->config->store, "flags", "my-flag"));
}

static void
testStreamOperations()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct StreamContext *context;

    LD_ASSERT(config = LDConfigNew("key"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(context = malloc(sizeof(struct StreamContext)));
    memset(context, 0, sizeof(struct StreamContext));
    context->client = client;

    testInitialPut(context);
    testPatchFlag(context);
    testDeleteFlag(context);

    free(context->dataBuffer);
    free(context->memory);
    free(context);
    LDClientClose(client);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    testParsePathFlags();
    testParsePathSegments();
    testParsePathUnknownKind();
    testStreamOperations();

    return 0;
}
