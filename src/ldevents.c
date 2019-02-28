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
newBaseEvent(const struct LDUser *const user)
{
    struct LDJSON *tmp;
    struct LDJSON *event;
    unsigned int milliseconds;

    LD_ASSERT(user);

    if (!(event = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    /* TODO: look at client / redaction context */
    if (!(tmp = LDUserToJSON(NULL, user, false))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(event, "user", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
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

    return event;

  error:
    LDJSONFree(event);

    return NULL;
}

struct LDJSON *
newFeatureRequestEvent(const char *const key, const struct LDUser *const user,
    const unsigned int *const variation, const struct LDJSON *const value,
    const struct LDJSON *const defaultValue, const char *const prereqOf,
    const struct LDJSON *const flag, const struct LDJSON *const reason)
{
    struct LDJSON *tmp;
    struct LDJSON *event;

    LD_ASSERT(key);
    LD_ASSERT(user);
    LD_ASSERT(value);

    if (!(event = newBaseEvent(user))) {
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
        if ((tmp = LDObjectLookup(flag, "version"))) {
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

        if ((tmp = LDObjectLookup(flag, "trackEvents"))) {
            if (LDJSONGetType(tmp) != LDBool) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                goto error;
            }

            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "memory error");

                goto error;
            }

            if (!LDObjectSetKey(event, "trackEvents", tmp)) {
                LD_LOG(LD_LOG_ERROR, "memory error");

                goto error;
            }
        }

        if ((tmp = LDObjectLookup(flag, "debugEventsUntilDate"))) {
            if (LDJSONGetType(tmp) != LDNumber) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                goto error;
            }

            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "memory error");

                goto error;
            }

            if (!LDObjectSetKey(event, "debugEventsUntilDate", tmp)) {
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
newCustomEvent(const struct LDUser *const user, const char *const key,
    struct LDJSON *const data)
{
    struct LDJSON *tmp;
    struct LDJSON *event;

    LD_ASSERT(user);
    LD_ASSERT(key);

    if (!(event = newBaseEvent(user))) {
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
newIdentifyEvent(const struct LDUser *const user)
{
    return newBaseEvent(user);
}

bool
addEvent(struct LDClient *const client, const struct LDJSON *const event)
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
makeSummaryKey(const struct LDJSON *const event)
{
    char *keytext;
    struct LDJSON *key;
    struct LDJSON *tmp;

    LD_ASSERT(event);

    if (!(key = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return false;
    }

    LD_ASSERT(tmp = LDObjectLookup(event, "key"));
    LD_ASSERT(LDJSONGetType(tmp) == LDText);

    if (!(tmp = LDJSONDuplicate(tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(key);

        return false;
    }

    if (!LDObjectSetKey(key, "key", tmp)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(key);

        return false;
    }

    if ((tmp = LDObjectLookup(event, "variation"))) {
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

    if ((tmp = LDObjectLookup(event, "version"))) {
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
summarizeEvent(struct LDClient *const client, const struct LDJSON *const event)
{
    char *keytext;
    struct LDJSON *tmp;
    struct LDJSON *entry;

    LD_ASSERT(client);
    LD_ASSERT(event);

    if (!(keytext = makeSummaryKey(event))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return false;
    }

    LD_ASSERT(LDi_wrlock(&client->lock));

    if (!(entry = LDObjectLookup(client->summary, keytext))) {
        if (!(entry = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!(tmp = LDNewNumber(1))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(entry, "count", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if ((tmp = LDObjectLookup(event, "value"))) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto error;
            }

            if (!LDObjectSetKey(entry, "value", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto error;
            }
        }

        if ((tmp = LDObjectLookup(event, "default"))) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto error;
            }

            if (!LDObjectSetKey(entry, "default", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto error;
            }
        }

        if (!(tmp = LDObjectLookup(event, "creationDate"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            goto error;
        }

        if (!(tmp = LDJSONDuplicate(tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(entry, "startDate", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!(tmp = LDJSONDuplicate(tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(entry, "endDate", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(client->summary, keytext, entry)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }
    } else {
        struct LDJSON *date;

        LD_ASSERT(tmp = LDObjectLookup(entry, "count"));
        LD_ASSERT(LDSetNumber(tmp, LDGetNumber(tmp) + 1));

        if (!(tmp = LDObjectLookup(event, "creationDate"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            goto error;
        }

        if (!(date = LDObjectLookup(entry, "startDate"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            goto error;
        }

        if (LDGetNumber(date) < LDGetNumber(tmp)) {
            LD_ASSERT(LDSetNumber(date, LDGetNumber(tmp)));
        }

        if (!(date = LDObjectLookup(entry, "endDate"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            goto error;
        }

        if (LDGetNumber(date) > LDGetNumber(tmp)) {
            LD_ASSERT(LDSetNumber(date, LDGetNumber(tmp)));
        }
    }

    LD_ASSERT(LDi_wrunlock(&client->lock));

    free(keytext);

    return true;

  error:
    LD_ASSERT(LDi_wrunlock(&client->lock));

    free(keytext);

    return false;
}

struct AnalyticsContext {
    bool active;
    unsigned int lastFlush;
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

    free(context->buffer);
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

    free(context);
}

static CURL *
poll(struct LDClient *const client, void *const rawcontext)
{
    CURL *curl = NULL;
    char url[4096];

    const char *const mime = "Content-Type: application/json";
    const char *const schema = "X-LaunchDarkly-Event-Schema: 3";

    struct AnalyticsContext *const context = rawcontext;

    LD_ASSERT(context);

    if (context->active) {
        return NULL;
    }

    {
        unsigned int now;

        LD_ASSERT(getMonotonicMilliseconds(&now));
        LD_ASSERT(now >= context->lastFlush);

        if (now - context->lastFlush < client->config->flushInterval * 1000) {
            return NULL;
        }

        LD_ASSERT(LDi_wrlock(&client->lock));

        if (LDCollectionGetSize(client->events) == 0 &&
            LDCollectionGetSize(client->summary))
        {
            LD_ASSERT(LDi_wrunlock(&client->lock));

            return NULL;
        }

        LD_ASSERT(LDi_wrunlock(&client->lock));
    }

    if (snprintf(url, sizeof(url), "%s/bulk",
        client->config->eventsURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return false;
    }

    LD_LOG(LD_LOG_INFO, "connecting to url: %s", url);

    if (!prepareShared(client->config, url, &curl, &context->headers)) {
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

    LDArrayPush(client->events, client->summary);

    if (!(context->buffer = LDJSONSerialize(client->events))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    LDJSONFree(client->events);

    if (!(client->events = LDNewArray())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LD_ASSERT(LDi_wrunlock(&client->lock));

        goto error;
    }

    if (!(client->summary = LDNewObject())) {
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
constructAnalytics(struct LDClient *const client)
{
    struct NetworkInterface *interface = NULL;
    struct AnalyticsContext *context = NULL;

    LD_ASSERT(client);

    if (!(interface = malloc(sizeof(struct NetworkInterface)))) {
        goto error;
    }

    if (!(context = malloc(sizeof(struct AnalyticsContext)))) {
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
    free(context);

    free(interface);

    return NULL;
}
