#include <time.h>

#include <launchdarkly/api.h>

#include "client.h"
#include "events.h"
#include "misc.h"
#include "config.h"
#include "user.h"

#include "util-flags.h"

static struct LDClient *
makeOfflineClient()
{
    struct LDConfig *config;
    struct LDClient *client;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigSetOffline(config, true);

    LD_ASSERT(client = LDClientInit(config, 0));

    return client;
}

static void
testMakeSummaryKeyIncrementsCounters()
{
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *flag1, *flag2, *event1, *event2, *event3, *event4, *event5,
        *summary, *features, *summaryEntry, *counterEntry, *value1, *value2,
        *value99, *default1, *default2, *default3;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(flag1 = makeMinimalFlag("key1", 11, true, false));
    LD_ASSERT(flag2 = makeMinimalFlag("key2", 22, true, false));

    LD_ASSERT(value1 = LDNewText("value1"));
    LD_ASSERT(value2 = LDNewText("value2"));
    LD_ASSERT(value99 = LDNewText("value88"));
    LD_ASSERT(default1 = LDNewText("default1"));
    LD_ASSERT(default2 = LDNewText("default2"));
    LD_ASSERT(default3 = LDNewText("default3"));

    LD_ASSERT(event1 = LDi_newFeatureRequestEvent(client, "key1", user,
        &variation1, value1, default1, NULL, flag1, NULL));
    LD_ASSERT(event2 = LDi_newFeatureRequestEvent(client, "key1", user,
        &variation2, value2, default1, NULL, flag1, NULL));
    LD_ASSERT(event3 = LDi_newFeatureRequestEvent(client, "key2", user,
        &variation1, value99, default2, NULL, flag2, NULL));
    LD_ASSERT(event4 = LDi_newFeatureRequestEvent(client, "key1", user,
        &variation1, value1, default1, NULL, flag1, NULL));
    LD_ASSERT(event5 = LDi_newFeatureRequestEvent(client, "badkey", user, NULL,
        default3, default3, NULL, NULL, NULL));

    LD_ASSERT(LDi_summarizeEvent(client, event1, false));
    LD_ASSERT(LDi_summarizeEvent(client, event2, false));
    LD_ASSERT(LDi_summarizeEvent(client, event3, false));
    LD_ASSERT(LDi_summarizeEvent(client, event4, false));
    LD_ASSERT(LDi_summarizeEvent(client, event5, false));

    LD_ASSERT(LDi_rdlock(&client->lock));
    LD_ASSERT(summary = LDi_prepareSummaryEvent(client));
    LD_ASSERT(LDi_rdunlock(&client->lock));
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
    LDClientClose(client);
}

static void
testCounterForNilVariationIsDistinctFromOthers()
{
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *flag, *event1, *event2, *event3, *value1, *value2, *default1,
        *summary, *features, *summaryEntry, *counterEntry;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(flag = makeMinimalFlag("key1", 11, true, false));

    LD_ASSERT(value1 = LDNewText("value1"));
    LD_ASSERT(value2 = LDNewText("value2"));
    LD_ASSERT(default1 = LDNewText("default1"));

    LD_ASSERT(event1 = LDi_newFeatureRequestEvent(client, "key1", user,
        &variation1, value1, default1, NULL, flag, NULL));
    LD_ASSERT(event2 = LDi_newFeatureRequestEvent(client, "key1", user,
        &variation2, value2, default1, NULL, flag, NULL));
    LD_ASSERT(event3 = LDi_newFeatureRequestEvent(client, "key1", user, NULL,
        default1, default1, NULL, flag, NULL));

    LD_ASSERT(LDi_summarizeEvent(client, event1, false));
    LD_ASSERT(LDi_summarizeEvent(client, event2, false));
    LD_ASSERT(LDi_summarizeEvent(client, event3, false));

    LD_ASSERT(LDi_rdlock(&client->lock));
    LD_ASSERT(summary = LDi_prepareSummaryEvent(client));
    LD_ASSERT(LDi_rdunlock(&client->lock));
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
    LDClientClose(client);
}

static void
testParseHTTPDate()
{
    struct tm tm;
    const char *const header = "Fri, 29 Mar 2019 17:55:35 GMT";
    LD_ASSERT(LDi_parseRFC822(header, &tm));
}

static void
testParseServerTimeHeaderActual()
{
    struct LDClient *client;
    const char *const header = "Date: Fri, 29 Mar 2019 17:55:35 GMT\r\n";
    const size_t total = strlen(header);

    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(LDi_onHeader(header, total, 1, client) == total);
    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(client->lastServerTime >= 1553880000000);
    LD_ASSERT(client->lastServerTime <= 1553911000000);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LDClientClose(client);
}

static void
testParseServerTimeHeaderAlt()
{
    struct LDClient *client;
    const char *const header = "date:Fri, 29 Mar 2019 17:55:35 GMT   \r\n";
    const size_t total = strlen(header);

    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(LDi_onHeader(header, total, 1, client) == total);
    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(client->lastServerTime >= 1553880000000);
    LD_ASSERT(client->lastServerTime <= 1553911000000);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LDClientClose(client);
}

static void
testParseServerTimeHeaderBad()
{
    struct LDClient *client;
    const char *const header1 = "Date: not a valid date\r\n";
    const size_t total1 = strlen(header1);
    const char *const header2 = "Date:\r\n";
    const size_t total2 = strlen(header2);

    LD_ASSERT(client = makeOfflineClient());

    LD_ASSERT(LDi_onHeader(header1, total1, 1, client) == total1);
    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(client->lastServerTime == 0);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LD_ASSERT(LDi_onHeader(header2, total2, 1, client) == total2);
    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(client->lastServerTime == 0);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LDClientClose(client);
}

static void
testTrackMetricQueued()
{
    double metric;
    const char *key;
    struct LDClient *client;
    struct LDUser *user;
    struct LDJSON *event;

    key = "metric-key";
    metric = 12.5;
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(user = LDUserNew("abc"));

    LD_ASSERT(LDClientTrackMetric(client, key, user, NULL, metric));

    LD_ASSERT(client->events);
    LD_ASSERT(LDJSONGetType(client->events) == LDArray);
    LD_ASSERT(LDCollectionGetSize(client->events) == 1);
    LD_ASSERT(event = LDGetIter(client->events));
    LD_ASSERT(LDJSONGetType(event) == LDObject);
    LD_ASSERT(strcmp(key, LDGetText(LDObjectLookup(event, "key"))) == 0);
    LD_ASSERT(!LDObjectLookup(event, "data"));
    LD_ASSERT(strcmp("custom", LDGetText(LDObjectLookup(event, "kind"))) == 0);
    LD_ASSERT(metric == LDGetNumber(LDObjectLookup(event, "metricValue")));

    LDUserFree(user);
    LDClientClose(client);
}

static void
testIndexEventGeneration()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event, *tmp;
    struct LDUser *user1, *user2;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user1 = LDUserNew("user1"));
    LD_ASSERT(user2 = LDUserNew("user2"));

    LD_ASSERT(flag = makeMinimalFlag("flag", 11, true, true));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(42));

    LD_ASSERT(LDStoreInitEmpty(config->store));
    LD_ASSERT(LDStoreUpsert(config->store, LD_FLAG, flag));

    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(LDCollectionGetSize(client->events) == 0);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    /* evaluation with new user generations index */
    LD_ASSERT(LDIntVariation(client, user1, "flag", 25, NULL) == 42);

    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(LDCollectionGetSize(client->events) == 2);
    /* index event */
    LD_ASSERT(event = LDArrayLookup(client->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "index") == 0);
    LD_ASSERT(tmp = LDUserToJSON(client, user1, true));
    LD_ASSERT(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);
    /* feature event */
    LD_ASSERT(event = LDArrayLookup(client->events, 1));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "userKey")), "user1")
        == 0);
    LD_ASSERT(LDObjectLookup(event, "user") == NULL);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    /* second evaluation with same user does not generate another event */
    LD_ASSERT(LDIntVariation(client, user1, "flag", 25, NULL) == 42);

    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(LDCollectionGetSize(client->events) == 3);
    /* feature event */
    LD_ASSERT(event = LDArrayLookup(client->events, 2));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(LDObjectLookup(event, "user") == NULL);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    /* evaluation with another user generates a new event */
    LD_ASSERT(LDIntVariation(client, user2, "flag", 25, NULL) == 42);

    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(LDCollectionGetSize(client->events) == 5);
    LD_ASSERT(event = LDArrayLookup(client->events, 3));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "index") == 0);
    LD_ASSERT(tmp = LDUserToJSON(client, user2, true));
    LD_ASSERT(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);
    LD_ASSERT(event = LDArrayLookup(client->events, 4));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(LDObjectLookup(event, "user") == NULL);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LDUserFree(user1);
    LDUserFree(user2);
    LDClientClose(client);
}

static void
testInlineUsersInEvents()
{
    struct LDConfig *config;
    struct LDClient *client;
    struct LDJSON *flag, *event, *tmp;
    struct LDUser *user;

    LD_ASSERT(config = LDConfigNew("api_key"));
    LDConfigInlineUsersInEvents(config, true);
    LD_ASSERT(client = LDClientInit(config, 0));
    LD_ASSERT(user = LDUserNew("user"));

    LD_ASSERT(flag = makeMinimalFlag("flag", 11, true, true));
    setFallthrough(flag, 0);
    addVariation(flag, LDNewNumber(51));

    LD_ASSERT(LDStoreInitEmpty(config->store));
    LD_ASSERT(LDStoreUpsert(config->store, LD_FLAG, flag));
    LD_ASSERT(LDCollectionGetSize(client->events) == 0);

    /* check that user is embedded in full fidelity event */
    LD_ASSERT(LDIntVariation(client, user, "flag", 25, NULL) == 51);

    LD_ASSERT(LDi_wrlock(&client->lock));
    LD_ASSERT(LDCollectionGetSize(client->events) == 1);
    LD_ASSERT(event = LDArrayLookup(client->events, 0));
    LD_ASSERT(strcmp(LDGetText(LDObjectLookup(event, "kind")), "feature") == 0);
    LD_ASSERT(tmp = LDUserToJSON(client, user, true));
    LD_ASSERT(LDJSONCompare(LDObjectLookup(event, "user"), tmp));
    LDJSONFree(tmp);
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LDUserFree(user);
    LDClientClose(client);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testMakeSummaryKeyIncrementsCounters();
    testCounterForNilVariationIsDistinctFromOthers();
    testParseHTTPDate();
    testParseServerTimeHeaderActual();
    testParseServerTimeHeaderAlt();
    testParseServerTimeHeaderBad();
    testTrackMetricQueued();
    testIndexEventGeneration();
    testInlineUsersInEvents();

    return 0;
}
