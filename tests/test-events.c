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
    struct LDJSON *summary;
    struct LDJSON *summaryEntry;
    struct LDJSON *counterEntry;
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

    LD_ASSERT(event1 = newFeatureRequestEvent(client, "key1", user, &variation1,
        value1, default1, NULL, flag1, NULL));
    LD_ASSERT(event2 = newFeatureRequestEvent(client, "key1", user, &variation2,
        value2, default1, NULL, flag1, NULL));
    LD_ASSERT(event3 = newFeatureRequestEvent(client, "key2", user, &variation1,
        value99, default2, NULL, flag2, NULL));
    LD_ASSERT(event4 = newFeatureRequestEvent(client, "key1", user, &variation1,
        value1, default1, NULL, flag1, NULL));
    LD_ASSERT(event5 = newFeatureRequestEvent(client, "badkey", user, NULL,
        default3, default3, NULL, NULL, NULL));

    LD_ASSERT(summarizeEvent(client, event1, false));
    LD_ASSERT(summarizeEvent(client, event2, false));
    LD_ASSERT(summarizeEvent(client, event3, false));
    LD_ASSERT(summarizeEvent(client, event4, false));
    LD_ASSERT(summarizeEvent(client, event5, false));

    LD_ASSERT(summary = prepareSummaryEvent(client));
    LD_ASSERT(summary = LDObjectLookup(summary, "features"))

    LD_ASSERT(summaryEntry = LDObjectLookup(summary, "key1"))
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

    LD_ASSERT(summaryEntry = LDObjectLookup(summary, "key2"))
    LD_ASSERT(LDJSONCompare(default2, LDObjectLookup(summaryEntry, "default")));
    LD_ASSERT(counterEntry = LDObjectLookup(summaryEntry, "counters"));

    LD_ASSERT(counterEntry = LDGetIter(counterEntry));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "count")) == 1);
    LD_ASSERT(LDJSONCompare(value99, LDObjectLookup(counterEntry, "value")));
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "variation")) == 1);
    LD_ASSERT(LDGetNumber(LDObjectLookup(counterEntry, "version")) == 22);

    LD_ASSERT(summaryEntry = LDObjectLookup(summary, "badkey"))
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
    struct LDJSON *summaryEntry;
    struct LDJSON *counterEntry;

    LD_ASSERT(user = LDUserNew("abc"));
    LD_ASSERT(client = makeOfflineClient());
    LD_ASSERT(flag = makeMinimalFlag("key1", 11));

    LD_ASSERT(value1 = LDNewText("value1"));
    LD_ASSERT(value2 = LDNewText("value2"));
    LD_ASSERT(default1 = LDNewText("default1"));

    LD_ASSERT(event1 = newFeatureRequestEvent(client, "key1", user, &variation1,
        value1, default1, NULL, flag, NULL));
    LD_ASSERT(event2 = newFeatureRequestEvent(client, "key1", user, &variation2,
        value2, default1, NULL, flag, NULL));
    LD_ASSERT(event3 = newFeatureRequestEvent(client, "key1", user, NULL,
        default1, default1, NULL, flag, NULL));

    LD_ASSERT(summarizeEvent(client, event1, false));
    LD_ASSERT(summarizeEvent(client, event2, false));
    LD_ASSERT(summarizeEvent(client, event3, false));

    LD_ASSERT(summary = prepareSummaryEvent(client));
    LD_ASSERT(summary = LDObjectLookup(summary, "features"))

    LD_ASSERT(summaryEntry = LDObjectLookup(summary, "key1"))
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

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testSummarizeEventIncrementsCounters();
    testCounterForNilVariationIsDistinctFromOthers();

    return 0;
}
