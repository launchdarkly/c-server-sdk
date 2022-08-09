#include "gtest/gtest.h"
#include "commonfixture.h"
#include <vector>
#include <mutex>

extern "C" {
#include <float.h>
#include <math.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "evaluate.h"
#include "store.h"
#include "test-utils/flags.h"
#include "utility.h"
}

static std::vector<std::string> logMessages;
static std::mutex logMessagesMutex;

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class EvalFixture : public CommonFixture {
    void SetUp() override {
        CommonFixture::SetUp();
        LDConfigureGlobalLogger(LD_LOG_TRACE, [](const LDLogLevel level, const char *const message){
            std::lock_guard<std::mutex> guard(logMessagesMutex);
            logMessages.push_back(std::string(message));
        });
    }

public:
    void ResetLogs() {
        std::lock_guard<std::mutex> guard(logMessagesMutex);
        logMessages.clear();
    }

    size_t LogCount() {
        std::lock_guard<std::mutex> guard(logMessagesMutex);
        return logMessages.size();
    }
};

static struct LDStore *
prepareEmptyStore() {
    struct LDStore *store;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew(""));
    LD_ASSERT(store = LDStoreNew(config));
    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(LDStoreInitEmpty(store));
    LD_ASSERT(LDStoreInitialized(store));

    LDConfigFree(config);

    return store;
}

static void
addPrerequisite(
        struct LDJSON *const flag,
        const char *const key,
        const unsigned int variation) {
    struct LDJSON *tmp, *prerequisites;

    if (!(prerequisites = LDObjectLookup(flag, "prerequisites"))) {
        LD_ASSERT(prerequisites = LDNewArray());
        LDObjectSetKey(flag, "prerequisites", prerequisites);
    }

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(tmp, "key", LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(tmp, "variation", LDNewNumber(variation)));

    LD_ASSERT(LDArrayPush(prerequisites, tmp));
}

static struct LDJSON *
booleanFlagWithClause(struct LDJSON *const clause) {
    struct LDJSON *flag, *rule, *clauses, *rules;

    LD_ASSERT(clauses = LDNewArray());
    LD_ASSERT(LDArrayPush(clauses, clause));

    LD_ASSERT(rule = LDNewObject());
    LD_ASSERT(LDObjectSetKey(rule, "id", LDNewText("rule-id")));
    LD_ASSERT(LDObjectSetKey(rule, "clauses", clauses));
    LD_ASSERT(LDObjectSetKey(rule, "variation", LDNewNumber(1)));

    LD_ASSERT(rules = LDNewArray());
    LD_ASSERT(LDArrayPush(rules, rule));

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText("feature")));
    LD_ASSERT(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    LD_ASSERT(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "rules", rules));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewBool(LDBooleanFalse));
    addVariation(flag, LDNewBool(LDBooleanTrue));

    return flag;
}

TEST_F(EvalFixture, ReturnsOffVariationIfFlagIsOff) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    addVariations1(flag);
    setFallthrough(flag, 0);

    /* run */
    ASSERT_EQ(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse), EVAL_MISS);

    /* validation */
    ASSERT_STREQ("off", LDGetText(result));
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 1);
    ASSERT_EQ(details.reason, LD_OFF);
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, ReturnsCorrectReasonWhenOffAndOffVariationNull) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNull()));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    addVariations1(flag);
    setFallthrough(flag, 0);

    ResetLogs();
    /* run */
    ASSERT_EQ(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse), EVAL_MISS);

    /* validation */
    ASSERT_EQ(LogCount(), 0); // No messages should have been logged.
    ASSERT_EQ(details.reason, LD_OFF);
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, FlagReturnsNilIfFlagIsOffAndOffVariantIsUnspecified) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 0);
    addVariations1(flag);

    ResetLogs();
    /* run */
    ASSERT_EQ(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse), EVAL_MISS);

    /* validation */
    ASSERT_FALSE(result);
    ASSERT_FALSE(details.hasVariation);
    ASSERT_EQ(details.reason, LD_OFF);
    ASSERT_FALSE(events);
    ASSERT_EQ(LogCount(), 0); // Unspecified is valid, there should be no logs.

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, FlagReturnsFallthroughIfFlagIsOnAndThereAreNoRules) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    ASSERT_TRUE(config = LDConfigNew("abc"));
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "rules", LDNewArray()));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 0);
    addVariations1(flag);

    /* run */
    ASSERT_EQ(
            LDi_evaluate(
                    client,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse), EVAL_MATCH);

    /* validate */
    ASSERT_STREQ(LDGetText(result), "fall");
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 0);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

TEST_F(EvalFixture, FlagReturnsErrorForFallthroughWithNoVariationAndNoRollout) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    ASSERT_TRUE(config = LDConfigNew("abc"));
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "rules", LDNewArray()));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));

    /* Set a fallthrough which has no variation or rollout. */
    struct LDJSON *fallthrough;
    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "fallthrough", fallthrough));

    ResetLogs();
    /* run */
    ASSERT_EQ(
            LDi_evaluate(
                    client,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse), EVAL_SCHEMA);

    ASSERT_FALSE(details.hasVariation);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    ASSERT_FALSE(events);
    ASSERT_FALSE(result);
    ASSERT_GE(LogCount(), 1);
    LDJSONFree(flag);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

TEST_F(EvalFixture, FlagReturnsOffVariationIfPrerequisiteIsOff) {
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    ASSERT_TRUE(config = LDConfigNew("abc"));
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    addVariations2(flag2);

    /* store setup */
    ASSERT_TRUE(store = prepareEmptyStore());
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, flag2));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            client,
            flag1,
            user,
            store,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_STREQ(LDGetText(result), "off");
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 1);
    ASSERT_EQ(details.reason, LD_PREREQUISITE_FAILED);
    ASSERT_STREQ("feature1", details.extra.prerequisiteKey);

    ASSERT_TRUE(events);
    ASSERT_EQ(LDCollectionGetSize(events), 1);
    ASSERT_TRUE(eventsiter = LDGetIter(events));
    ASSERT_STREQ("feature1", LDGetText(LDObjectLookup(eventsiter, "key")));
    ASSERT_STREQ("go", LDGetText(LDObjectLookup(eventsiter, "value")));
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "version")), 3);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "variation")), 1);
    ASSERT_STREQ("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf")));

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

TEST_F(EvalFixture, FlagReturnsOffVariationIfPrerequisiteIsNotMet) {
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    ASSERT_TRUE(config = LDConfigNew("abc"));
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(2)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    addVariations2(flag2);
    setFallthrough(flag2, 0);

    /* store */
    ASSERT_TRUE(store = prepareEmptyStore());
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, flag2));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            client,
            flag1,
            user,
            store,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_STREQ(LDGetText(result), "off");
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 1);
    ASSERT_EQ(details.reason, LD_PREREQUISITE_FAILED);

    ASSERT_TRUE(events);
    ASSERT_EQ(LDCollectionGetSize(events), 1);
    ASSERT_TRUE(eventsiter = LDGetIter(events));
    ASSERT_STREQ("feature1", LDGetText(LDObjectLookup(eventsiter, "key")));
    ASSERT_STREQ("nogo", LDGetText(LDObjectLookup(eventsiter, "value")));
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "version")), 2);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "variation")), 0);
    ASSERT_TRUE(
            strcmp("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf"))) ==
            0);

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

TEST_F(EvalFixture, FlagReturnsFallthroughVariationIfPrerequisiteIsMetAndThereAreNoRules) {
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    ASSERT_TRUE(config = LDConfigNew("abc"));
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    setFallthrough(flag2, 1);
    addVariations2(flag2);

    /* store */
    ASSERT_TRUE(store = prepareEmptyStore());
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, flag2));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            client,
            flag1,
            user,
            store,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_STREQ(LDGetText(result), "fall");
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 0);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);

    ASSERT_TRUE(events);
    ASSERT_EQ(LDCollectionGetSize(events), 1);
    ASSERT_TRUE(eventsiter = LDGetIter(events));
    ASSERT_STREQ("feature1", LDGetText(LDObjectLookup(eventsiter, "key")));
    ASSERT_STREQ("go", LDGetText(LDObjectLookup(eventsiter, "value")));
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "version")), 3);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "variation")), 1);
    ASSERT_STREQ("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf")));

    LDJSONFree(flag1);
    LDJSONFree(result);
    LDJSONFree(events);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

TEST_F(EvalFixture, MultipleLevelsOfPrerequisiteProduceMultipleEvents) {
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag1, *flag2, *flag3, *result, *events, *eventsiter;
    struct LDDetails details;
    struct LDClient *client;
    struct LDConfig *config;

    events = NULL;
    result = NULL;

    LDDetailsInit(&details);
    ASSERT_TRUE(config = LDConfigNew("abc"));
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag1 */
    ASSERT_TRUE(flag1 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag1, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag1, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag1, "salt", LDNewText("abc")));
    addPrerequisite(flag1, "feature1", 1);
    setFallthrough(flag1, 0);
    addVariations1(flag1);

    /* flag2 */
    ASSERT_TRUE(flag2 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag2, "key", LDNewText("feature1")));
    ASSERT_TRUE(LDObjectSetKey(flag2, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "version", LDNewNumber(3)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag2, "salt", LDNewText("abc")));
    addPrerequisite(flag2, "feature2", 1);
    setFallthrough(flag2, 1);
    addVariations2(flag2);

    /* flag3 */
    ASSERT_TRUE(flag3 = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag3, "key", LDNewText("feature2")));
    ASSERT_TRUE(LDObjectSetKey(flag3, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag3, "version", LDNewNumber(3)));
    ASSERT_TRUE(LDObjectSetKey(flag3, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag3, "salt", LDNewText("abc")));
    setFallthrough(flag3, 1);
    addVariations2(flag3);

    /* store */
    ASSERT_TRUE(store = prepareEmptyStore());
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, flag2));
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, flag3));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            client,
            flag1,
            user,
            store,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_STREQ(LDGetText(result), "fall");
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 0);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);

    ASSERT_TRUE(events);
    ASSERT_TRUE(LDCollectionGetSize(events) == 2);

    ASSERT_TRUE(eventsiter = LDGetIter(events));
    ASSERT_STREQ("feature2", LDGetText(LDObjectLookup(eventsiter, "key")));
    ASSERT_STREQ("go", LDGetText(LDObjectLookup(eventsiter, "value")));
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "version")), 3);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "variation")), 1);
    ASSERT_STREQ("feature1", LDGetText(LDObjectLookup(eventsiter, "prereqOf")));

    ASSERT_TRUE(eventsiter = LDIterNext(eventsiter));
    ASSERT_STREQ("feature1", LDGetText(LDObjectLookup(eventsiter, "key")));
    ASSERT_STREQ("go", LDGetText(LDObjectLookup(eventsiter, "value")));
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "version")), 3);
    ASSERT_EQ(LDGetNumber(LDObjectLookup(eventsiter, "variation")), 1);
    ASSERT_STREQ("feature0", LDGetText(LDObjectLookup(eventsiter, "prereqOf")));

    LDJSONFree(flag1);
    LDJSONFree(events);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
    LDClientClose(client);
}

TEST_F(EvalFixture, FlagMatchesUserFromTarget) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature")));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("abc")));
    setFallthrough(flag, 0);
    addVariations1(flag);

    {
        struct LDJSON *targetsets;
        struct LDJSON *targetset;
        struct LDJSON *list;

        ASSERT_TRUE(list = LDNewArray());
        ASSERT_TRUE(LDArrayPush(list, LDNewText("whoever")));
        ASSERT_TRUE(LDArrayPush(list, LDNewText("userkey")));

        ASSERT_TRUE(targetset = LDNewObject());
        ASSERT_TRUE(LDObjectSetKey(targetset, "values", list));
        ASSERT_TRUE(LDObjectSetKey(targetset, "variation", LDNewNumber(2)));

        ASSERT_TRUE(targetsets = LDNewArray());
        ASSERT_TRUE(LDArrayPush(targetsets, targetset));
        ASSERT_TRUE(LDObjectSetKey(flag, "targets", targetsets));
    }

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *) 1,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_STREQ(LDGetText(result), "on");
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 2);
    ASSERT_EQ(details.reason, LD_TARGET_MATCH);
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(events);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, FlagMatchesUserFromRules) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *variation, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userkey"));

    /* flag */
    ASSERT_TRUE(variation = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(variation, "variation", LDNewNumber(2)));
    flag = makeFlagToMatchUser("userkey", variation);

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *) 1,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_STREQ(LDGetText(result), "on");
    ASSERT_TRUE(details.hasVariation);
    ASSERT_EQ(details.variationIndex, 2);
    ASSERT_EQ(details.reason, LD_RULE_MATCH);
    ASSERT_EQ(details.extra.rule.ruleIndex, 0);
    ASSERT_STREQ(details.extra.rule.id, "rule-id");
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(events);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, ClauseCanMatchBuiltInAttribute) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    ASSERT_TRUE(user = LDUserNew("key"));
    ASSERT_TRUE(LDUserSetName(user, "Bob"));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("Bob")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("name")));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *) 1,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_TRUE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, ClauseCanMatchCustomAttribute) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *custom, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    ASSERT_TRUE(user = LDUserNew("key"));
    ASSERT_TRUE(custom = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(custom, "legs", LDNewNumber(4)));
    LDUserSetCustom(user, custom);

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewNumber(4)));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("legs")));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *) 1,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_TRUE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, ClauseReturnsFalseForMissingAttribute) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    ASSERT_TRUE(user = LDUserNew("key"));
    ASSERT_TRUE(LDUserSetName(user, "Bob"));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewNumber(4)));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("legs")));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *) 1,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_FALSE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, ClauseCanBeNegated) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    ASSERT_TRUE(user = LDUserNew("key"));
    ASSERT_TRUE(LDUserSetName(user, "Bob"));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("Bob")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("name")));
    ASSERT_TRUE(LDObjectSetKey(clause, "negate", LDNewBool(LDBooleanTrue)));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *) 1,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_FALSE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, ClauseForMissingAttributeIsFalseEvenIfNegate) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    /* user */
    ASSERT_TRUE(user = LDUserNew("key"));
    ASSERT_TRUE(LDUserSetName(user, "Bob"));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewNumber(4)));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("in")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("legs")));
    ASSERT_TRUE(LDObjectSetKey(clause, "negate", LDNewBool(LDBooleanTrue)));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* run */
    ASSERT_TRUE(LDi_evaluate(
            NULL,
            flag,
            user,
            (struct LDStore *) 1,
            &details,
            &events,
            &result,
            LDBooleanFalse));

    /* validate */
    ASSERT_FALSE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, ClauseWithUnknownOperatorDoesNotMatch) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *clause, *values, *events;
    struct LDDetails details;

    result = NULL;
    events = NULL;
    LDDetailsInit(&details);

    /* user */
    ASSERT_TRUE(user = LDUserNew("key"));
    ASSERT_TRUE(LDUserSetName(user, "Bob"));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("Bob")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("unsupported")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("name")));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* run */
    ASSERT_TRUE(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    ASSERT_FALSE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, SegmentMatchClauseRetrievesSegmentFromStore) {
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *segment, *flag, *result, *included, *values, *clause,
            *events;
    struct LDDetails details;

    result = NULL;
    events = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("foo"));

    /* segment */
    ASSERT_TRUE(included = LDNewArray());
    ASSERT_TRUE(LDArrayPush(included, LDNewText("foo")));

    ASSERT_TRUE(segment = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(segment, "key", LDNewText("segkey")));
    ASSERT_TRUE(LDObjectSetKey(segment, "included", included));
    ASSERT_TRUE(LDObjectSetKey(segment, "version", LDNewNumber(3)));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("segkey")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("")));
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("segmentMatch")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* store */
    ASSERT_TRUE(store = prepareEmptyStore());
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, segment));

    /* run */
    ASSERT_TRUE(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    store,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    ASSERT_TRUE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, SegmentMatchClauseFallsThroughIfSegmentNotFound) {
    struct LDUser *user;
    struct LDStore *store;
    struct LDJSON *flag, *result, *values, *clause, *events;
    struct LDDetails details;

    result = NULL;
    events = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("foo"));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("segkey")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("")));
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("segmentMatch")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* store */
    ASSERT_TRUE(store = prepareEmptyStore());

    /* run */
    ASSERT_TRUE(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    store,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    ASSERT_FALSE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, CanMatchJustOneSegmentFromList) {
    struct LDStore *store;
    struct LDUser *user;
    struct LDJSON *segment, *flag, *result, *included, *values, *clause,
            *events;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("foo"));

    /* segment */
    ASSERT_TRUE(included = LDNewArray());
    ASSERT_TRUE(LDArrayPush(included, LDNewText("foo")));

    ASSERT_TRUE(segment = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(segment, "key", LDNewText("segkey")));
    ASSERT_TRUE(LDObjectSetKey(segment, "included", included));
    ASSERT_TRUE(LDObjectSetKey(segment, "version", LDNewNumber(3)));

    /* flag */
    ASSERT_TRUE(values = LDNewArray());
    ASSERT_TRUE(LDArrayPush(values, LDNewText("unknownsegkey")));
    ASSERT_TRUE(LDArrayPush(values, LDNewText("segkey")));

    ASSERT_TRUE(clause = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(clause, "attribute", LDNewText("")));
    ASSERT_TRUE(LDObjectSetKey(clause, "op", LDNewText("segmentMatch")));
    ASSERT_TRUE(LDObjectSetKey(clause, "values", values));

    ASSERT_TRUE(flag = booleanFlagWithClause(clause));

    /* store */
    ASSERT_TRUE(store = prepareEmptyStore());
    ASSERT_TRUE(LDStoreUpsert(store, LD_SEGMENT, segment));

    /* run */
    ASSERT_TRUE(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    store,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse) == EVAL_MATCH);

    /* validate */
    ASSERT_TRUE(LDGetBool(result));
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDStoreDestroy(store);
    LDUserFree(user);
    LDDetailsClear(&details);
}

static LDBoolean
floateq(const float left, const float right) {
    return fabs(left - right) < FLT_EPSILON;
}

TEST_F(EvalFixture, BucketUser) {
    float bucket;
    struct LDUser *user;

    ASSERT_TRUE(user = LDUserNew("userKeyA"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    ASSERT_TRUE(floateq(0.42157587, bucket));
    LDUserFree(user);

    ASSERT_TRUE(user = LDUserNew("userKeyB"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    ASSERT_TRUE(floateq(0.6708485, bucket));
    LDUserFree(user);

    ASSERT_TRUE(user = LDUserNew("userKeyC"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    ASSERT_TRUE(floateq(0.10343106, bucket));
    LDUserFree(user);

    ASSERT_TRUE(user = LDUserNew("userKeyC"));
    ASSERT_TRUE(
            !LDi_bucketUser(user, "hashKey", "unknown", "saltyA", NULL, &bucket));
    ASSERT_TRUE(floateq(0.0, bucket));
    LDUserFree(user);

    ASSERT_TRUE(user = LDUserNew("primaryKey"));
    ASSERT_TRUE(LDUserSetSecondary(user, "secondaryKey"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", NULL, &bucket));
    ASSERT_TRUE(floateq(0.100876, bucket));
    LDUserFree(user);
}

TEST_F(EvalFixture, BucketUserWithSeed) {
    float bucket;
    struct LDUser *user;
    int seed;

    seed = 61;

    ASSERT_TRUE(user = LDUserNew("userKeyA"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    ASSERT_TRUE(floateq(0.09801207, bucket));
    LDUserFree(user);

    ASSERT_TRUE(user = LDUserNew("userKeyB"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    ASSERT_TRUE(floateq(0.14483777, bucket));
    LDUserFree(user);

    ASSERT_TRUE(user = LDUserNew("userKeyC"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    ASSERT_TRUE(floateq(0.9242641, bucket));
    LDUserFree(user);

    ASSERT_TRUE(user = LDUserNew("primaryKey"));
    ASSERT_TRUE(LDUserSetSecondary(user, "secondaryKey"));
    ASSERT_TRUE(LDi_bucketUser(user, "hashKey", "key", "saltyA", &seed, &bucket));
    ASSERT_TRUE(floateq(0.0742077678, bucket));
    LDUserFree(user);
}

TEST_F(EvalFixture, InExperimentExplanation) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events, *fallthrough, *rollout, *variations,
            *variation;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    addVariations1(flag);

    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "fallthrough", fallthrough));
    ASSERT_TRUE(rollout = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(fallthrough, "rollout", rollout));
    ASSERT_TRUE(LDObjectSetKey(rollout, "kind", LDNewText("experiment")));
    ASSERT_TRUE(variations = LDNewArray());
    ASSERT_TRUE(LDObjectSetKey(rollout, "variations", variations));
    ASSERT_TRUE(variation = LDNewObject());
    ASSERT_TRUE(LDArrayPush(variations, variation));
    ASSERT_TRUE(LDObjectSetKey(variation, "weight", LDNewNumber(100000)));
    ASSERT_TRUE(LDObjectSetKey(variation, "variation", LDNewNumber(0)));

    /* run */
    ASSERT_TRUE(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse) == EVAL_MATCH);

    /* validation */
    ASSERT_STREQ("fall", LDGetText(result));
    ASSERT_TRUE(details.hasVariation);
    ASSERT_TRUE(details.extra.fallthrough.inExperiment);
    ASSERT_EQ(details.variationIndex, 0);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, NotInExperimentExplanation) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events, *fallthrough, *rollout, *variations,
            *variation;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    addVariations1(flag);

    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "fallthrough", fallthrough));
    ASSERT_TRUE(rollout = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(fallthrough, "rollout", rollout));
    ASSERT_TRUE(LDObjectSetKey(rollout, "kind", LDNewText("experiment")));
    ASSERT_TRUE(variations = LDNewArray());
    ASSERT_TRUE(LDObjectSetKey(rollout, "variations", variations));
    ASSERT_TRUE(variation = LDNewObject());
    ASSERT_TRUE(LDArrayPush(variations, variation));
    ASSERT_TRUE(LDObjectSetKey(variation, "weight", LDNewNumber(100000)));
    ASSERT_TRUE(LDObjectSetKey(variation, "untracked", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(variation, "variation", LDNewNumber(0)));

    /* run */
    ASSERT_TRUE(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse) == EVAL_MATCH);

    /* validation */
    ASSERT_STREQ("fall", LDGetText(result));
    ASSERT_TRUE(details.hasVariation);
    ASSERT_FALSE(details.extra.fallthrough.inExperiment);
    ASSERT_EQ(details.variationIndex, 0);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}

TEST_F(EvalFixture, RolloutCustomSeed) {
    struct LDUser *user;
    struct LDJSON *flag, *result, *events, *fallthrough, *rollout, *variations,
            *variation;
    struct LDDetails details;

    events = NULL;
    result = NULL;
    LDDetailsInit(&details);

    ASSERT_TRUE(user = LDUserNew("userKeyA"));

    /* flag */
    ASSERT_TRUE(flag = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "key", LDNewText("feature0")));
    ASSERT_TRUE(LDObjectSetKey(flag, "offVariation", LDNewNumber(1)));
    ASSERT_TRUE(LDObjectSetKey(flag, "on", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "salt", LDNewText("123123")));
    addVariations1(flag);

    ASSERT_TRUE(fallthrough = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(flag, "fallthrough", fallthrough));
    ASSERT_TRUE(rollout = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(rollout, "seed", LDNewNumber(50)));
    ASSERT_TRUE(LDObjectSetKey(fallthrough, "rollout", rollout));
    ASSERT_TRUE(LDObjectSetKey(rollout, "kind", LDNewText("experiment")));
    ASSERT_TRUE(variations = LDNewArray());
    ASSERT_TRUE(LDObjectSetKey(rollout, "variations", variations));
    ASSERT_TRUE(variation = LDNewObject());
    ASSERT_TRUE(LDArrayPush(variations, variation));
    ASSERT_TRUE(LDObjectSetKey(variation, "weight", LDNewNumber(100000)));
    ASSERT_TRUE(LDObjectSetKey(variation, "untracked", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(variation, "variation", LDNewNumber(0)));

    /* run */
    ASSERT_EQ(
            LDi_evaluate(
                    NULL,
                    flag,
                    user,
                    (struct LDStore *) 1,
                    &details,
                    &events,
                    &result,
                    LDBooleanFalse), EVAL_MATCH);

    /* validation */
    ASSERT_STREQ("fall", LDGetText(result));
    ASSERT_TRUE(details.hasVariation);
    ASSERT_TRUE(details.extra.fallthrough.inExperiment == LDBooleanFalse);
    ASSERT_EQ(details.variationIndex, 0);
    ASSERT_EQ(details.reason, LD_FALLTHROUGH);
    ASSERT_FALSE(events);

    LDJSONFree(flag);
    LDJSONFree(result);
    LDUserFree(user);
    LDDetailsClear(&details);
}
