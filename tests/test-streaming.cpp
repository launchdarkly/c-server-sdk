#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "streaming.h"
#include "utility.h"

}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class StreamingFixture : public CommonFixture {
};

class StreamingFixtureWithContext : public CommonFixture {
    struct LDConfig *config;
    struct LDClient *client;

protected:
    struct StreamContext *context;

    void SetUp() override {
        CommonFixture::SetUp();
        LD_ASSERT(config = LDConfigNew("key"));
        LDConfigSetUseLDD(config, LDBooleanTrue);
        LDConfigSetSendEvents(config, LDBooleanFalse);
        LD_ASSERT(client = LDClientInit(config, 0));
        LD_ASSERT(context = LDi_constructStreamContext(client, NULL, NULL));
    }

    void TearDown() override {
        LDSSEParserDestroy(&context->parser);
        LDFree(context);
        LDClientClose(client);
        CommonFixture::TearDown();
    }
};

TEST_F(StreamingFixture, ParsePathFlags) {
    enum FeatureKind kind;
    const char *key;

    ASSERT_EQ(LDi_parsePath("/flags/abcd", &kind, &key), PARSE_PATH_SUCCESS);

    ASSERT_EQ(kind, LD_FLAG);
    ASSERT_STREQ(key, "abcd");
}

TEST_F(StreamingFixture, ParsePathSegments) {
    enum FeatureKind kind;
    const char *key;

    ASSERT_EQ(LDi_parsePath("/segments/xyz", &kind, &key), PARSE_PATH_SUCCESS);

    ASSERT_EQ(kind, LD_SEGMENT);
    ASSERT_STREQ(key, "xyz");
}

TEST_F(StreamingFixture, ParsePathUnknownKind) {
    enum FeatureKind kind;
    const char *key;

    ASSERT_EQ(LDi_parsePath("/unknown/123", &kind, &key), PARSE_PATH_IGNORE);

    ASSERT_FALSE(key);
}

TEST_F(StreamingFixtureWithContext, InitialPut) {
    struct LDJSONRC *flag, *segment;

    const char *const event =
            "event: put\n"
            "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
            "{\"key\": \"my-flag\", \"version\": 2}},\"segments\": "
            "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));

    ASSERT_TRUE(LDStoreGet(context->client->store, LD_FLAG, "my-flag", &flag));
    ASSERT_TRUE(flag);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(LDJSONRCGet(flag), "version")), 2);

    ASSERT_TRUE(
            LDStoreGet(context->client->store, LD_SEGMENT, "my-segment", &segment));
    ASSERT_TRUE(segment);
    ASSERT_EQ(
            LDGetNumber(LDObjectLookup(LDJSONRCGet(segment), "version")), 5);

    LDJSONRCDecrement(flag);
    LDJSONRCDecrement(segment);
}

TEST_F(StreamingFixtureWithContext, PatchFlag) {
    struct LDJSONRC *flag;

    const char *const event =
            "event: patch\n"
            "data: {\"path\": \"/flags/my-flag\", \"data\": "
            "{\"key\": \"my-flag\", \"version\": 3}}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));

    ASSERT_TRUE(LDStoreGet(context->client->store, LD_FLAG, "my-flag", &flag));
    ASSERT_TRUE(flag);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(LDJSONRCGet(flag), "version")), 3);

    LDJSONRCDecrement(flag);
}

TEST_F(StreamingFixtureWithContext, DeleteFlag) {
    struct LDJSONRC *lookup;

    const char *const event =
            "event: delete\n"
            "data: {\"path\": \"/flags/my-flag\", \"version\": 4}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));

    ASSERT_TRUE(LDStoreGet(context->client->store, LD_FLAG, "my-flag", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_F(StreamingFixtureWithContext, PatchSegment) {
    struct LDJSONRC *segment;

    const char *const event =
            "event: patch\n"
            "data: {\"path\": \"/segments/my-segment\", \"data\": "
            "{\"key\": \"my-segment\", \"version\": 7}}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));

    ASSERT_TRUE(
            LDStoreGet(context->client->store, LD_SEGMENT, "my-segment", &segment));
    ASSERT_TRUE(segment);
    ASSERT_EQ(
            LDGetNumber(LDObjectLookup(LDJSONRCGet(segment), "version")), 7);

    LDJSONRCDecrement(segment);
}

TEST_F(StreamingFixtureWithContext, DeleteSegment) {
    struct LDJSONRC *lookup;

    const char *const event =
            "event: delete\n"
            "data: {\"path\": \"/segments/my-segment\", \"version\": 8}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));

    ASSERT_TRUE(
            LDStoreGet(context->client->store, LD_SEGMENT, "my-segment", &lookup));
    ASSERT_FALSE(lookup);
}

TEST_F(StreamingFixtureWithContext, EventDataIsNotValidJSON) {
    const char *const event =
            "event: delete\n"
            "data: hello\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, EventDataIsNotAnObject) {
    const char *const event =
            "event: delete\n"
            "data: 123\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, DeleteWithoutPath) {
    const char *const event =
            "event: delete\n"
            "data: {\"version\": 8}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, DeletePathNotString) {
    const char *const event =
            "event: delete\n"
            "data: {\"path\": 123, \"version\": 8}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, DeletePathUnrecognized) {
    const char *const event =
            "event: delete\n"
            "data: {\"path\": \"hello\", \"version\": 8}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, DeleteMissingVersion) {
    const char *const event =
            "event: delete\n"
            "data: {\"path\": \"/flags/my-flag\"}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, DeleteVersionNotANumber) {
    const char *const event =
            "event: delete\n"
            "data: {\"path\": \"/flags/my-flag\", \"version\": \"test\"}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PatchInvalidPath) {
    const char *const event =
            "event: patch\n"
            "data: {\"path\": 123, \"data\": "
            "{\"key\": \"my-flag\", \"version\": 3}}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PatchMissingDataField) {
    const char *const event =
            "event: patch\n"
            "data: {\"path\": \"/flags/my-flag\"}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PutMissingDataField) {
    const char *const event =
            "event: put\n"
            "data: {\"path\": \"/\"}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PutDataNotAnObject) {
    const char *const event =
            "event: put\n"
            "data: {\"path\": \"/\", \"data\": 52}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PutDataMissingFlagsField) {
    const char *const event =
            "event: put\n"
            "data: {\"path\": \"/\", \"data\": {\"segments\": "
            "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PutDataFlagsNotAnObject) {
    const char *const event =
            "event: put\n"
            "data: {\"path\": \"/\", \"data\": {\"flags\": 123, \"segments\": "
            "{\"my-segment\": {\"key\": \"my-segment\", \"version\": 5}}}}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PutDataMissingSegmentsField) {
    const char *const event =
            "event: put\n"
            "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
            "{\"key\": \"my-flag\", \"version\": 2}}}}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, PutDataSegmentsNotAnObject) {
    const char *const event =
            "event: put\n"
            "data: {\"path\": \"/\", \"data\": {\"flags\": {\"my-flag\":"
            "{\"key\": \"my-flag\", \"version\": 2}},\"segments\": 52}}\n\n";

    ASSERT_FALSE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, SSEUnknownEventType) {
    const char *const event =
            "event: hello\n"
            "data: {}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, SSENoData) {
    const char *const event = "event: hello\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}

TEST_F(StreamingFixtureWithContext, SSENoEventType) {
    const char *const event = "data: {}\n\n";

    ASSERT_TRUE(LDi_streamWriteCallback(event, strlen(event), 1, context));
}
