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

// If the store is uninitialized, LDAllFlagsState returns a value that implements something similar to the
// Null Object Pattern, rather than NULL. The LDAllFlagState's methods are well-defined, and the user can
// detect that the state is valid/invalid via LDAllFlagsStateValid.
TEST_F(AllFlagsStateFixture, InvalidStateIfStoreIsUninitialized) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    user = LDUserNew("foo");

    ASSERT_TRUE(state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_FALSE(LDAllFlagsStateValid(state));

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies that the invalid AllFlagsState can be serialized into a well-defined JSON object.
// The '$valid' key can be checked by downstream code (such as a web-frontend) to determine if error handling
// needs to take place.
TEST_F(AllFlagsStateFixture, InvalidStateSerializesToWellDefinedJSON) {
    struct LDAllFlagsState *state;
    struct LDUser *user;
    char *json;

    user = LDUserNew("foo");

    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);
    ASSERT_TRUE(json = LDAllFlagsStateSerializeJSON(state));
    ASSERT_STREQ(json, "{\"$valid\":false,\"$flagsState\":{}}");

    LDFree(json);
    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies that LDAllFlagsStateGetValue returns a well-defined NULL (instead of crashing)
// in the case of invalid state.
TEST_F(AllFlagsStateFixture, InvalidStateGetValueReturnsNULL) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    user = LDUserNew("foo");

    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);
    ASSERT_FALSE(LDAllFlagsStateGetValue(state, "key"));

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies that LDAllFlagsStateGetDetails returns a well-defined NULL (instead of crashing)
// in the case of invalid state.
TEST_F(AllFlagsStateFixture, InvalidStateGetDetailsReturnsNULL) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    user = LDUserNew("foo");

    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);
    ASSERT_FALSE(LDAllFlagsStateGetDetails(state, "key"));

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies that LDAllFlagsState returns a valid object if the store is initialized.
TEST_F(AllFlagsStateFixture, ValidStateIfStoreInitializedAsEmpty) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    user = LDUserNew("foo");

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT));
    ASSERT_TRUE(LDAllFlagsStateValid(state));

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies that if the store is initialized as empty, LDAllFlagsState returns a valid object
// with a well-defined JSON representation.
TEST_F(AllFlagsStateFixture, ValidEmptyStateSerializesToWellDefinedJSON) {
    struct LDAllFlagsState *state;
    struct LDUser *user;
    char *str;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);

    ASSERT_TRUE(str = LDAllFlagsStateSerializeJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":true,\"$flagsState\":{}}");

    LDAllFlagsStateFree(state);
    LDFree(str);
    LDUserFree(user);
}

static LDJSON*
boolFlagOn(const char *key)
{
    struct LDJSON *flag;
    flag = LDNewObject();
    LDObjectSetKey(flag, "key", LDNewText(key));
    LDObjectSetKey(flag, "version", LDNewNumber(1));
    LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue));
    LDObjectSetKey(flag, "salt", LDNewText("abc"));
    LDObjectSetKey(flag, "offVariation", LDNewNumber(1));
    addVariation(flag, LDNewBool(LDBooleanTrue));
    addVariation(flag, LDNewBool(LDBooleanFalse));
    setFallthrough(flag, 0);
    return flag;
}

static LDJSON*
boolFlagOff(const char *key)
{
    struct LDJSON *flag;
    flag = LDNewObject();
    LDObjectSetKey(flag, "key", LDNewText(key));
    LDObjectSetKey(flag, "version", LDNewNumber(1));
    LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse));
    LDObjectSetKey(flag, "offVariation", LDNewNumber(1));
    LDObjectSetKey(flag, "salt", LDNewText("def"));
    addVariation(flag, LDNewBool(LDBooleanTrue));
    addVariation(flag, LDNewBool(LDBooleanFalse));
    setFallthrough(flag, 1);
    return flag;
}


static LDJSON*
flagWithPrerequisite(const char *key, const char *prereqKey)
{
    struct LDJSON *flag = LDNewObject();
    LDObjectSetKey(flag, "key", LDNewText(key));
    LDObjectSetKey(flag, "version", LDNewNumber(1));
    LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue));
    LDObjectSetKey(flag, "offVariation", LDNewNumber(1));
    LDObjectSetKey(flag, "salt", LDNewText("abc"));

    struct LDJSON *prereqs = LDNewArray();
    struct LDJSON *prereq = LDNewObject();
    LDObjectSetKey(prereq, "key", LDNewText(prereqKey));
    LDObjectSetKey(prereq, "variation", LDNewNumber(1));

    LDArrayPush(prereqs, prereq);

    LDObjectSetKey(flag, "prerequisites", prereqs);

    addVariation(flag, LDNewBool(LDBooleanTrue));
    addVariation(flag, LDNewBool(LDBooleanFalse));
    setFallthrough(flag, 0);
    return flag;
}

// This scenario checks that the default serialization of a flag contains both the 'variation' and 'version' keys,
// within $flagState, as well as the flag's key and value.
TEST_F(AllFlagsStateFixture, ValidFlagSerializesToWellDefinedJSON_Default) {
    struct LDAllFlagsState* state;
    struct LDUser *user;
    char* str;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, boolFlagOn("flag1"));

    state = LDAllFlagsState(client, user,  LD_ALLFLAGS_DEFAULT);
    ASSERT_TRUE(str = LDAllFlagsStateSerializeJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":true,\"$flagsState\":{\"flag1\":{\"variation\":0,\"version\":1}}}");
    LDFree(str);

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// This scenario checks that if LD_INCLUDE_REASON is specified, the serialized flag state should contain the flag's
// evaluation reason.
TEST_F(AllFlagsStateFixture, ValidFlagSerializesToWellDefinedJSON_IncludeReason) {
    struct LDAllFlagsState* state;
    struct LDUser *user;
    char* str;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, boolFlagOn("flag1"));

    state = LDAllFlagsState(client, user,  LD_INCLUDE_REASON);
    ASSERT_TRUE(str = LDAllFlagsStateSerializeJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":true,\"$flagsState\":{\"flag1\":{\"variation\":0,\"version\":1,\"reason\":{\"kind\":\"FALLTHROUGH\"}}}}");
    LDFree(str);

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// This scenario checks that if DETAILS_ONLY_FOR_TRACKED_FLAGS is specified, the 'version' key is omitted.
TEST_F(AllFlagsStateFixture, ValidFlagSerializesToWellDefinedJSON_DetailsOnlyForTrackedFlags) {
    struct LDAllFlagsState* state;
    struct LDUser *user;
    char* str;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, boolFlagOn("flag1"));

    state = LDAllFlagsState(client, user,  LD_DETAILS_ONLY_FOR_TRACKED_FLAGS);
    ASSERT_TRUE(str = LDAllFlagsStateSerializeJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":true,\"$flagsState\":{\"flag1\":{\"variation\":0}}}");
    LDFree(str);

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}


// This scenario checks that if both LD_INCLUDE_REASON and DETAILS_ONLY_FOR_TRACKED_FLAGS are specified,
// the reason will be omitted since the flag is not tracked.
TEST_F(AllFlagsStateFixture, ValidFlagSerializesToWellDefinedJSON_IncludeReasonAndDetailsOnlyForTrackedFlags) {
    struct LDAllFlagsState* state;
    struct LDUser *user;
    char* str;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, boolFlagOn("flag1"));

    state = LDAllFlagsState(client, user,  LD_INCLUDE_REASON | LD_DETAILS_ONLY_FOR_TRACKED_FLAGS);
    ASSERT_TRUE(str = LDAllFlagsStateSerializeJSON(state));
    ASSERT_STREQ(str, "{\"$valid\":true,\"flag1\":true,\"$flagsState\":{\"flag1\":{\"variation\":0}}}");
    LDFree(str);

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}


// Verifies that the values map (flag key -> flag value) representing an empty, but valid LDAllFlagsState, serializes
// to a well-defined JSON string (which is simply an empty object.)
TEST_F(AllFlagsStateFixture, ValidEmptyStateSerializesValueMapToWellDefinedJSON) {
    struct LDAllFlagsState *state;
    struct LDUser *user;
    char *str;
    struct LDJSON *json;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);

    ASSERT_TRUE(json = LDAllFlagsStateToValuesMap(state));
    ASSERT_TRUE(str = LDJSONSerialize(json));
    ASSERT_STREQ(str, "{}");

    LDFree(str);
    LDUserFree(user);
    LDAllFlagsStateFree(state);
}



static LDJSON*
malformedFlag(const char *key, unsigned int variation)
{
    struct LDJSON *flag;
    flag = LDNewObject();
    LDObjectSetKey(flag, "key", LDNewText(key));
    LDObjectSetKey(flag, "version", LDNewNumber(1));
    LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue));
    LDObjectSetKey(flag, "salt", LDNewText("abc"));
    setFallthrough(flag, variation);
    return flag;
}


// Verifies that if a flag is present in the store, then it is accessible via GetDetails/GetValue.
// Neither the details nor the value need to be explicitly freed, since they are references into the
// LDAllFlagsState object.
TEST_F(AllFlagsStateFixture, GetFlagDetailsAndValue) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, boolFlagOn("flag1"));

    user = LDUserNew("foo");
    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);

    ASSERT_TRUE(LDAllFlagsStateGetDetails(state, "flag1"));
    ASSERT_TRUE(LDAllFlagsStateGetValue(state, "flag1"));

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies that if a flag doesn't exist in the store, then it won't be accessible via GetDetails/GetValue.
TEST_F(AllFlagsStateFixture, GetNonexistentFlagFails) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    LDStoreInitEmpty(client->store);

    user = LDUserNew("foo");
    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);

    ASSERT_FALSE(LDAllFlagsStateGetDetails(state, "flag_true"));
    ASSERT_FALSE(LDAllFlagsStateGetValue(state, "flag_true"));

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}


// Verifies that if a flag is malformed such that it has a NULL value, it will still be present in the
// AllFlagsState object, but GetValue will return NULL.
TEST_F(AllFlagsStateFixture, GetFlagWithNullValue) {
    struct LDAllFlagsState *state;
    struct LDUser *user;

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, malformedFlag("flag_true", 1));

    user = LDUserNew("foo");
    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);

    ASSERT_TRUE(LDAllFlagsStateGetDetails(state, "flag_true"));
    ASSERT_FALSE(LDAllFlagsStateGetValue(state, "flag_true"));

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies a simple scenario of values map serialization with two flags, both having non-null boolean values.
TEST_F(AllFlagsStateFixture, ValuesMapSerializationWithoutNull) {
    struct LDAllFlagsState* state;
    struct LDJSON *map;
    struct LDUser *user;
    char* str;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, boolFlagOn("flag1"));
    LDStoreUpsert(client->store, LD_FLAG, boolFlagOff("flag2"));

    state = LDAllFlagsState(client, user,  LD_ALLFLAGS_DEFAULT);

    map = LDAllFlagsStateToValuesMap(state);

    ASSERT_TRUE(str = LDJSONSerialize(map));
    ASSERT_STREQ(str, "{\"flag1\":true,\"flag2\":false}");
    LDFree(str);

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}

// Verifies a scenario in which a flag has a NULL value. The NULL should be rendered in the JSON serialization
// of the values map.
TEST_F(AllFlagsStateFixture, ValuesMapSerializationWithNull) {
    struct LDAllFlagsState* state;
    struct LDJSON *map;
    struct LDUser *user;
    char* str;

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, malformedFlag("flag1", 1));

    state = LDAllFlagsState(client, user,  LD_ALLFLAGS_DEFAULT);

    map = LDAllFlagsStateToValuesMap(state);

    ASSERT_TRUE(str = LDJSONSerialize(map));
    ASSERT_STREQ(str, "{\"flag1\":null}");
    LDFree(str);

    LDAllFlagsStateFree(state);
    LDUserFree(user);
}



// This test was created because of a bug in the handling of the LDDetails structure.

// If a flag fails evaluation due to a prerequisite, that prereq's key is cloned and then stored in the details object.
// When specifying LD_ALLFLAGS_DEFAULT, this reason object is wiped out internally - and it must take care to clean
// out any allocated values.
//
// This test only has value if the test harness is running under Valgrind's memory checker, because there's
// no assertion that will trigger failure otherwise.
TEST_F(AllFlagsStateFixture, BuilderAddFlagsWithoutReasonsMemoryLeak) {
    struct LDAllFlagsState* state;
    struct LDUser *user;
    struct LDJSON *flag, *req;

    req = boolFlagOff("req1");
    flag = boolFlagOn("flag1");
    addPrerequisite(flag, req, 0);

    user = LDUserNew("foo");

    LDStoreInitEmpty(client->store);
    LDStoreUpsert(client->store, LD_FLAG, req);
    LDStoreUpsert(client->store, LD_FLAG, flag);

    state = LDAllFlagsState(client, user,  LD_INCLUDE_REASON);

    // This is a meta-check to be sure that this test's flags were setup correctly. In other words,
    // it could be commented out if we could guarantee that the flag configuration was correct in some other way.
    struct LDDetails *details = LDAllFlagsStateGetDetails(state, "flag1");
    ASSERT_EQ(details->reason, LD_PREREQUISITE_FAILED);
    ASSERT_STREQ(details->extra.prerequisiteKey, "req1");
    LDAllFlagsStateFree(state);
    // End meta-check.


    state = LDAllFlagsState(client, user, LD_ALLFLAGS_DEFAULT);  /* Allocates.. */
    LDAllFlagsStateFree(state); /* Frees.. */

    // At this point all memory associated with 'state' should be freed. If valgrind detects that this isn't the case,
    // it will fail the test.

    LDUserFree(user);
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
