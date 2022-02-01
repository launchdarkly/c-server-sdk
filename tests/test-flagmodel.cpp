#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>
#include "flag_model.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class FlagModelFixture : public CommonFixture {
};


TEST_F(FlagModelFixture, HandlesOldClientSideSchema_ClientSideTrue) {
    struct LDJSON *json;
    struct LDFlagModel model;
    const char* str =
            "{"
            "\"key\": \"flag\","
            "\"clientSide\": true"
            "}";

    ASSERT_TRUE(json = LDJSONDeserialize(str));

    LDi_initFlagModel(&model, json);

    ASSERT_TRUE(model.clientSideAvailability.usingEnvironmentID);
    ASSERT_TRUE(model.clientSideAvailability.usingMobileKey);
    ASSERT_FALSE(model.clientSideAvailability.usingExplicitSchema);

    LDJSONFree(json);
}

TEST_F(FlagModelFixture, HandlesOldClientSideSchema_ClientSideFalse) {
    struct LDJSON *json;
    struct LDFlagModel model;
    const char* str =
            "{"
            "\"key\": \"flag\","
            "\"clientSide\": false"
            "}";

    ASSERT_TRUE(json = LDJSONDeserialize(str));

    LDi_initFlagModel(&model, json);

    ASSERT_FALSE(model.clientSideAvailability.usingEnvironmentID);
    ASSERT_TRUE(model.clientSideAvailability.usingMobileKey);
    ASSERT_FALSE(model.clientSideAvailability.usingExplicitSchema);

    LDJSONFree(json);
}

TEST_F(FlagModelFixture, HandlesNewClientSideSchema_UsingEnvironmentIdTrue) {
    struct LDJSON *json;
    struct LDFlagModel model;
    const char* str =
            "{"
            "\"key\": \"flag\","
            "\"clientSideAvailability\": {"
            "\"usingEnvironmentId\": true,"
            "\"usingMobileKey\": false"
            "}}";

    ASSERT_TRUE(json = LDJSONDeserialize(str));
    LDi_initFlagModel(&model, json);

    ASSERT_TRUE(model.clientSideAvailability.usingEnvironmentID);
    ASSERT_FALSE(model.clientSideAvailability.usingMobileKey);
    ASSERT_TRUE(model.clientSideAvailability.usingExplicitSchema);

    LDJSONFree(json);
}

TEST_F(FlagModelFixture, HandlesNewClientSideSchema_UsingEnvironmentIdFalse) {
    struct LDJSON *json;
    struct LDFlagModel model;
    const char* str =
            "{"
            "\"key\": \"flag\","
            "\"clientSideAvailability\": {"
            "\"usingEnvironmentId\": false,"
            "\"usingMobileKey\": false"
            "}}";

    ASSERT_TRUE(json = LDJSONDeserialize(str));
    LDi_initFlagModel(&model, json);

    ASSERT_FALSE(model.clientSideAvailability.usingEnvironmentID);
    ASSERT_FALSE(model.clientSideAvailability.usingMobileKey);
    ASSERT_TRUE(model.clientSideAvailability.usingExplicitSchema);

    LDJSONFree(json);
}

TEST_F(FlagModelFixture, HandlesExpectedFields) {
    struct LDJSON *json;
    struct LDFlagModel model;
    const char* str =
            "{"
            "\"key\": \"flag\","
            "\"version\": 10,"
            "\"trackEvents\": true,"
            "\"debugEventsUntilDate\": 100000,"
            "\"clientSideAvailability\": {"
            "\"usingEnvironmentId\": false,"
            "\"usingMobileKey\": true"
            "}}";

    ASSERT_TRUE(json = LDJSONDeserialize(str));
    LDi_initFlagModel(&model, json);

    ASSERT_STREQ("flag", model.key);
    ASSERT_EQ(10, model.version);
    ASSERT_TRUE(model.trackEvents);
    ASSERT_EQ(100000, model.debugEventsUntilDate);

    ASSERT_FALSE(model.clientSideAvailability.usingEnvironmentID);
    ASSERT_TRUE(model.clientSideAvailability.usingMobileKey);
    ASSERT_TRUE(model.clientSideAvailability.usingExplicitSchema);

    LDJSONFree(json);
}
