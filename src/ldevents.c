#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ldapi.h"
#include "ldinternal.h"
#include "ldjson.h"
#include "ldevents.h"
#include "ldnetwork.h"

struct LDJSON *
LDi_newBaseEvent(struct LDClient *const client,
    const struct LDUser *const user, const char *const kind)
{
    struct LDJSON *tmp, *event;
    unsigned long milliseconds;

    tmp          = NULL;
    event        = NULL;
    milliseconds = 0;

    if (!(event = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (user) {
        if (!(tmp = LDUserToJSON(client, user, true))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!(LDObjectSetKey(event, "user", tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }
    }

    if (!getUnixMilliseconds(&milliseconds)) {
        LD_LOG(LD_LOG_ERROR, "failed to get time");

        goto error;
    }

    if (!(tmp = LDNewNumber(milliseconds))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(event, "creationDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDNewText(kind))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(event, "kind", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    return event;

  error:
    LDJSONFree(event);

    return NULL;
}

bool
notNull(const struct LDJSON *const json)
{
    if (json) {
        if (LDJSONGetType(json) != LDNull) {
            return true;
        }
    }

    return false;
}

struct LDJSON *
LDi_newFeatureRequestEvent(struct LDClient *const client,
    const char *const key, const struct LDUser *const user,
    const unsigned int *const variation, const struct LDJSON *const value,
    const struct LDJSON *const defaultValue, const char *const prereqOf,
    const struct LDJSON *const flag, const struct LDJSON *const reason)
{
    struct LDJSON *tmp, *event;

    LD_ASSERT(key);
    LD_ASSERT(value);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newBaseEvent(client, user, "feature"))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDNewText(key))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!LDObjectSetKey(event, "key", tmp)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (variation) {
        if (!(tmp = LDNewNumber(*variation))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(event, "variation", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }
    }

    if (!(tmp = LDJSONDuplicate(value))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!LDObjectSetKey(event, "value", tmp)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (defaultValue) {
        if (!(tmp = LDJSONDuplicate(defaultValue))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(event, "default", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }
    }

    if (prereqOf) {
        if (!(tmp = LDNewText(prereqOf))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(event, "prereqOf", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }
    }

    if (flag) {
        tmp = LDObjectLookup(flag, "version");

        if (notNull(tmp)) {
            if (LDJSONGetType(tmp) != LDNumber) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                goto error;
            }

            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "memory error");

                goto error;
            }

            if (!LDObjectSetKey(event, "version", tmp)) {
                LD_LOG(LD_LOG_ERROR, "memory error");

                goto error;
            }
        }
    }

    if (reason) {
        if (!(tmp = LDJSONDuplicate(reason))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }

        if (!LDObjectSetKey(event, "reason", tmp)) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }
    }

    return event;

  error:
    LDJSONFree(event);

    return NULL;
}

struct LDJSON *
LDi_newCustomEvent(struct LDClient *const client, const struct LDUser *const user,
    const char *const key, struct LDJSON *const data)
{
    struct LDJSON *tmp, *event;

    LD_ASSERT(user);
    LD_ASSERT(key);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newBaseEvent(client, user, "custom"))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!(tmp = LDNewText(key))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!LDObjectSetKey(event, "key", tmp)) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (data) {
        if (!LDObjectSetKey(event, "data", data)) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }
    }

    return event;

  error:
    LDJSONFree(event);

    return NULL;
}

struct LDJSON *
newIdentifyEvent(struct LDClient *const client, const struct LDUser *const user)
{
    struct LDJSON *event, *tmp;

    LD_ASSERT(client);
    LD_ASSERT(user);

    event = NULL;
    tmp   = NULL;

    if (!(event = LDi_newBaseEvent(client, user, "identify"))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct base event");

        return NULL;
    }

    if (user->key) {
        if (!(tmp = LDNewText(user->key))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(event);

            return NULL;
        }

        if (!LDObjectSetKey(event, "key", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(event);

            return NULL;
        }
    }

    return event;
}

bool
LDi_addEvent(struct LDClient *const client, const struct LDJSON *const event)
{
    LD_ASSERT(client);
    LD_ASSERT(event);

    LD_ASSERT(LDi_wrlock(&client->lock));

    /* sanity check */
    LD_ASSERT(LDJSONGetType(client->events) == LDArray);

    if (LDCollectionGetSize(client->events) > client->config->eventsCapacity) {
        LD_LOG(LD_LOG_WARNING, "event capacity exceeded, dropping event");

        LD_ASSERT(LDi_wrunlock(&client->lock));

        return true;
    } else {
        struct LDJSON *dupe;

        if (!(dupe = LDJSONDuplicate(event))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LD_ASSERT(LDi_wrunlock(&client->lock));

            return false;
        }

        LDArrayPush(client->events, dupe);

        LD_ASSERT(LDi_wrunlock(&client->lock));

        return true;
    }
}

char *
LDi_makeSummaryKey(const struct LDJSON *const event)
{
    char *keytext;
    struct LDJSON *key, *tmp;

    LD_ASSERT(event);

    tmp     = NULL;
    keytext = NULL;
    key     = NULL;

    if (!(key = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return false;
    }

    tmp = LDObjectLookup(event, "variation");

    if (notNull(tmp)) {
        LD_ASSERT(LDJSONGetType(tmp) == LDNumber);

        if (!(tmp = LDJSONDuplicate(tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);

            return false;
        }

        if (!LDObjectSetKey(key, "variation", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);

            return false;
        }
    }

    tmp = LDObjectLookup(event, "version");

    if (notNull(tmp)) {
        LD_ASSERT(LDJSONGetType(tmp) == LDNumber);

        if (!(tmp = LDJSONDuplicate(tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);

            return false;
        }

        if (!LDObjectSetKey(key, "version", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);

            return false;
        }
    }

    if (!(keytext = LDJSONSerialize(key))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(key);

        return false;
    }

    LDJSONFree(key);

    return keytext;
}

bool
LDi_summarizeEvent(struct LDClient *const client, const struct LDJSON *const event,
    const bool unknown)
{
    char *keytext;
    const char *flagKey;
    struct LDJSON *tmp, *entry, *flagContext, *counters;
    bool success;

    LD_ASSERT(client);
    LD_ASSERT(event);

    keytext     = NULL;
    flagKey     = NULL;
    tmp         = NULL;
    entry       = NULL;
    flagContext = NULL;
    counters    = NULL;
    success     = false;

    LD_ASSERT(tmp = LDObjectLookup(event, "key"));
    LD_ASSERT(LDJSONGetType(tmp) == LDText);
    LD_ASSERT(flagKey = LDGetText(tmp));

    if (!(keytext = LDi_makeSummaryKey(event))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return false;
    }

    LD_ASSERT(LDi_wrlock(&client->lock));

    if (client->summaryStart == 0) {
        unsigned long now;

        LD_ASSERT(getUnixMilliseconds(&now));

        client->summaryStart = now;
    }

    if (!(flagContext = LDObjectLookup(client->summaryCounters, flagKey))) {
        if (!(flagContext = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        tmp = LDObjectLookup(event, "default");

        if (notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }

            if (!LDObjectSetKey(flagContext, "default", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }
        }

        if (!(tmp = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        if (!LDObjectSetKey(flagContext, "counters", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        if (!LDObjectSetKey(client->summaryCounters, flagKey, flagContext)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }
    }

    LD_ASSERT(counters = LDObjectLookup(flagContext, "counters"));
    LD_ASSERT(LDJSONGetType(counters) == LDObject);

    if (!(entry = LDObjectLookup(counters, keytext))) {
        if (!(entry = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        if (!(tmp = LDNewNumber(1))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        if (!LDObjectSetKey(entry, "count", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        tmp = LDObjectLookup(event, "value");

        if (notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "value", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }
        }

        tmp = LDObjectLookup(event, "version");

        if (notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "version", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }
        }

        tmp = LDObjectLookup(event, "variation");

        if (notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "variation", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }
        }

        if (unknown) {
            if (!(tmp = LDNewBool(true))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "unknown", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto cleanup;
            }
        }

        if (!LDObjectSetKey(counters, keytext, entry)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }
    } else {
        LD_ASSERT(tmp = LDObjectLookup(entry, "count"));
        LD_ASSERT(LDSetNumber(tmp, LDGetNumber(tmp) + 1));
    }

    success = true;

  cleanup:
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LDFree(keytext);

    return success;
}

struct AnalyticsContext {
    bool active;
    unsigned long lastFlush;
    struct curl_slist *headers;
    struct LDClient *client;
    char *buffer;
};

static void
resetMemory(struct AnalyticsContext *const context)
{
    LD_ASSERT(context);

    curl_slist_free_all(context->headers);
    context->headers = NULL;

    LDFree(context->buffer);
    context->buffer = NULL;
}

static void
done(struct LDClient *const client, void *const rawcontext)
{
    struct AnalyticsContext *const context = rawcontext;

    LD_ASSERT(client);
    LD_ASSERT(context);

    LD_LOG(LD_LOG_INFO, "done!");

    context->active = false;

    LD_ASSERT(LDi_wrlock(&client->lock));
    client->shouldFlush = false;
    client->initialized = true;
    LD_ASSERT(LDi_wrunlock(&client->lock));

    LD_ASSERT(getMonotonicMilliseconds(&context->lastFlush));

    resetMemory(context);
}

static void
destroy(void *const rawcontext)
{
    struct AnalyticsContext *const context = rawcontext;

    LD_ASSERT(context);

    LD_LOG(LD_LOG_INFO, "analytics destroyed");

    resetMemory(context);

    LDFree(context);
}

static struct LDJSON *
objectToArray(const struct LDJSON *const object)
{
    struct LDJSON *iter, *array;

    LD_ASSERT(object);
    LD_ASSERT(LDJSONGetType(object) == LDObject);

    iter  = NULL;
    array = NULL;

    if (!(array = LDNewArray())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return NULL;
    }

    for (iter = LDGetIter(object); iter; iter = LDIterNext(iter)) {
        struct LDJSON *dupe;

        if (!(dupe = LDJSONDuplicate(iter))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(array);

            return NULL;
        }

        LDArrayPush(array, dupe);
    }

    return array;
}

struct LDJSON *
LDi_prepareSummaryEvent(struct LDClient *const client)
{
    unsigned long now;
    struct LDJSON *tmp, *summary, *iter, *counters;

    LD_ASSERT(client);

    tmp      = NULL;
    summary  = NULL;
    iter     = NULL;
    counters = NULL;
    now      = 0;

    if (!(summary = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDNewText("summary"))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(summary, "kind", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDNewNumber(client->summaryStart))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(summary, "startDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!getUnixMilliseconds(&now)) {
        LD_LOG(LD_LOG_ERROR, "failed to get time");

        goto error;
    }

    if (!(tmp = LDNewNumber(now))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(summary, "endDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(counters = LDJSONDuplicate(client->summaryCounters))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    for (iter = LDGetIter(counters); iter; iter = LDIterNext(iter)) {
        struct LDJSON *countersObject, *countersArray;

        LD_ASSERT(countersObject = LDObjectDetachKey(iter, "counters"));

        if (!(countersArray = objectToArray(countersObject))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(iter, "counters", countersArray)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }
    }

    if (!LDObjectSetKey(summary, "features", counters)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    return summary;

  error:
    LDJSONFree(summary);
    LDJSONFree(counters);

    return NULL;
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *curl;
    struct AnalyticsContext *context;
    char url[4096];
    const char *mime, *schema;
    bool shouldFlush;
    struct LDJSON *summaryEvent;

    LD_ASSERT(rawcontext);

    curl        = NULL;
    shouldFlush = false;
    mime        = "Content-Type: application/json";
    schema      = "X-LaunchDarkly-Event-Schema: 3";
    context     = rawcontext;

    /* decide if events should be sent */

    if (context->active) {
        return NULL;
    }

    LD_ASSERT(LDi_wrlock(&client->lock));
    if (LDCollectionGetSize(client->events) == 0 &&
        LDCollectionGetSize(client->summaryCounters) == 0)
    {
        LD_ASSERT(LDi_wrunlock(&client->lock));

        client->shouldFlush = false;

        return NULL;
    }
    shouldFlush = client->shouldFlush;
    LD_ASSERT(LDi_wrunlock(&client->lock));

    if (!shouldFlush) {
        unsigned long now;

        LD_ASSERT(getMonotonicMilliseconds(&now));
        LD_ASSERT(now >= context->lastFlush);

        if (now - context->lastFlush < client->config->flushInterval * 1000) {
            return NULL;
        }
    }

    /* prepare request */

    if (snprintf(url, sizeof(url), "%s/bulk",
        client->config->eventsURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return false;
    }

    /* LD_LOG(LD_LOG_INFO, "connecting to url: %s", url); */

    if (!LDi_prepareShared(client->config, url, &curl, &context->headers)) {
        goto error;
    }

    if (!(context->headers = curl_slist_append(context->headers, mime))) {
        goto error;
    }

    if (!(context->headers = curl_slist_append(context->headers, schema))) {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, context->headers)
        != CURLE_OK)
    {
        goto error;
    }

    /* serialize events */

    LD_ASSERT(LDi_wrlock(&client->lock));

    if (!(summaryEvent = LDi_prepareSummaryEvent(client))) {
        LD_LOG(LD_LOG_ERROR, "failed to prepare summary");

        LD_ASSERT(LDi_wrunlock(&client->lock));

        goto error;
    }

    LDArrayPush(client->events, summaryEvent);

    if (!(context->buffer = LDJSONSerialize(client->events))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LD_ASSERT(LDi_wrunlock(&client->lock));

        goto error;
    }

    LDJSONFree(client->events);

    client->summaryStart = 0;

    if (!(client->events = LDNewArray())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LD_ASSERT(LDi_wrunlock(&client->lock));

        goto error;
    }

    if (!(client->summaryCounters = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LD_ASSERT(LDi_wrunlock(&client->lock));

        goto error;
    }

    LD_ASSERT(LDi_wrunlock(&client->lock));

    /* add outgoing buffer */

    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, context->buffer)
        != CURLE_OK)
    {
        goto error;
    }

    context->active = true;

    return curl;

  error:
    curl_slist_free_all(context->headers);

    curl_easy_cleanup(curl);

    return NULL;
}

struct NetworkInterface *
LDi_constructAnalytics(struct LDClient *const client)
{
    struct NetworkInterface *interface;
    struct AnalyticsContext *context;

    LD_ASSERT(client);

    interface = NULL;
    context   = NULL;

    if (!(interface = LDAlloc(sizeof(struct NetworkInterface)))) {
        goto error;
    }

    if (!(context = LDAlloc(sizeof(struct AnalyticsContext)))) {
        goto error;
    }

    context->active    = false;
    context->lastFlush = 0;
    context->headers   = NULL;
    context->client    = client;
    context->buffer    = NULL;

    interface->done    = done;
    interface->poll    = poll;
    interface->context = context;
    interface->destroy = destroy;
    interface->context = context;
    interface->current = NULL;

    return interface;

  error:
    LDFree(context);

    LDFree(interface);

    return NULL;
}
