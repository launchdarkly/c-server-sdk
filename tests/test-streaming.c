#include <launchdarkly/api.h>

#include "config.h"
#include "client.h"
#include "evaluate.h"
#include "streaming.h"
#include "misc.h"

static void
testParsePathFlags()
{
    enum FeatureKind kind;
    const char *key;

    LD_ASSERT(LDi_parsePath("/flags/abcd", &kind, &key));

    LD_ASSERT(kind == LD_FLAG);
    LD_ASSERT(strcmp(key, "abcd") == 0);
}

static void
testParsePathSegments()
{
    enum FeatureKind kind;
    const char *key;

    LD_ASSERT(LDi_parsePath("/segments/xyz", &kind, &key));

    LD_ASSERT(kind == LD_SEGMENT);
    LD_ASSERT(strcmp(key, "xyz") == 0);
}

static void
testParsePathUnknownKind()
{
    enum FeatureKind kind;
    const char *key;

    LD_ASSERT(!LDi_parsePath("/unknown/123", &kind, &key));

    LD_ASSERT(!key);
}

static void
testInitialPut(struct StreamContext *const context)
{
    struct LDJSONRC *flag, *segment;

    const char *const event =
        "event: put\n"
        "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
        "{\"key\": \"my-flag\", \"version\": 2}},\"segments\": "
        "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}\n\n";

    LD_ASSERT(context);

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));

    LD_ASSERT(LDStoreGet(
        context->client->store, LD_FLAG, "my-flag", &flag));
    LD_ASSERT(flag);
    LD_ASSERT(LDGetNumber(LDObjectLookup(LDJSONRCGet(flag), "version")) == 2);

    LD_ASSERT(LDStoreGet(
        context->client->store, LD_SEGMENT, "my-segment", &segment));
    LD_ASSERT(segment);
    LD_ASSERT(LDGetNumber(LDObjectLookup(LDJSONRCGet(segment), "version")) ==
        5);

    LDJSONRCDecrement(flag);
    LDJSONRCDecrement(segment);
}

static void
testPatchFlag(struct StreamContext *const context)
{
    struct LDJSONRC *flag;

    const char *const event =
        "event: patch\n"
        "data: {\"path\": \"/flags/my-flag\", \"data\": "
        "{\"key\": \"my-flag\", \"version\": 3}}\n\n";

    LD_ASSERT(context);

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));

    LD_ASSERT(LDStoreGet(
        context->client->store, LD_FLAG, "my-flag", &flag));
    LD_ASSERT(flag);
    LD_ASSERT(LDGetNumber(LDObjectLookup(LDJSONRCGet(flag), "version")) == 3);

    LDJSONRCDecrement(flag);
}

static void
testDeleteFlag(struct StreamContext *const context)
{
    struct LDJSONRC *lookup;

    const char *const event =
        "event: delete\n"
        "data: {\"path\": \"/flags/my-flag\", \"version\": 4}\n\n";

    LD_ASSERT(context);

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));

    LD_ASSERT(LDStoreGet(
        context->client->store, LD_FLAG, "my-flag", &lookup));
    LD_ASSERT(!lookup);
}

static void
testPatchSegment(struct StreamContext *const context)
{
    struct LDJSONRC *segment;

    const char *const event =
        "event: patch\n"
        "data: {\"path\": \"/segments/my-segment\", \"data\": "
        "{\"key\": \"my-segment\", \"version\": 7}}\n\n";

    LD_ASSERT(context);

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));

    LD_ASSERT(LDStoreGet(
        context->client->store, LD_SEGMENT, "my-segment", &segment));
    LD_ASSERT(segment);
    LD_ASSERT(LDGetNumber(LDObjectLookup(LDJSONRCGet(segment), "version")) ==
        7);

    LDJSONRCDecrement(segment);
}

static void
testDeleteSegment(struct StreamContext *const context)
{
    struct LDJSONRC *lookup;

    const char *const event =
        "event: delete\n"
        "data: {\"path\": \"/segments/my-segment\", \"version\": 8}\n\n";

    LD_ASSERT(context);

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));

    LD_ASSERT(LDStoreGet(
        context->client->store, LD_SEGMENT, "my-segment", &lookup));
    LD_ASSERT(!lookup);
}

static void
testStreamBundle(struct StreamContext *const context)
{
    testInitialPut(context);
    testPatchFlag(context);
    testDeleteFlag(context);
    testPatchSegment(context);
    testDeleteSegment(context);
}

static void
testStreamContext(void (*const action)())
{
    struct LDConfig *config;
    struct LDClient *client;
    struct StreamContext *context;

    LD_ASSERT(config = LDConfigNew("key"));
    LDConfigSetUseLDD(config, true);
    LDConfigSetSendEvents(config, false);
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(context = (struct StreamContext *)
        malloc(sizeof(struct StreamContext)));
    memset(context, 0, sizeof(struct StreamContext));
    context->client = client;

    action(context);

    LDFree(context->dataBuffer);
    LDFree(context->memory);
    LDFree(context);
    LDClientClose(client);
}

static void
testEventDataIsNotValidJSON(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: delete\n"
        "data: hello\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testEventDataIsNotAnObject(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: delete\n"
        "data: 123\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testDeleteWithoutPath(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: delete\n"
        "data: {\"version\": 8}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testDeletePathNotString(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: delete\n"
        "data: {\"path\": 123, \"version\": 8}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testDeletePathUnrecognized(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: delete\n"
        "data: {\"path\": \"hello\", \"version\": 8}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testDeleteMissingVersion(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: delete\n"
        "data: {\"path\": \"/flags/my-flag\"}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testDeleteVersionNotANumber(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: delete\n"
        "data: {\"path\": \"/flags/my-flag\", \"version\": \"test\"}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPatchInvalidPath(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: patch\n"
        "data: {\"path\": 123, \"data\": "
        "{\"key\": \"my-flag\", \"version\": 3}}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPatchMissingDataField(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: patch\n"
        "data: {\"path\": \"/flags/my-flag\"}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPutMissingDataField(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: put\n"
        "data: {\"path\": \"/\"}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPutDataNotAnObject(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: put\n"
        "data: {\"path\": \"/\", \"data\": 52}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPutDataMissingFlagsField(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: put\n"
        "data: {\"path\": \"/\", \"data\": {\"segments\": "
        "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPutDataFlagsNotAnObject(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: put\n"
        "data: {\"path\": \"/\", \"data\": {\"flags\": 123, \"segments\": "
        "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPutDataMissingSegmentsField(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: put\n"
        "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
        "{\"key\": \"my-flag\", \"version\": 2}}}}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testPutDataSegmentsNotAnObject(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: put\n"
        "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
        "{\"key\": \"my-flag\", \"version\": 2}},\"segments\": 52}}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testSSEUnknownEventType(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event =
        "event: hello\n"
        "data: {}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testSSENoData(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event = "event: hello\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

static void
testSSENoEventType(struct StreamContext *const context)
{
    LD_ASSERT(context);

    const char *const event = "data: {}\n\n";

    LD_ASSERT(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testParsePathFlags();
    testParsePathSegments();
    testParsePathUnknownKind();
    testStreamContext(testStreamBundle);
    testStreamContext(testEventDataIsNotValidJSON);
    testStreamContext(testEventDataIsNotAnObject);
    testStreamContext(testDeleteWithoutPath);
    testStreamContext(testDeletePathNotString);
    testStreamContext(testDeletePathUnrecognized);
    testStreamContext(testDeleteMissingVersion);
    testStreamContext(testDeleteVersionNotANumber);
    testStreamContext(testPatchInvalidPath);
    testStreamContext(testPatchMissingDataField);
    testStreamContext(testPutMissingDataField);
    testStreamContext(testPutDataNotAnObject);
    testStreamContext(testPutDataMissingFlagsField);
    testStreamContext(testPutDataFlagsNotAnObject);
    testStreamContext(testPutDataMissingSegmentsField);
    testStreamContext(testPutDataSegmentsNotAnObject);
    testStreamContext(testSSEUnknownEventType);
    testStreamContext(testSSENoData);
    testStreamContext(testSSENoEventType);

    return 0;
}
