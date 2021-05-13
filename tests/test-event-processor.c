#include <stdio.h>
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "event_processor.h"
#include "event_processor_internal.h"
#include "store.h"

#include "test-utils/client.h"
#include "test-utils/flags.h"

static void
testConstructAndFree()
{
    struct LDConfig *      config;
    struct EventProcessor *processor;

    LD_ASSERT(config = LDConfigNew("abc"));
    LD_ASSERT(processor = LDi_newEventProcessor(config));

    LDConfigFree(config);
    LDi_freeEventProcessor(processor);
}

static void
testMakeSummaryKeyIncrementsCounters()
{
    struct LDUser *        user;
    struct LDConfig *      config;
    struct EventProcessor *processor;
    struct LDJSON *flag1, *flag2, *event1, *event2, *event3, *event4, *event5,
        *summary, *features, *summaryEntry, *counterEntry, *value1, *value2,
        *value99, *default1, *default2, *default3;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(config = LDConfigNew("key"));
    LD_ASSERT(processor = LDi_newEventProcessor(config));

    LD_ASSERT(
        flag1 = makeMinimalFlag("key1", 11, LDBooleanTrue, LDBooleanFalse));
    LD_ASSERT(
        flag2 = makeMinimalFlag("key2", 22, LDBooleanTrue, LDBooleanFalse));

    LD_ASSERT(value1 = LDNewText("value1"));
    LD_ASSERT(value2 = LDNewText("value2"));
    LD_ASSERT(value99 = LDNewText("value88"));
    LD_ASSERT(default1 = LDNewText("default1"));
    LD_ASSERT(default2 = LDNewText("default2"));
    LD_ASSERT(default3 = LDNewText("default3"));

    LD_ASSERT(
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
    LD_ASSERT(
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
    LD_ASSERT(
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
    LD_ASSERT(
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
    LD_ASSERT(
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

    LD_ASSERT(LDi_summarizeEvent(processor, event1, LDBooleanFalse));
    LD_ASSERT(LDi_summarizeEvent(processor, event2, LDBooleanFalse));
    LD_ASSERT(LDi_summarizeEvent(processor, event3, LDBooleanFalse));
    LD_ASSERT(LDi_summarizeEvent(processor, event4, LDBooleanFalse));
    LD_ASSERT(LDi_summarizeEvent(processor, event5, LDBooleanFalse));

    LD_ASSERT(summary = LDi_prepareSummaryEvent(processor, 0));
    LD_ASSERT(features = LDObjectLookup(summary, "features"))

    LD_ASSERT(summaryEntry = LDObjectLookup(features, "key1"))
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summaryEntry, "default")));
    LD_ASSERT(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    LD_ASSERT(counterEntry = LDGetIter(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 2);
    LD_ASSERT(LDJSONCompare(value1, LDObjectLookup(counterEntry, "value")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 1);
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    LD_ASSERT(counterEntry = LDIterNext(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    LD_ASSERT(LDJSONCompare(value2, LDObjectLookup(counterEntry, "value")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 2);
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    LD_ASSERT(summaryEntry = LDObjectLookup(features, "key2"))
    LD_ASSERT(LDJSONCompare(default2, LDObjectLookup(summaryEntry, "default")));
    LD_ASSERT(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    LD_ASSERT(counterEntry = LDGetIter(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    LD_ASSERT(LDJSONCompare(value99, LDObjectLookup(counterEntry, "value")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 1);
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 22);

    LD_ASSERT(summaryEntry = LDObjectLookup(features, "badkey"))
    LD_ASSERT(LDJSONCompare(default3, LDObjectLookup(summaryEntry, "default")));
    LD_ASSERT(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    LD_ASSERT(counterEntry = LDGetIter(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    LD_ASSERT(LDJSONCompare(default3, LDObjectLookup(counterEntry, "value")));

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

static void
testCounterForNilVariationIsDistinctFromOthers()
{
    struct LDUser *        user;
    struct LDConfig *      config;
    struct EventProcessor *processor;
    struct LDJSON *flag, *event1, *event2, *event3, *value1, *value2, *default1,
        *summary, *features, *summaryEntry, *counterEntry;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(config = LDConfigNew("key"));
    LD_ASSERT(processor = LDi_newEventProcessor(config));

    LD_ASSERT(
        flag = makeMinimalFlag("key1", 11, LDBooleanTrue, LDBooleanFalse));

    LD_ASSERT(value1 = LDNewText("value1"));
    LD_ASSERT(value2 = LDNewText("value2"));
    LD_ASSERT(default1 = LDNewText("default1"));

    LD_ASSERT(
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
    LD_ASSERT(
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
    LD_ASSERT(
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

    LD_ASSERT(LDi_summarizeEvent(processor, event1, LDBooleanFalse));
    LD_ASSERT(LDi_summarizeEvent(processor, event2, LDBooleanFalse));
    LD_ASSERT(LDi_summarizeEvent(processor, event3, LDBooleanFalse));

    LD_ASSERT(summary = LDi_prepareSummaryEvent(processor, 0));
    LD_ASSERT(features = LDObjectLookup(summary, "features"))

    LD_ASSERT(summaryEntry = LDObjectLookup(features, "key1"))
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summaryEntry, "default")));
    LD_ASSERT(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    LD_ASSERT(counterEntry = LDGetIter(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    LD_ASSERT(LDJSONCompare(value1, LDObjectLookup(counterEntry, "value")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 1);
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    LD_ASSERT(counterEntry = LDIterNext(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    LD_ASSERT(LDJSONCompare(value2, LDObjectLookup(counterEntry, "value")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 2);
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 11);

    LD_ASSERT(counterEntry = LDIterNext(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(counterEntry, "value")));

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

static void
testTrackQueued()
{
    const char *     key;
    struct LDClient *client;
    struct LDUser *  user;
    struct LDJSON *  event;

    key = "metric-key1";
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(user = LDUserNew("abc"));

    LD_ASSERT(LDClientTrack(client, key, user, NULL));

    LD_ASSERT(client->eventProcessor->events);
    LD_ASSERT(LDJSONGetType(client->eventProcessor->events) == LDArray);
    /* event + index */
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 2);
    LD_ASSERT(event = LDGetIter(client->eventProcessor->events));
    LD_ASSERT(LDJSONGetType(event) == LDObject);
    LD_ASSERT(strcmp(key, LDGetText(LDObjectLookup(event, "key"))) == 0);
    LD_ASSERT(!LDObjectLookup(event, "data"));
    LD_ASSERT(strcmp("custom", LDGetText(LDObjectLookup(event, "kind"))) == 0);

    LDUserFree(user);
    LDClientClose(client);
}

static void
testTrackMetricQueued()
{
    double           metric;
    const char *     key;
    struct LDClient *client;
    struct LDUser *  user;
    struct LDJSON *  event;

    key    = "metric-key";
    metric = 12.5;
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(user = LDUserNew("abc"));

    LD_ASSERT(LDClientTrackMetric(client, key, user, NULL, metric));

    LD_ASSERT(client->eventProcessor->events);
    LD_ASSERT(LDJSONGetType(client->eventProcessor->events) == LDArray);
    /* event + index */
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 2);
    LD_ASSERT(event = LDGetIter(client->eventProcessor->events));
    LD_ASSERT(LDJSONGetType(event) == LDObject);
    LD_ASSERT(strcmp(key, LDGetText(LDObjectLookup(event, "key"))) == 0);
    LD_ASSERT(!LDObjectLookup(event, "data"));
    LD_ASSERT(strcmp("custom", LDGetText(LDObjectLookup(event, "kind"))) == 0);
    LD_ASSERT(metric == LDGetNumber(LDObjectLookup(event, "metricValue")));

    LDUserFree(user);
    LDClientClose(client);
}

static void
testIdentifyQueued()
{
    struct LDClient *client;
    struct LDUser *  user;
    struct LDJSON *  event;

    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(user = LDUserNew("abc"));

    LD_ASSERT(LDClientIdentify(client, user));

    LD_ASSERT(client->eventProcessor->events);
    LD_ASSERT(LDJSONGetType(client->eventProcessor->events) == LDArray);
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 1);
    LD_ASSERT(event = LDGetIter(client->eventProcessor->events));
    LD_ASSERT(LDJSONGetType(event) == LDObject);
    LD_ASSERT(
        strcmp("identify", LDGetText(LDObjectLookup(event, "kind"))) == 0);
    LD_ASSERT(strcmp("abc", LDGetText(LDObjectLookup(event, "key"))) == 0);

    LDUserFree(user);
    LDClientClose(client);
}

static void
testIndexEventGeneration()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *  flag, *event, *tmp;
    struct LDUser *  user1, *user2;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user1 = LDUserNew("user1"));
    LD_ASSERT(user2 = LDUserNew("user2"));

    LD_ASSERT(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(42));

    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag));

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* evaluation with new user generations index */
    LD_ASSERT(LDIntVariation(client, user1, "flag", 25, NULL) == 42);

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 2);
    /* index event */
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "index") == 0);
    LD_ASSERT(
        tmp = LDi_userToJSON(
            user1,
            LDBooleanTrue,
            config->allAttributesPrivate,
            config->privateAttributeNames));
    LD_ASSERT(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);
    /* feature event */
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 1));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(
        strcmp(LDGetText(LDObjectLookup(event, "userKey")), "user1") == 0);
    LD_ASSERT(LDObjectLookup(event, "user") == NULL);

    /* second evaluation with same user does not generate another event */
    LD_ASSERT(LDIntVariation(client, user1, "flag", 25, NULL) == 42);

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 3);
    /* feature event */
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 2));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(LDObjectLookup(event, "user") == NULL);

    /* evaluation with another user generates a new event */
    LD_ASSERT(LDIntVariation(client, user2, "flag", 25, NULL) == 42);

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 5);
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 3));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "index") == 0);
    LD_ASSERT(
        tmp = LDi_userToJSON(
            user2,
            LDBooleanTrue,
            config->allAttributesPrivate,
            config->privateAttributeNames));
    LD_ASSERT(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 4));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(LDObjectLookup(event, "user") == NULL);

    LDUserFree(user1);
    LDUserFree(user2);
    LDClientClose(client);
}

static void
testInlineUsersInEvents()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *  flag, *event, *tmp;
    struct LDUser *  user;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("user"));

    LD_ASSERT(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));

    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag));
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    LD_ASSERT(LDIntVariation(client, user, "flag", 25, NULL) == 51);

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 1);
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(
        tmp = LDi_userToJSON(
            user,
            LDBooleanTrue,
            config->allAttributesPrivate,
            config->privateAttributeNames));
    LD_ASSERT(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);

    LDUserFree(user);
    LDClientClose(client);
}

static void
testDetailsNotIncludedIfNotDetailed()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *  flag, *event;
    struct LDUser *  user;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("user"));

    LD_ASSERT(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));

    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag));
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    LD_ASSERT(LDIntVariation(client, user, "flag", 25, NULL) == 51);

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 1);
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(LDObjectLookup(event, "reason") == NULL);

    LDUserFree(user);
    LDClientClose(client);
}

static void
testDetailsIncludedIfDetailed()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *  flag, *event, *tmp;
    struct LDUser *  user;
    struct LDDetails details;
    char *           reason;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("user"));

    LD_ASSERT(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));

    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag));
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    LD_ASSERT(LDIntVariation(client, user, "flag", 25, &details) == 51);

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 1);
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(tmp = LDObjectLookup(event, "reason"));
    LD_ASSERT(reason = LDJSONSerialize(tmp));
    LD_ASSERT(strcmp(reason, "{\"kind\":\"FALLTHROUGH\"}") == 0);

    LDFree(reason);
    LDDetailsClear(&details);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testExperimentationFallthroughNonDetailed()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *  flag, *event, *tmp;
    struct LDUser *  user;
    char *           reason;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("user"));

    LD_ASSERT(flag = makeMinimalFlag("flag", 11, LDBooleanTrue, LDBooleanTrue));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));
    LD_ASSERT(tmp = LDNewBool(LDBooleanTrue));
    LD_ASSERT(LDObjectSetKey(flag, "trackEventsFallthrough", tmp));

    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag));
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    LD_ASSERT(LDIntVariation(client, user, "flag", 25, NULL) == 51);

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 1);
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(tmp = LDObjectLookup(event, "reason"));
    LD_ASSERT(reason = LDJSONSerialize(tmp));
    LD_ASSERT(strcmp(reason, "{\"kind\":\"FALLTHROUGH\"}") == 0);

    LDFree(reason);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testExperimentationRuleNonDetailed()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *  flag, *event, *tmp, *variation;
    struct LDUser *  user;
    char *           result;
    char *           reason;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, LDBooleanTrue);
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("user"));

    /* flag */
    LD_ASSERT(variation = LDNewObject());
    LD_ASSERT(LDObjectSetKey(variation, "variation", LDNewNumber(2)));
    flag = makeFlagToMatchUser("user", variation);
    LD_ASSERT(tmp = LDArrayLookup(LDObjectLookup(flag, "rules"), 0))
    LD_ASSERT(LDObjectSetKey(tmp, "trackEvents", LDNewBool(LDBooleanTrue)));
    LD_ASSERT(LDObjectSetKey(flag, "trackEvents", LDNewBool(LDBooleanFalse)));
    LD_ASSERT(LDObjectSetKey(
        flag, "trackEventsFallthrough", LDNewBool(LDBooleanFalse)));

    LD_ASSERT(LDStoreInitEmpty(client->store));
    LD_ASSERT(LDStoreUpsert(client->store, LD_FLAG, flag));
    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 0);

    /* check that user is embedded in full fidelity event */
    LD_ASSERT(result = LDStringVariation(client, user, "feature", "a", NULL));

    LD_ASSERT(LDCollectionGetSize(client->eventProcessor->events) == 1);
    LD_ASSERT(event = LDArrayLookup(client->eventProcessor->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(tmp = LDObjectLookup(event, "reason"));
    LD_ASSERT(reason = LDJSONSerialize(tmp));
    LD_ASSERT(
        strcmp(
            reason,
            "{\"kind\":\"RULE_MATCH\",\"ruleId\":\"rule-id\",\"ruleIndex\":"
            "0}") == 0);

    LDFree(result);
    LDFree(reason);
    LDUserFree(user);
    LDClientClose(client);
}

static void
testConstructAliasEvent()
{
    struct LDUser *previous, *current;
    struct LDJSON *result, *expected;

    LD_ASSERT(previous = LDUserNew("a"));
    LD_ASSERT(current = LDUserNew("b"));

    LDUserSetAnonymous(previous, LDBooleanTrue);

    LD_ASSERT(result = LDi_newAliasEvent(current, previous, 52));

    LD_ASSERT(expected = LDNewObject());
    LD_ASSERT(LDObjectSetKey(expected, "kind", LDNewText("alias")));
    LD_ASSERT(LDObjectSetKey(expected, "creationDate", LDNewNumber(52)));
    LD_ASSERT(LDObjectSetKey(expected, "key", LDNewText("b")));
    LD_ASSERT(LDObjectSetKey(expected, "contextKind", LDNewText("user")));
    LD_ASSERT(LDObjectSetKey(expected, "previousKey", LDNewText("a")));
    LD_ASSERT(LDObjectSetKey(
        expected, "previousContextKind", LDNewText("anonymousUser")));

    LD_ASSERT(LDJSONCompare(result, expected));

    LDJSONFree(expected);
    LDJSONFree(result);
    LDUserFree(previous);
    LDUserFree(current);
}

static void
testAliasEventIsQueued()
{
    struct LDClient *client;
    double           metricValue;
    const char *     metricName;
    struct LDJSON *  payload, *event;
    struct LDUser *  previous, *current;

    LD_ASSERT(previous = LDUserNew("p"));
    LD_ASSERT(current = LDUserNew("c"));

    LD_ASSERT(client = makeOfflineClient());

    LDClientAlias(client, current, previous);

    LD_ASSERT(LDi_bundleEventPayload(client->eventProcessor, &payload))

    LD_ASSERT(LDCollectionGetSize(payload) == 1);
    LD_ASSERT(event = LDArrayLookup(payload, 0));
    LD_ASSERT(strcmp("alias", LDGetText(LDObjectLookup(event, "kind"))) == 0);

    LDUserFree(previous);
    LDUserFree(current);
    LDJSONFree(payload);

    LDClientClose(client);
}

int
main()
{
    LDBasicLoggerThreadSafeInitialize();
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLoggerThreadSafe);
    LDGlobalInit();

    testConstructAndFree();
    testMakeSummaryKeyIncrementsCounters();
    testCounterForNilVariationIsDistinctFromOthers();
    testIndexEventGeneration();
    testTrackQueued();
    testTrackMetricQueued();
    testIdentifyQueued();
    testInlineUsersInEvents();
    testDetailsNotIncludedIfNotDetailed();
    testDetailsIncludedIfDetailed();
    testExperimentationFallthroughNonDetailed();
    testExperimentationRuleNonDetailed();
    testConstructAliasEvent();
    testAliasEventIsQueued();

    LDBasicLoggerThreadSafeShutdown();

    return 0;
};
