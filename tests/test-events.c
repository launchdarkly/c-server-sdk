#include "ldconfig.h"
#include "ldclient.h"
#include "ldvariations.h"
#include "ldinternal.h"
#include "ldjson.h"
#include "ldevents.h"

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

static struct LDJSON *
makeMinimalFlag(const char *const key, const unsigned int version)
{
    struct LDJSON *flag;

    LD_ASSERT(flag = LDNewObject());
    LD_ASSERT(LDObjectSetKey(flag, "key", LDNewText(key)));
    LD_ASSERT(LDObjectSetKey(flag, "version", LDNewNumber(version)));

    return flag;
}

static void
testSummarizeEventIncrementsCounters()
{
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *flag1;
    struct LDJSON *flag2;
    struct LDJSON *event1;
    struct LDJSON *event2;
    struct LDJSON *event3;
    struct LDJSON *event4;
    struct LDJSON *event5;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;
    char *summarykey1;
    char *summarykey2;
    char *summarykey3;
    char *summarykey4;
    struct LDJSON *summary;
    struct LDJSON *value1;
    struct LDJSON *value2;
    struct LDJSON *value99;
    struct LDJSON *default1;
    struct LDJSON *default2;
    struct LDJSON *default3;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(flag1 = makeMinimalFlag("key1", 11));
    LD_ASSERT(flag2 = makeMinimalFlag("key2", 22));

    LD_ASSERT(value1 = LDNewText("value1"));
    LD_ASSERT(value2 = LDNewText("value2"));
    LD_ASSERT(value99 = LDNewText("value88"));
    LD_ASSERT(default1 = LDNewText("default1"));
    LD_ASSERT(default2 = LDNewText("default2"));
    LD_ASSERT(default3 = LDNewText("default3"));

    LD_ASSERT(event1 = newFeatureRequestEvent("key1", user, &variation1,
        value1, default1, NULL, flag1, NULL));
    LD_ASSERT(event2 = newFeatureRequestEvent("key1", user, &variation2,
        value2, default1, NULL, flag1, NULL));
    LD_ASSERT(event3 = newFeatureRequestEvent("key2", user, &variation1,
        value99, default2, NULL, flag2, NULL));
    LD_ASSERT(event4 = newFeatureRequestEvent("key1", user, &variation1,
        value1, default1, NULL, flag1, NULL));
    LD_ASSERT(event5 = newFeatureRequestEvent("badkey", user, NULL,
        default3, default3, NULL, NULL, NULL));

    LD_ASSERT(summarizeEvent(client, event1));
    LD_ASSERT(summarizeEvent(client, event2));
    LD_ASSERT(summarizeEvent(client, event3));
    LD_ASSERT(summarizeEvent(client, event4));
    LD_ASSERT(summarizeEvent(client, event5));

    LD_ASSERT(summarykey1 = makeSummaryKey(event1));
    LD_ASSERT(summarykey2 = makeSummaryKey(event2));
    LD_ASSERT(summarykey3 = makeSummaryKey(event3));
    LD_ASSERT(summarykey4 = makeSummaryKey(event5));

    LD_ASSERT(summary = LDObjectLookup(client->summary, summarykey1));
    LD_ASSERT(LDJSONCompare(value1, LDObjectLookup(summary, "value")));
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summary, "default")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(summary, "count")) == 2);

    LD_ASSERT(summary = LDObjectLookup(client->summary, summarykey2));
    LD_ASSERT(LDJSONCompare(value2, LDObjectLookup(summary, "value")));
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summary, "default")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(summary, "count")) == 1);

    LD_ASSERT(summary = LDObjectLookup(client->summary, summarykey3));
    LD_ASSERT(LDJSONCompare(value99, LDObjectLookup(summary, "value")));
    LD_ASSERT(LDJSONCompare(default2, LDObjectLookup(summary, "default")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(summary, "count")) == 1);

    LD_ASSERT(summary = LDObjectLookup(client->summary, summarykey4));
    LD_ASSERT(LDJSONCompare(default3, LDObjectLookup(summary, "value")));
    LD_ASSERT(LDJSONCompare(default3, LDObjectLookup(summary, "default")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(summary, "count")) == 1);

    free(summarykey1);
    free(summarykey2);
    free(summarykey3);
    free(summarykey4);
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
    LDUserFree(user);
    LDClientClose(client);
}

void
testCounterForNilVariationIsDistinctFromOthers()
{
    struct LDUser *user;
    struct LDClient *client;
    struct LDJSON *flag;
    struct LDJSON *event1;
    struct LDJSON *event2;
    struct LDJSON *event3;
    const unsigned int variation1 = 1;
    const unsigned int variation2 = 2;
    struct LDJSON *value1;
    struct LDJSON *value2;
    struct LDJSON *default1;
    struct LDJSON *summary;
    char *summarykey1;
    char *summarykey2;
    char *summarykey3;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(flag = makeMinimalFlag("key1", 11));

    LD_ASSERT(value1 = LDNewText("value1"));
    LD_ASSERT(value2 = LDNewText("value2"));
    LD_ASSERT(default1 = LDNewText("default1"));

    LD_ASSERT(event1 = newFeatureRequestEvent("key1", user, &variation1,
        value1, default1, NULL, flag, NULL));
    LD_ASSERT(event2 = newFeatureRequestEvent("key1", user, &variation2,
        value2, default1, NULL, flag, NULL));
    LD_ASSERT(event3 = newFeatureRequestEvent("key1", user, NULL,
        default1, default1, NULL, flag, NULL));

    LD_ASSERT(summarykey1 = makeSummaryKey(event1));
    LD_ASSERT(summarykey2 = makeSummaryKey(event2));
    LD_ASSERT(summarykey3 = makeSummaryKey(event3));

    LD_ASSERT(summarizeEvent(client, event1));
    LD_ASSERT(summarizeEvent(client, event2));
    LD_ASSERT(summarizeEvent(client, event3));

    LD_ASSERT(summary = LDObjectLookup(client->summary, summarykey1));
    LD_ASSERT(LDJSONCompare(value1, LDObjectLookup(summary, "value")));
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summary, "default")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(summary, "count")) == 1);

    LD_ASSERT(summary = LDObjectLookup(client->summary, summarykey2));
    LD_ASSERT(LDJSONCompare(value2, LDObjectLookup(summary, "value")));
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summary, "default")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(summary, "count")) == 1);

    LD_ASSERT(summary = LDObjectLookup(client->summary, summarykey3));
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summary, "value")));
    LD_ASSERT(LDJSONCompare(default1, LDObjectLookup(summary, "default")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(summary, "count")) == 1);

    free(summarykey1);
    free(summarykey2);
    free(summarykey3);
    LDJSONFree(flag);
    LDJSONFree(event1);
    LDJSONFree(event2);
    LDJSONFree(event3);
    LDJSONFree(value1);
    LDJSONFree(value2);
    LDJSONFree(default1);
    LDUserFree(user);
    LDClientClose(client);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testSummarizeEventIncrementsCounters();
    testCounterForNilVariationIsDistinctFromOthers();

    return 0;
}
