#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>
#include "client.h"
#include "config.h"
#include "store.h"
#include "utility.h"
#include "json_internal_helpers.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"


#include "all_flags_state.h"

}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class AllFlagsStateFixture : public CommonFixture {
protected:
    struct LDClient* client;

    AllFlagsStateFixture(): client{nullptr} {}

    void SetUp() override {
        CommonFixture::SetUp();
        ASSERT_TRUE(client = makeTestClient());
    }

    void TearDown() override {
        LDClientClose(client);
        CommonFixture::TearDown();
    }
};


TEST_F(AllFlagsStateFixture, ValidState) {
    struct LDAllFlagsState *state;
    char *str;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));
    ASSERT_TRUE(LDi_allFlagsStateValid(state));

    ASSERT_TRUE(str = LDi_allFlagsStateJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":true,\"$flagsState\":{}}");

    LDi_freeAllFlagsState(state);
    LDFree(str);
}

TEST_F(AllFlagsStateFixture, InvalidState) {
    struct LDAllFlagsState *state;
    char *str;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanFalse));
    ASSERT_FALSE(LDi_allFlagsStateValid(state));

    ASSERT_TRUE(str = LDi_allFlagsStateJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":false,\"$flagsState\":{}}");

    LDi_freeAllFlagsState(state);
    LDFree(str);
}

TEST_F(AllFlagsStateFixture, GetFlag) {
    struct LDAllFlagsState *state;
    struct LDFlagState *flag;
    struct LDDetails *details;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));
    ASSERT_TRUE(flag = LDi_newFlagState("known-flag"));

    ASSERT_TRUE(LDi_allFlagsStateAdd(state, flag));;

    ASSERT_TRUE(details = LDi_allFlagsStateDetails(state, "known-flag"));
    ASSERT_EQ(details->reason, LD_UNKNOWN);
    ASSERT_EQ(details->hasVariation, LDBooleanFalse);
    ASSERT_EQ(details->variationIndex, 0);

    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, GetValueFlagDoesNotExist) {
    struct LDAllFlagsState *state;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));

    ASSERT_FALSE(LDi_allFlagsStateValue(state, "unknown-flag"));

    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, GetValueFlagExistsAndIsNull) {
    struct LDAllFlagsState *state;
    struct LDFlagState *flag;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));
    ASSERT_TRUE(flag = LDi_newFlagState("known-flag"));

    ASSERT_TRUE(LDi_allFlagsStateAdd(state, flag));;

    ASSERT_FALSE(LDi_allFlagsStateValue(state, "known-flag"));

    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, GetValueFlagExistAndIsNotNull) {
    struct LDAllFlagsState *state;
    struct LDFlagState *flag;

    ASSERT_TRUE(flag = LDi_newFlagState("known-flag"));
    ASSERT_TRUE(flag->value = LDNewObject());

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));
    ASSERT_TRUE(LDi_allFlagsStateAdd(state, flag));;

    ASSERT_EQ(flag->value, LDi_allFlagsStateValue(state, "known-flag"));

    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, ToValuesMapEmpty) {
    struct LDAllFlagsState *state;
    struct LDJSON *map;
    char *str;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));
    ASSERT_TRUE(map = LDi_allFlagsStateValuesMap(state));

    ASSERT_EQ(0, LDCollectionGetSize(map));

    ASSERT_TRUE(str = LDJSONSerialize(map));
    ASSERT_STREQ("{}", str);

    LDFree(str);
    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, ToValuesMap) {
    struct LDAllFlagsState* state;
    struct LDJSON *map;
    struct LDFlagState *flag;
    struct LDJSON *iter;
    char* str;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));

    ASSERT_TRUE(flag = LDi_newFlagState("flag1"));
    ASSERT_TRUE(flag->value = LDNewText("value1"));

    ASSERT_TRUE(LDi_allFlagsStateAdd(state, flag));;

    ASSERT_TRUE(map = LDi_allFlagsStateValuesMap(state));
    ASSERT_EQ(1, LDCollectionGetSize(map));

    for (iter = LDGetIter(map); iter; iter = LDIterNext(iter)) {
        ASSERT_STREQ("flag1", LDIterKey(iter));
        ASSERT_STREQ("value1", LDGetText(iter));
    }

    ASSERT_TRUE(str = LDJSONSerialize(map));
    ASSERT_STREQ("{\"flag1\":\"value1\"}", str);

    LDFree(str);
    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, ToValuesMapNullEvaluation) {
    struct LDAllFlagsState *state;
    struct LDJSON *map;
    struct LDFlagState *flag;
    struct LDJSON *iter;
    char *str;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));

    ASSERT_TRUE(flag = LDi_newFlagState("flag1"));
    ASSERT_EQ(flag->value, nullptr);

    ASSERT_TRUE(LDi_allFlagsStateAdd(state, flag));;

    ASSERT_TRUE(map = LDi_allFlagsStateValuesMap(state));
    ASSERT_EQ(1, LDCollectionGetSize(map));

    for (iter = LDGetIter(map); iter; iter = LDIterNext(iter)) {
        ASSERT_STREQ("flag1", LDIterKey(iter));
        ASSERT_EQ(LDNull, LDJSONGetType(iter));
    }

    ASSERT_TRUE(str = LDJSONSerialize(map));
    ASSERT_STREQ("{\"flag1\":null}", str);

    LDFree(str);
    LDi_freeAllFlagsState(state);
}



TEST_F(AllFlagsStateFixture, MinimalFlagJSON) {
    struct LDAllFlagsState *state;
    struct LDFlagState *flag;
    char *str;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));

    ASSERT_TRUE(flag = LDi_newFlagState("flag1"));
    ASSERT_TRUE(flag->value = LDNewText("value1"));
    flag->version = 1000;

    ASSERT_TRUE(LDi_allFlagsStateAdd(state, flag));;

    ASSERT_TRUE(str = LDi_allFlagsStateJSON(state));

    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":\"value1\",\"$flagsState\":{\"flag1\":{\"version\":1000}}}");

    LDFree(str);
    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, FlagWithAllPropertiesJSON) {
    struct LDAllFlagsState *state;
    struct LDFlagState *flag;
    char *str;

    ASSERT_TRUE(state = LDi_newAllFlagsState(LDBooleanTrue));

    ASSERT_TRUE(flag = LDi_newFlagState("flag1"));
    ASSERT_TRUE(flag->value = LDNewText("value1"));
    flag->version = 1000;
    flag->trackEvents = LDBooleanTrue;
    flag->debugEventsUntilDate = 100000;
    flag->details.hasVariation = LDBooleanTrue;
    flag->details.reason = LD_FALLTHROUGH;
    flag->details.variationIndex = 1;
    flag->details.extra.fallthrough.inExperiment = LDBooleanFalse;

    ASSERT_TRUE(LDi_allFlagsStateAdd(state, flag));;

    ASSERT_TRUE(str = LDi_allFlagsStateJSON(state));

    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":\"value1\",\"$flagsState\":{\"flag1\":{\"variation\":1,\"version\":1000,\"reason\":{\"kind\":\"FALLTHROUGH\"},\"trackEvents\":true,\"debugEventsUntilDate\":100000}}}");

    LDFree(str);
    LDi_freeAllFlagsState(state);
}

TEST_F(AllFlagsStateFixture, BuilderIsAlwaysValid) {
    struct LDAllFlagsBuilder *builder;
    struct LDAllFlagsState *state;

    ASSERT_TRUE(builder = LDi_newAllFlagsBuilder(LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(state = LDi_allFlagsBuilderBuild(builder));
    ASSERT_TRUE(LDi_allFlagsStateValid(state));

    LDi_freeAllFlagsState(state);
    LDi_freeAllFlagsBuilder(builder);
}

TEST_F(AllFlagsStateFixture, BuilderAddFlagsWithoutReasons) {
    struct LDAllFlagsBuilder *builder;
    struct LDAllFlagsState *state;
    struct LDFlagState *flag1, *flag2;
    char *str;

    ASSERT_TRUE(builder = LDi_newAllFlagsBuilder(LD_ALLFLAGS_DEFAULT));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDi_newFlagState("flag1"));
    ASSERT_TRUE(flag1->value = LDNewText("value1"));
    flag1->version = 1000;
    flag1->details.hasVariation = LDBooleanTrue;
    flag1->details.reason = LD_FALLTHROUGH;
    flag1->details.variationIndex = 1;
    flag1->details.extra.fallthrough.inExperiment = LDBooleanFalse;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag1));

    /* flag2 */
    ASSERT_TRUE(flag2 = LDi_newFlagState("flag2"));
    ASSERT_TRUE(flag2->value = LDNewText("value2"));
    flag2->version = 2000;
    flag2->trackEvents = LDBooleanTrue;
    flag2->debugEventsUntilDate = 100000;
    flag2->details.hasVariation = LDBooleanFalse;
    flag2->details.reason = LD_ERROR;
    flag2->details.variationIndex = 0;
    flag2->details.extra.errorKind = LD_STORE_ERROR;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag2));

    ASSERT_TRUE(state = LDi_allFlagsBuilderBuild(builder));
    ASSERT_TRUE(str = LDi_allFlagsStateJSON(state));


    /* Ensure that reason information does not end up in the JSON,
     * since 'LD_INCLUDE_REASON' was not passed into the builder. */
    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":\"value1\",\"flag2\":\"value2\",\"$flagsState\":{\"flag1\":{\"variation\":1,\"version\":1000},\"flag2\":{\"version\":2000,\"trackEvents\":true,\"debugEventsUntilDate\":100000}}}");

    LDFree(str);
    LDi_freeAllFlagsState(state);
    LDi_freeAllFlagsBuilder(builder);
}

// This test is intended to trigger valgrind errors if the flag detail prerequisiteKey is not freed properly
// by the builder.
TEST_F(AllFlagsStateFixture, BuilderAddFlagsWithoutReasonsMemoryLeak) {
    struct LDAllFlagsBuilder *builder;
    struct LDAllFlagsState *state;
    struct LDFlagState *flag1;
    char *str;

    ASSERT_TRUE(builder = LDi_newAllFlagsBuilder(LD_ALLFLAGS_DEFAULT));

    ASSERT_TRUE(flag1 = LDi_newFlagState("flag1"));
    flag1->version = 1000;
    flag1->details.reason = LD_PREREQUISITE_FAILED;
    ASSERT_TRUE(flag1->details.extra.prerequisiteKey = LDStrDup("prereq"));
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag1));

    ASSERT_TRUE(state = LDi_allFlagsBuilderBuild(builder));
    ASSERT_TRUE(str = LDi_allFlagsStateJSON(state));

    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":null,\"$flagsState\":{\"flag1\":{\"version\":1000}}}");

    LDFree(str);
    LDi_freeAllFlagsState(state);
    LDi_freeAllFlagsBuilder(builder);
}

TEST_F(AllFlagsStateFixture, BuilderAddFlagsWithReasons) {
    struct LDAllFlagsBuilder *builder;
    struct LDAllFlagsState *state;
    struct LDFlagState *flag1, *flag2;
    char *str;

    ASSERT_TRUE(builder = LDi_newAllFlagsBuilder(LD_INCLUDE_REASON));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDi_newFlagState("flag1"));
    ASSERT_TRUE(flag1->value = LDNewText("value1"));
    flag1->version = 1000;
    flag1->details.hasVariation = LDBooleanTrue;
    flag1->details.reason = LD_FALLTHROUGH;
    flag1->details.variationIndex = 1;
    flag1->details.extra.fallthrough.inExperiment = LDBooleanFalse;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag1));

    /* flag2 */
    ASSERT_TRUE(flag2 = LDi_newFlagState("flag2"));
    ASSERT_TRUE(flag2->value = LDNewText("value2"));
    flag2->version = 2000;
    flag2->trackEvents = LDBooleanTrue;
    flag2->debugEventsUntilDate = 100000;
    flag2->details.hasVariation = LDBooleanFalse;
    flag2->details.reason = LD_ERROR;
    flag2->details.variationIndex = 0;
    flag2->details.extra.errorKind = LD_STORE_ERROR;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag2));

    ASSERT_TRUE(state = LDi_allFlagsBuilderBuild(builder));
    ASSERT_TRUE(str = LDi_allFlagsStateJSON(state));

    /* Ensure that reason information does not end up in the JSON,
     * since 'LD_INCLUDE_REASON' was not passed into the builder. */
    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":\"value1\",\"flag2\":\"value2\",\"$flagsState\":{\"flag1\":{\"variation\":1,\"version\":1000,\"reason\":{\"kind\":\"FALLTHROUGH\"}},\"flag2\":{\"version\":2000,\"reason\":{\"kind\":\"ERROR\",\"errorKind\":\"STORE_ERROR\"},\"trackEvents\":true,\"debugEventsUntilDate\":100000}}}");

    LDFree(str);
    LDi_freeAllFlagsState(state);
    LDi_freeAllFlagsBuilder(builder);
}


TEST_F(AllFlagsStateFixture, BuilderAddFlagsWithReasonsOnlyIfTracked) {
    struct LDAllFlagsBuilder *builder;
    struct LDAllFlagsState *state;
    struct LDFlagState *flag1, *flag2, *flag3, *flag4;
    double now3, now4;

    ASSERT_TRUE(builder = LDi_newAllFlagsBuilder(LD_INCLUDE_REASON | LD_DETAILS_ONLY_FOR_TRACKED_FLAGS));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDi_newFlagState("flag1"));
    ASSERT_TRUE(flag1->value = LDNewText("value1"));
    flag1->version = 1000;
    flag1->details.hasVariation = LDBooleanTrue;
    flag1->details.reason = LD_FALLTHROUGH;
    flag1->details.variationIndex = 1;
    flag1->details.extra.fallthrough.inExperiment = LDBooleanFalse;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag1));

    /* flag2 */
    ASSERT_TRUE(flag2 = LDi_newFlagState("flag2"));
    ASSERT_TRUE(flag2->value = LDNewText("value2"));
    flag2->version = 2000;
    flag2->trackEvents = LDBooleanTrue;
    flag2->debugEventsUntilDate = 100000;
    flag2->details.hasVariation = LDBooleanFalse;
    flag2->details.reason = LD_ERROR;
    flag2->details.variationIndex = 0;
    flag2->details.extra.errorKind = LD_STORE_ERROR;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag2));

    /* flag3 */
    LDi_getMonotonicMilliseconds(&now3);
    now3 -= 1;

    ASSERT_TRUE(flag3 = LDi_newFlagState("flag3"));
    ASSERT_TRUE(flag3->value = LDNewText("value3"));
    flag3->version = 3000;
    flag3->debugEventsUntilDate = now3;
    flag3->details.hasVariation = LDBooleanTrue;
    flag3->details.reason = LD_FALLTHROUGH;
    flag3->details.variationIndex = 3;
    flag3->details.extra.fallthrough.inExperiment = LDBooleanFalse;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag3));

    /* flag4 */
    LDi_getMonotonicMilliseconds(&now4);
    now4 += 10000;

    ASSERT_TRUE(flag4 = LDi_newFlagState("flag4"));
    ASSERT_TRUE(flag4->value = LDNewText("value1"));
    flag4->version = 4000;
    flag4->debugEventsUntilDate = now4;
    flag4->details.hasVariation = LDBooleanTrue;
    flag4->details.reason = LD_FALLTHROUGH;
    flag4->details.variationIndex = 4;
    flag4->details.extra.fallthrough.inExperiment = LDBooleanFalse;
    ASSERT_TRUE(LDi_allFlagsBuilderAdd(builder, flag4));

    ASSERT_TRUE(state = LDi_allFlagsBuilderBuild(builder));

    ASSERT_EQ(LDi_allFlagsStateDetails(state, "flag1")->reason, LD_UNKNOWN);
    ASSERT_EQ(LDi_allFlagsStateDetails(state, "flag2")->reason, LD_ERROR);
    ASSERT_EQ(LDi_allFlagsStateDetails(state, "flag3")->reason, LD_UNKNOWN);
    ASSERT_EQ(LDi_allFlagsStateDetails(state, "flag4")->reason, LD_FALLTHROUGH);

    LDi_freeAllFlagsState(state);
    LDi_freeAllFlagsBuilder(builder);
}


/* Top-level API tests */

TEST_F(AllFlagsStateFixture, AllFlagsState_GetDetails) {
    struct LDAllFlagsState *state;
    struct LDJSON *inFlag;
    struct LDDetails *outDetails;
    struct LDUser *user;

    ASSERT_TRUE(inFlag = makeMinimalFlag("flag1", 1, LDBooleanTrue, LDBooleanTrue));

    setFallthrough(inFlag, 1);
    addVariation(inFlag, LDNewText("a"));
    addVariation(inFlag, LDNewText("b"));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, inFlag));

    ASSERT_TRUE(user = LDUserNew("user1"));
    ASSERT_TRUE(state = LDAllFlagsState(client, user, LD_INCLUDE_REASON));
    ASSERT_TRUE(LDAllFlagsStateValid(state));

    ASSERT_TRUE(outDetails = LDAllFlagsStateGetDetails(state, "flag1"));
    ASSERT_EQ(LDBooleanTrue, outDetails->hasVariation);
    ASSERT_EQ(1, outDetails->variationIndex);
    ASSERT_EQ(LD_FALLTHROUGH, outDetails->reason);


    LDUserFree(user);
    LDAllFlagsStateFree(state);
}

TEST_F(AllFlagsStateFixture, AllFlagsState_NullClientReturnsInvalidState) {
    struct LDUser *user;
    struct LDAllFlagsState* state;

    ASSERT_TRUE(user = LDUserNew("user1"));
    ASSERT_TRUE(state = LDAllFlagsState(nullptr, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_FALSE(LDAllFlagsStateValid(state));

    LDUserFree(user);
    LDAllFlagsStateFree(state);
}

TEST_F(AllFlagsStateFixture, AllFlagsState_NullUserReturnsInvalidState) {
    struct LDAllFlagsState *state;
    ASSERT_TRUE(state = LDAllFlagsState(client, nullptr, LD_ALLFLAGS_DEFAULT));
    ASSERT_FALSE(LDAllFlagsStateValid(state));

    LDAllFlagsStateFree(state);
}

/* This test is intended to trigger valgrind errors if forgetting to call LDAllFlagsStateFree would
 * leak memory when an invalid state is returned.
 * The test should not fail, since invalid AllFlagsState objects refer to a global object. */
TEST_F(AllFlagsStateFixture, AllFlagsState_CallerForgetsToFreeState) {
    struct LDAllFlagsState *state;
    ASSERT_TRUE(state = LDAllFlagsState(nullptr, nullptr, LD_ALLFLAGS_DEFAULT));
    ASSERT_FALSE(LDAllFlagsStateValid(state));

    /* forgotten: LDAllFlagsStateFree(state); */
}


TEST_F(AllFlagsStateFixture, AllFlagsState_InitializedStoreCreatesValidState) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    ASSERT_TRUE(user = LDUserNew("user1"));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(LDAllFlagsStateValid(state));

    LDUserFree(user);
    LDAllFlagsStateFree(state);
}

TEST_F(AllFlagsStateFixture, AllFlagsState_InitializedStoreCreatesValidJSON) {
    struct LDAllFlagsState *state;
    struct LDUser *user;
    char *str;

    ASSERT_TRUE(user = LDUserNew("user1"));
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(str = LDAllFlagsStateSerializeJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":true,\"$flagsState\":{}}");

    LDFree(str);
    LDUserFree(user);
    LDAllFlagsStateFree(state);
}

TEST_F(AllFlagsStateFixture, AllFlagsState_GetValue) {
    struct LDAllFlagsState *state;
    struct LDJSON *inFlag;
    struct LDJSON *outFlag;
    struct LDUser *user;
    char *str;


    ASSERT_TRUE(inFlag = makeMinimalFlag("flag1", 1, LDBooleanTrue, LDBooleanFalse));

    setFallthrough(inFlag, 1);
    addVariation(inFlag, LDNewText("a"));
    addVariation(inFlag, LDNewText("b"));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, inFlag));

    ASSERT_TRUE(user = LDUserNew("user1"));
    ASSERT_TRUE(state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(LDAllFlagsStateValid(state));

    ASSERT_TRUE(outFlag = LDAllFlagsStateGetValue(state, "flag1"));

    ASSERT_TRUE(str = LDJSONSerialize(outFlag));

    ASSERT_STREQ(str, "\"b\"");

    LDFree(str);
    LDUserFree(user);
    LDAllFlagsStateFree(state);
}

TEST_F(AllFlagsStateFixture, AllFlagsState_GivesSameResultAsAllFlags) {
    struct LDJSON *allFlagsResult;
    struct LDAllFlagsState *allFlagsState;
    struct LDJSON *allFlagsStateMapResult;
    struct LDUser *user;
    char *allFlagsStr, *allFlagsStateStr;

    struct LDJSON *flag1, *flag2, *flag3, *flag4;

    std::vector<LDJSON*> flags;

    /* flag1 */
    ASSERT_TRUE(flag1 = makeMinimalFlag("flag1", 1, LDBooleanTrue, LDBooleanFalse));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));
    flags.push_back(flag1);

    /* flag2 */
    ASSERT_TRUE(flag2 = makeMinimalFlag("flag2", 2, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag2, 1);
    addVariation(flag2, LDNewText("a"));
    addVariation(flag2, LDNewText("b"));
    flags.push_back(flag2);

    /* flag3 */
    ASSERT_TRUE(flag3 = makeMinimalFlag("flag3", 3, LDBooleanFalse, LDBooleanTrue));
    LDObjectSetKey(flag3, "offVariation", LDNewNumber(0));
    addVariation(flag3, LDNewText("off"));
    flags.push_back(flag3);

    /* flag4 */
    ASSERT_TRUE(flag4 = makeMinimalFlag("flag4", 4, LDBooleanFalse, LDBooleanFalse));
    LDObjectSetKey(flag4, "offVariation", LDNewNumber(0));
    addVariation(flag4, LDNewText("off"));
    flags.push_back(flag4);

    ASSERT_TRUE(LDStoreInitEmpty(client->store));

    for (LDJSON* flag : flags) {
        ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag));
    }

    ASSERT_TRUE(user = LDUserNew("user1"));

    /* Obtain map from key -> value via AllFlagsState */
    ASSERT_TRUE(allFlagsState = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(allFlagsStateMapResult = LDAllFlagsStateToValuesMap(allFlagsState));
    ASSERT_TRUE(allFlagsStateStr = LDJSONSerialize(allFlagsStateMapResult));

    /* Obtain map from key -> value via deprecated LDAllFlags. They should be equivalent. */
    ASSERT_TRUE(allFlagsResult = LDAllFlags(client, user));
    ASSERT_TRUE(allFlagsStr = LDJSONSerialize(allFlagsResult));

    ASSERT_STREQ(allFlagsStr, allFlagsStateStr);

    LDFree(allFlagsStr);
    LDFree(allFlagsStateStr);

    LDJSONFree(allFlagsResult);

    LDUserFree(user);
    LDAllFlagsStateFree(allFlagsState);
}

TEST_F(AllFlagsStateFixture, AllFlagsState_ReturnsAllValidFlags) {
    struct LDJSON *flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *user;
    struct LDAllFlagsState *allFlagsState;


    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("flag1")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("flag2")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    setFallthrough(flag2, 1);
    addVariation(flag2, LDNewText("c"));
    addVariation(flag2, LDNewText("d"));

    /* store */
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag1));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag2));

    /* test */
    ASSERT_TRUE(allFlagsState = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(allFlags = LDAllFlagsStateToValuesMap(allFlagsState));

    /* validation */
    ASSERT_EQ(LDCollectionGetSize(allFlags), 2);
    ASSERT_STREQ(LDGetText(LDObjectLookup(allFlags, "flag1")), "b");
    ASSERT_STREQ(LDGetText(LDObjectLookup(allFlags, "flag2")), "d");

    /* cleanup */
    LDAllFlagsStateFree(allFlagsState);
    LDUserFree(user);
    LDClientClose(client);
}

/* If there is a problem with a single flag, then that should not prevent returning other flags.
 * In this test one of the flags will have an invalid fallthrough that contains neither a variation
 * nor rollout.
 */
TEST_F(AllFlagsStateFixture, AllFlagsState_InvalidFlagDoesNotPreventValidFlagFromBeingReturned) {
    struct LDJSON *flag1, *flag2, *allFlags;
    struct LDClient *client;
    struct LDUser *user;
    struct LDAllFlagsState *allFlagsState;


    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("flag1")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    setFallthrough(flag1, 1);
    addVariation(flag1, LDNewText("a"));
    addVariation(flag1, LDNewText("b"));

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("flag2")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));

    /* Set a fallthrough which has no variation or rollout. */
    struct LDJSON *fallthrough;
    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "fallthrough", fallthrough));

    /* store */
    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag1));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag2));

    /* test */
    ASSERT_TRUE(allFlagsState = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(allFlags = LDAllFlagsStateToValuesMap(allFlagsState));

    /* validation */
    ASSERT_EQ(LDCollectionGetSize(allFlags), 1);
    ASSERT_STREQ(LDGetText(LDObjectLookup(allFlags, "flag1")), "b");

    /* cleanup */
    LDAllFlagsStateFree(allFlagsState);
    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(AllFlagsStateFixture, AllFlagsState_NoFlagsInStore) {
    struct LDJSON *allFlags;
    struct LDClient *client;
    struct LDUser *user;
    struct LDAllFlagsState *allFlagsState;

    ASSERT_TRUE(client = makeTestClient());
    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* store */
    ASSERT_TRUE(LDStoreInitEmpty(client->store));

    /* test */
    ASSERT_TRUE(allFlagsState = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(allFlags = LDAllFlagsStateToValuesMap(allFlagsState));

    /* validation */
    ASSERT_EQ(LDCollectionGetSize(allFlags), 0);

    /* cleanup */
    LDAllFlagsStateFree(allFlagsState);
    LDUserFree(user);
    LDClientClose(client);
}
