#include "ldnetwork.h"
#include "ldinternal.h"
#include "ldstreaming.h"
#include "ldstore.h"

static void
testParsePathFlags()
{
    char *kind, *key;

    LD_ASSERT(LDi_parsePath("/flags/abcd", &kind, &key));

    LD_ASSERT(strcmp(kind, "flags") == 0);
    LD_ASSERT(strcmp(key, "abcd") == 0);

    LDFree(kind);
    LDFree(key);
}

static void
testParsePathSegments()
{
    char *kind, *key;

    LD_ASSERT(LDi_parsePath("/segments/xyz", &kind, &key));

    LD_ASSERT(strcmp(kind, "segments") == 0);
    LD_ASSERT(strcmp(key, "xyz") == 0);

    LDFree(kind);
    LDFree(key);
}

static void
testParsePathUnknownKind()
{
    char *kind, *key;

    LD_ASSERT(!LDi_parsePath("/unknown/123", &kind, &key));

    LD_ASSERT(!kind);
    LD_ASSERT(!key);
}

static void
testInitialPut(struct StreamContext *const context)
{
    struct LDJSON *flag, *segment;

    const char *const event = "event: put";

    const char *const body =
        "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
        "{\"key\": \"my-flag\", \"version\": 2}},\"segments\": "
        "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}";

    LD_ASSERT(context);

    LD_ASSERT(LDi_onSSE(context, event));
    LD_ASSERT(LDi_onSSE(context, body));
    LD_ASSERT(LDi_onSSE(context, ""));

    LD_ASSERT(LDStoreGet(
        context->client->config->store, "flags", "my-flag", &flag));
    LD_ASSERT(flag);
    LD_ASSERT(LDGetNumber(LDObjectLookup(flag, "version")) == 2);

    LD_ASSERT(LDStoreGet(
        context->client->config->store, "segments", "my-segment", &segment));
    LD_ASSERT(segment);
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

    LD_ASSERT(LDi_onSSE(context, event));
    LD_ASSERT(LDi_onSSE(context, body));
    LD_ASSERT(LDi_onSSE(context, ""));

    LD_ASSERT(LDStoreGet(
        context->client->config->store, "flags", "my-flag", &flag));
    LD_ASSERT(flag);
    LD_ASSERT(LDGetNumber(LDObjectLookup(flag, "version")) == 3);

    LDJSONFree(flag);
}

static void
testDeleteFlag(struct StreamContext *const context)
{
    struct LDJSON *lookup;

    const char *const event = "event: delete";

    const char *const body =
        "data: {\"path\": \"/flags/my-flag\", \"version\": 4}";

    LD_ASSERT(context);

    LD_ASSERT(LDi_onSSE(context, event));
    LD_ASSERT(LDi_onSSE(context, body));
    LD_ASSERT(LDi_onSSE(context, ""));

    LD_ASSERT(LDStoreGet(
        context->client->config->store, "flags", "my-flag", &lookup));
    LD_ASSERT(!lookup);
}

static void
testPatchSegment(struct StreamContext *const context)
{
    struct LDJSON *segment;

    const char *const event = "event: patch";

    const char *const body =
        "data: {\"path\": \"/segments/my-segment\", \"data\": "
        "{\"key\": \"my-segment\", \"version\": 7}}";

    LD_ASSERT(context);

    LD_ASSERT(LDi_onSSE(context, event));
    LD_ASSERT(LDi_onSSE(context, body));
    LD_ASSERT(LDi_onSSE(context, ""));

    LD_ASSERT(LDStoreGet(
        context->client->config->store, "segments", "my-segment", &segment));
    LD_ASSERT(segment);
    LD_ASSERT(LDGetNumber(LDObjectLookup(segment, "version")) == 7);

    LDJSONFree(segment);
}

static void
testDeleteSegment(struct StreamContext *const context)
{
    struct LDJSON *lookup;

    const char *const event = "event: delete";

    const char *const body =
        "data: {\"path\": \"/segments/my-segment\", \"version\": 8}";

    LD_ASSERT(context);

    LD_ASSERT(LDi_onSSE(context, event));
    LD_ASSERT(LDi_onSSE(context, body));
    LD_ASSERT(LDi_onSSE(context, ""));

    LD_ASSERT(LDStoreGet(
        context->client->config->store, "segments", "my-segment", &lookup));
    LD_ASSERT(!lookup);
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
    testPatchSegment(context);
    testDeleteSegment(context);

    LDFree(context->dataBuffer);
    LDFree(context->memory);
    LDFree(context);
    LDClientClose(client);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testParsePathFlags();
    testParsePathSegments();
    testParsePathUnknownKind();
    testStreamOperations();

    return 0;
}
