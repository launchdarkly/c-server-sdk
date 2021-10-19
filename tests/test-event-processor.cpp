#include "gtest/gtest.h"
#include "commonfixture.h"

extern "C" {
#include <launchdarkly/api.h>

#include "client.h"
#include "event_processor.h"
#include "event_processor_internal.h"
#include "store.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class EventProcessorFixture : public CommonFixture {
};


TEST_F(EventProcessorFixture, ConstructAndFree) {
    struct LDConfig *config;
    struct EventProcessor *processor;

    ASSERT_TRUE(config = LDConfigNew("abc")
    );
    ASSERT_TRUE(processor = LDi_newEventProcessor(config)
    );

    LDConfigFree(config);
    LDi_freeEventProcessor(processor);
}

TEST_F(EventProcessorFixture, MakeSummaryKeyIncrementsCounters) {
    struct LDUser *user;
    struct LDConfig *config;
    struct EventProcessor *processor;
    struct LDJSON *flag1, *flag2, *event1, *event2, *event3, *event4, *event5,
            *summary, *features, *summaryEntry, *counterEntry, *value1, *value2,
            *value99, *default1, *default2, *default3;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;

    ASSERT_TRUE(user = LDUserNew("abc"));
    ASSERT_TRUE(config = LDConfigNew("key"));
    ASSERT_TRUE(processor = LDi_newEventProcessor(config));

    ASSERT_TRUE(
            flag1 = makeMinimalFlag("key1", 11, LDBooleanTrue, LDBooleanFalse));
    ASSERT_TRUE(
            flag2 = makeMinimalFlag("key2", 22, LDBooleanTrue, LDBooleanFalse));

    ASSERT_TRUE(value1 = LDNewText("value1"));
    ASSERT_TRUE(value2 = LDNewText("value2"));
    ASSERT_TRUE(value99 = LDNewText("value88"));
    ASSERT_TRUE(default1 = LDNewText("default1"));
    ASSERT_TRUE(default2 = LDNewText("default2"));
    ASSERT_TRUE(default3 = LDNewText("default3"));

    ASSERT_TRUE(
            event1 = LDi_newFeatureRequestEvent(
                    processor,
                    "key1",
                    user,
                    &variation1,
                    value1,
                    default1,
                    NULL,
                    flag1,
                    NULL,
                    0));
    ASSERT_TRUE(
            event2 = LDi_newFeatureRequestEvent(
                    processor,
                    "key1",
                    user,
                    &variation2,
                    value2,
                    default1,
                    NULL,
                    flag1,
                    NULL,
                    0));
    ASSERT_TRUE(
            event3 = LDi_newFeatureRequestEvent(
                    processor,
                    "key2",
                    user,
                    &variation1,
                    value99,
                    default2,
                    NULL,
                    flag2,
                    NULL,
                    0));
    ASSERT_TRUE(
            event4 = LDi_newFeatureRequestEvent(
                    processor,
                    "key1",
                    user,
                    &variation1,
                    value1,
                    default1,
                    NULL,
                    flag1,
                    NULL,
                    0));
    ASSERT_TRUE(
            event5 = LDi_newFeatureRequestEvent(
                    processor,
                    "badkey",
                    user,
                    NULL,
                    default3,
                    default3,
                    NULL,
                    NULL,
                    NULL,
                    0));

    ASSERT_TRUE(LDi_summarizeEvent(processor, event1, LDBooleanFalse));
    ASSERT_TRUE(LDi_summarizeEvent(processor, event2, LDBooleanFalse));
    ASSERT_TRUE(LDi_summarizeEvent(processor, event3, LDBooleanFalse));
    ASSERT_TRUE(LDi_summarizeEvent(processor, event4, LDBooleanFalse));
    ASSERT_TRUE(LDi_summarizeEvent(processor, event5, LDBooleanFalse));

    ASSERT_TRUE(summary = LDi_prepareSummaryEvent(processor, 0));
    ASSERT_TRUE(features = LDObjectLookup(summary, "features"));

    ASSERT_TRUE(summaryEntry = LDObjectLookup(features, "key1"));
    ASSERT_TRUE(LDJSONCompare(default1, LDObjectLookup(summaryEntry, "default")));
    ASSERT_TRUE(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    ASSERT_TRUE(counterEntry = LDGetIter(counterEntry));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 2);
    ASSERT_TRUE(LDJSONCompare(value1, LDObjectLookup(counterEntry, "value")));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 1);
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    ASSERT_TRUE(counterEntry = LDIterNext(counterEntry));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    ASSERT_TRUE(LDJSONCompare(value2, LDObjectLookup(counterEntry, "value")));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 2);
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    ASSERT_TRUE(summaryEntry = LDObjectLookup(features, "key2"));
    ASSERT_TRUE(LDJSONCompare(default2, LDObjectLookup(summaryEntry, "default")));
    ASSERT_TRUE(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    ASSERT_TRUE(counterEntry = LDGetIter(counterEntry));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    ASSERT_TRUE(LDJSONCompare(value99, LDObjectLookup(counterEntry, "value")));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 1);
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 22);

    ASSERT_TRUE(summaryEntry = LDObjectLookup(features, "badkey"));
    ASSERT_TRUE(LDJSONCompare(default3, LDObjectLookup(summaryEntry, "default")));
    ASSERT_TRUE(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    ASSERT_TRUE(counterEntry = LDGetIter(counterEntry));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    ASSERT_TRUE(LDJSONCompare(default3, LDObjectLookup(counterEntry, "value")));

    LDJSONFree(flag1);
    LDJSONFree(flag2);
    LDJSONFree(event1);
    LDJSONFree(event2);
    LDJSONFree(event3);
    LDJSONFree(event4);
    LDJSONFree(event5);
    LDJSONFree(value1);
    LDJSONFree(value2);
    LDJSONFree(value99);
    LDJSONFree(default1);
    LDJSONFree(default2);
    LDJSONFree(default3);
    LDJSONFree(summary);
    LDUserFree(user);
    LDConfigFree(config);
    LDi_freeEventProcessor(processor);
}

TEST_F(EventProcessorFixture, CounterForNilVariationIsDistinctFromOthers) {
    struct LDUser *user;
    struct LDConfig *config;
    struct EventProcessor *processor;
    struct LDJSON *flag, *event1, *event2, *event3, *value1, *value2, *default1,
            *summary, *features, *summaryEntry, *counterEntry;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;

    ASSERT_TRUE(user = LDUserNew("abc"));
    ASSERT_TRUE(config = LDConfigNew("key"));
    ASSERT_TRUE(processor = LDi_newEventProcessor(config));

    ASSERT_TRUE(
            flag = makeMinimalFlag("key1", 11, LDBooleanTrue, LDBooleanFalse));

    ASSERT_TRUE(value1 = LDNewText("value1"));
    ASSERT_TRUE(value2 = LDNewText("value2"));
    ASSERT_TRUE(default1 = LDNewText("default1"));

    ASSERT_TRUE(
            event1 = LDi_newFeatureRequestEvent(
                    processor,
                    "key1",
                    user,
                    &variation1,
                    value1,
                    default1,
                    NULL,
                    flag,
                    NULL,
                    0));
    ASSERT_TRUE(
            event2 = LDi_newFeatureRequestEvent(
                    processor,
                    "key1",
                    user,
                    &variation2,
                    value2,
                    default1,
                    NULL,
                    flag,
                    NULL,
                    0));
    ASSERT_TRUE(
            event3 = LDi_newFeatureRequestEvent(
                    processor,
                    "key1",
                    user,
                    NULL,
                    default1,
                    default1,
                    NULL,
                    flag,
                    NULL,
                    0));

    ASSERT_TRUE(LDi_summarizeEvent(processor, event1, LDBooleanFalse));
    ASSERT_TRUE(LDi_summarizeEvent(processor, event2, LDBooleanFalse));
    ASSERT_TRUE(LDi_summarizeEvent(processor, event3, LDBooleanFalse));

    ASSERT_TRUE(summary = LDi_prepareSummaryEvent(processor, 0));
    ASSERT_TRUE(features = LDObjectLookup(summary, "features"));

    ASSERT_TRUE(summaryEntry = LDObjectLookup(features, "key1"));
    ASSERT_TRUE(LDJSONCompare(default1, LDObjectLookup(summaryEntry, "default")));
    ASSERT_TRUE(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    ASSERT_TRUE(counterEntry = LDGetIter(counterEntry));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    ASSERT_TRUE(LDJSONCompare(value1, LDObjectLookup(counterEntry, "value")));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 1);
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    ASSERT_TRUE(counterEntry = LDIterNext(counterEntry));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    ASSERT_TRUE(LDJSONCompare(value2, LDObjectLookup(counterEntry, "value")));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 2);
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    ASSERT_TRUE(counterEntry = LDIterNext(counterEntry));
    ASSERT_TRUE(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    ASSERT_TRUE(LDJSONCompare(default1, LDObjectLookup(counterEntry, "value")));

    LDJSONFree(flag);
    LDJSONFree(event1);
    LDJSONFree(event2);
    LDJSONFree(event3);
    LDJSONFree(value1);
    LDJSONFree(value2);
    LDJSONFree(default1);
    LDJSONFree(summary);
    LDUserFree(user);
    LDConfigFree(config);
    LDi_freeEventProcessor(processor);
}

TEST_F(EventProcessorFixture, TrackQueued) {
    const char *key;
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *event;

    key = "metric-key1";
    ASSERT_TRUE(client = makeOfflineClient());
    ASSERT_TRUE(user = LDUserNew("abc"));

    ASSERT_TRUE(LDClientTrack(client, key, user, NULL));

    ASSERT_TRUE(client->eventProcessor->events);
    ASSERT_TRUE(LDJSONGetType(client->eventProcessor->events) == LDArray);
    /* event + index */
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 2);
    ASSERT_TRUE(event = LDGetIter(client->eventProcessor->events));
    ASSERT_TRUE(LDJSONGetType(event) == LDObject);
    ASSERT_STREQ(key, LDGetText(LDObjectLookup(event, "key")));
    ASSERT_TRUE(!LDObjectLookup(event, "data"));
    ASSERT_STREQ("custom", LDGetText(LDObjectLookup(event, "kind")));

    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, TrackMetricQueued) {
    double metric;
    const char *key;
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *event;

    key = "metric-key";
    metric = 12.5;
    ASSERT_TRUE(client = makeOfflineClient());
    ASSERT_TRUE(user = LDUserNew("abc"));

    ASSERT_TRUE(LDClientTrackMetric(client, key, user, NULL, metric));

    ASSERT_TRUE(client->eventProcessor->events);
    ASSERT_TRUE(LDJSONGetType(client->eventProcessor->events) == LDArray);
    /* event + index */
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 2);
    ASSERT_TRUE(event = LDGetIter(client->eventProcessor->events));
    ASSERT_TRUE(LDJSONGetType(event) == LDObject);
    ASSERT_STREQ(key, LDGetText(LDObjectLookup(event, "key")));
    ASSERT_TRUE(!LDObjectLookup(event, "data"));
    ASSERT_STREQ("custom", LDGetText(LDObjectLookup(event, "kind")));
    ASSERT_TRUE(metric == LDGetNumber(LDObjectLookup(event, "metricValue")));

    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, IdentifyQueued) {
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *event;

    ASSERT_TRUE(client = makeOfflineClient());
    ASSERT_TRUE(user = LDUserNew("abc"));

    ASSERT_TRUE(LDClientIdentify(client, user));

    ASSERT_TRUE(client->eventProcessor->events);
    ASSERT_TRUE(LDJSONGetType(client->eventProcessor->events) == LDArray);
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 1);
    ASSERT_TRUE(event = LDGetIter(client->eventProcessor->events));
    ASSERT_TRUE(LDJSONGetType(event) == LDObject);
    ASSERT_STREQ("identify", LDGetText(LDObjectLookup(event, "kind")));
    ASSERT_STREQ("abc", LDGetText(LDObjectLookup(event, "key")));

    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, IndexEventGeneration) {
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event, *tmp;
    struct LDUser *user1, *user2;

    ASSERT_TRUE(config = LDConfigNew("api_key"));
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user1 = LDUserNew("user1"));
    ASSERT_TRUE(user2 = LDUserNew("user2"));

    ASSERT_TRUE(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(42));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag));

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* evaluation with new user generations index */
    ASSERT_TRUE(LDIntVariation(client, user1, "flag", 25, NULL) == 42);

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 2);
    /* index event */
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 0));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "index");
    ASSERT_TRUE(
            tmp = LDi_userToJSON(
                    user1,
                    LDBooleanTrue,
                    config->allAttributesPrivate,
                    config->privateAttributeNames));
    ASSERT_TRUE(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);
    /* feature event */
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 1));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "userKey")), "user1");
    ASSERT_TRUE(LDObjectLookup(event, "user") == NULL);

    /* second evaluation with same user does not generate another event */
    ASSERT_TRUE(LDIntVariation(client, user1, "flag", 25, NULL) == 42);

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 3);
    /* feature event */
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 2));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_TRUE(LDObjectLookup(event, "user") == NULL);

    /* evaluation with another user generates a new event */
    ASSERT_TRUE(LDIntVariation(client, user2, "flag", 25, NULL) == 42);

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 5);
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 3));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "index");
    ASSERT_TRUE(
            tmp = LDi_userToJSON(
                    user2,
                    LDBooleanTrue,
                    config->allAttributesPrivate,
                    config->privateAttributeNames));
    ASSERT_TRUE(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 4));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_TRUE(LDObjectLookup(event, "user") == NULL);

    LDUserFree(user1);
    LDUserFree(user2);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, InlineUsersInEvents) {
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event, *tmp;
    struct LDUser *user;

    ASSERT_TRUE(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("user"));

    ASSERT_TRUE(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag));
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    ASSERT_TRUE(LDIntVariation(client, user, "flag", 25, NULL) == 51);

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 1);
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 0));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_TRUE(
            tmp = LDi_userToJSON(
                    user,
                    LDBooleanTrue,
                    config->allAttributesPrivate,
                    config->privateAttributeNames));
    ASSERT_TRUE(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);

    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, DetailsNotIncludedIfNotDetailed) {
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event;
    struct LDUser *user;

    ASSERT_TRUE(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("user"));

    ASSERT_TRUE(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag));
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    ASSERT_TRUE(LDIntVariation(client, user, "flag", 25, NULL) == 51);

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 1);
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 0));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_TRUE(LDObjectLookup(event, "reason") == NULL);

    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, DetailsIncludedIfDetailed) {
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event, *tmp;
    struct LDUser *user;
    struct LDDetails details;
    char *reason;

    ASSERT_TRUE(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("user"));

    ASSERT_TRUE(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag));
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    ASSERT_TRUE(LDIntVariation(client, user, "flag", 25, &details) == 51);

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 1);
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 0));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_TRUE(tmp = LDObjectLookup(event, "reason"));
    ASSERT_TRUE(reason = LDJSONSerialize(tmp));
    ASSERT_STREQ(reason, "{\"kind\":\"FALLTHROUGH\"}");

    LDFree(reason);
    LDDetailsClear(&details);
    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, ExperimentationFallthroughNonDetailed) {
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event, *tmp;
    struct LDUser *user;
    char *reason;

    ASSERT_TRUE(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("user"));

    ASSERT_TRUE(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));
    ASSERT_TRUE(tmp = LDNewBool(LDBooleanTrue));
    ASSERT_TRUE(LDObjectSetKey(flag, "trackEventsFallthrough", tmp));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag));
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    ASSERT_TRUE(LDIntVariation(client, user, "flag", 25, NULL) == 51);

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 1);
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 0));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_TRUE(tmp = LDObjectLookup(event, "reason"));
    ASSERT_TRUE(reason = LDJSONSerialize(tmp));
    ASSERT_STREQ(reason, "{\"kind\":\"FALLTHROUGH\"}");

    LDFree(reason);
    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, ExperimentationRuleNonDetailed) {
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event, *tmp, *variation;
    struct LDUser *user;
    char *result;
    char *reason;

    ASSERT_TRUE(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    ASSERT_TRUE(client = LDClientInit(config, 0));
    ASSERT_TRUE(user = LDUserNew("user"));

    /* flag */
    ASSERT_TRUE(variation = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(variation, "variation", LDNewNumber(2)));
    flag = makeFlagToMatchUser("user", variation);
    ASSERT_TRUE(tmp = LDArrayLookup(LDObjectLookup(flag, "rules"), 0));
    ASSERT_TRUE(LDObjectSetKey(tmp, "trackEvents", LDNewBool(LDBooleanTrue)));
    ASSERT_TRUE(LDObjectSetKey(flag, "trackEvents", LDNewBool(LDBooleanFalse)));
    ASSERT_TRUE(LDObjectSetKey(
            flag, "trackEventsFallthrough", LDNewBool(LDBooleanFalse)));

    ASSERT_TRUE(LDStoreInitEmpty(client->store));
    ASSERT_TRUE(LDStoreUpsert(client->store, LD_FLAG, flag));
    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    ASSERT_TRUE(result = LDStringVariation(client, user, "feature", "a", NULL));

    ASSERT_TRUE(LDCollectionGetSize(client->eventProcessor->events) == 1);
    ASSERT_TRUE(event = LDArrayLookup(client->eventProcessor->events, 0));
    ASSERT_STREQ(LDGetText(LDObjectLookup(event, "kind")), "feature");
    ASSERT_TRUE(tmp = LDObjectLookup(event, "reason"));
    ASSERT_TRUE(reason = LDJSONSerialize(tmp));
    ASSERT_STREQ(reason,
                 "{\"kind\":\"RULE_MATCH\",\"ruleId\":\"rule-id\",\"ruleIndex\":"
                 "0}");

    LDFree(result);
    LDFree(reason);
    LDUserFree(user);
    LDClientClose(client);
}

TEST_F(EventProcessorFixture, ConstructAliasEvent) {
    struct LDUser *previous, *current;
    struct LDJSON *result, *expected;

    ASSERT_TRUE(previous = LDUserNew("a"));
    ASSERT_TRUE(current = LDUserNew("b"));

    LDUserSetAnonymous(previous, LDBooleanTrue);

    ASSERT_TRUE(result = LDi_newAliasEvent(current, previous, 52));

    ASSERT_TRUE(expected = LDNewObject());
    ASSERT_TRUE(LDObjectSetKey(expected, "kind", LDNewText("alias")));
    ASSERT_TRUE(LDObjectSetKey(expected, "creationDate", LDNewNumber(52)));
    ASSERT_TRUE(LDObjectSetKey(expected, "key", LDNewText("b")));
    ASSERT_TRUE(LDObjectSetKey(expected, "contextKind", LDNewText("user")));
    ASSERT_TRUE(LDObjectSetKey(expected, "previousKey", LDNewText("a")));
    ASSERT_TRUE(LDObjectSetKey(
            expected, "previousContextKind", LDNewText("anonymousUser")));

    ASSERT_TRUE(LDJSONCompare(result, expected));

    LDJSONFree(expected);
    LDJSONFree(result);
    LDUserFree(previous);
    LDUserFree(current);
}

TEST_F(EventProcessorFixture, AliasEventIsQueued) {
    struct LDClient *client;
    double metricValue;
    const char *metricName;
    struct LDJSON *payload, *event;
    struct LDUser *previous, *current;

    ASSERT_TRUE(previous = LDUserNew("p"));
    ASSERT_TRUE(current = LDUserNew("c"));

    ASSERT_TRUE(client = makeOfflineClient());

    LDClientAlias(client, current, previous);

    ASSERT_TRUE(LDi_bundleEventPayload(client->eventProcessor, &payload));

    ASSERT_TRUE(LDCollectionGetSize(payload) == 1);
    ASSERT_TRUE(event = LDArrayLookup(payload, 0));
    ASSERT_STREQ("alias", LDGetText(LDObjectLookup(event, "kind")));

    LDUserFree(previous);
    LDUserFree(current);
    LDJSONFree(payload);

    LDClientClose(client);
}
