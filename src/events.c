#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <launchdarkly/api.h>

#include "events.h"
#include "network.h"
#include "user.h"
#include "client.h"
#include "config.h"
#include "misc.h"
#include "lru.h"

bool
LDi_maybeMakeIndexEvent(struct LDClient *const client,
    const struct LDUser *const user, struct LDJSON **result)
{
    unsigned long now;
    struct LDJSON *event, *tmp;
    enum LDLRUStatus status;

    LD_ASSERT(client);
    LD_ASSERT(user);
    LD_ASSERT(result);

    if (client->config->inlineUsersInEvents) {
        *result = NULL;

        return true;
    }

    LDi_getMonotonicMilliseconds(&now);

    LDi_rwlock_wrlock(&client->lock);
    if (now > client->lastUserKeyFlush +
        client->config->userKeysFlushInterval) {
        LDLRUClear(client->userKeys);

        client->lastUserKeyFlush = now;
    }
    status = LDLRUInsert(client->userKeys, user->key);
    LDi_rwlock_wrunlock(&client->lock);

    if (status == LDLRUSTATUS_ERROR) {
        return false;
    } else if (status == LDLRUSTATUS_EXISTED) {
        *result = NULL;

        return true;
    }

    if (!(event = LDi_newBaseEvent("index"))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return false;
    }

    if (!(tmp = LDUserToJSON(client, user, true))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(event);

        return false;
    }

    if (!(LDObjectSetKey(event, "user", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);
        LDJSONFree(event);

        return false;
    }

    *result = event;

    return true;
}

bool
LDi_addUserInfoToEvent(struct LDJSON *const event,
    struct LDClient *const client, const struct LDUser *const user)
{
    struct LDJSON *tmp;

    LD_ASSERT(event);
    LD_ASSERT(client);
    LD_ASSERT(user);

    if (client->config->inlineUsersInEvents) {
        if (!(tmp = LDUserToJSON(client, user, true))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            return false;
        }

        if (!(LDObjectSetKey(event, "user", tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);

            return false;
        }
    } else {
        if (!(tmp = LDNewText(user->key))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            return false;
        }

        if (!(LDObjectSetKey(event, "userKey", tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);

            return false;
        }
    }

    return true;
}

struct LDJSON *
LDi_newBaseEvent(const char *const kind)
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

    LDi_getUnixMilliseconds(&milliseconds);

    if (!(tmp = LDNewNumber(milliseconds))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(event, "creationDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);

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
LDi_notNull(const struct LDJSON *const json)
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
    const struct LDJSON *const flag, const struct LDDetails *const details)
{
    struct LDJSON *tmp, *event;

    LD_ASSERT(key);
    LD_ASSERT(user);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newBaseEvent("feature"))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!LDi_addUserInfoToEvent(event, client, user)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDNewText(key))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!LDObjectSetKey(event, "key", tmp)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);

        goto error;
    }

    if (variation) {
        if (!(tmp = LDNewNumber(*variation))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(event, "variation", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);

            goto error;
        }
    }

    if (value) {
        if (!(tmp = LDJSONDuplicate(value))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(event, "value", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);

            goto error;
        }
    }

    if (defaultValue) {
        if (!(tmp = LDJSONDuplicate(defaultValue))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto error;
        }

        if (!LDObjectSetKey(event, "default", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);

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

            LDJSONFree(tmp);

            goto error;
        }
    }

    if (flag) {
        if (LDi_notNull(tmp = LDObjectLookup(flag, "version"))) {
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

                LDJSONFree(tmp);

                goto error;
            }
        }

        if (LDi_notNull(tmp = LDObjectLookup(flag, "debugEventsUntilDate"))) {
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

                LDJSONFree(tmp);

                goto error;
            }
        }

        if (LDi_notNull(tmp = LDObjectLookup(flag, "trackEvents"))) {
            if (LDJSONGetType(tmp) != LDBool) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                goto error;
            }

            if (LDGetBool(tmp)) {
                if (!(tmp = LDJSONDuplicate(tmp))) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

                    goto error;
                }

                if (!LDObjectSetKey(event, "trackEvents", tmp)) {
                    LD_LOG(LD_LOG_ERROR, "memory error");

                    LDJSONFree(tmp);

                    goto error;
                }
            }
        }
    }

    if (details) {
        if (!(tmp = LDReasonToJSON(details))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }

        if (!LDObjectSetKey(event, "reason", tmp)) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            LDJSONFree(tmp);

            goto error;
        }
    }

    return event;

  error:
    LDJSONFree(event);

    return NULL;
}

struct LDJSON *
LDi_newCustomEvent(struct LDClient *const client,
    const struct LDUser *const user, const char *const key,
    struct LDJSON *const data)
{
    struct LDJSON *tmp, *event;

    LD_ASSERT(key);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newBaseEvent("custom"))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!LDi_addUserInfoToEvent(event, client, user)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDNewText(key))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!LDObjectSetKey(event, "key", tmp)) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        LDJSONFree(tmp);

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
LDi_newCustomMetricEvent(struct LDClient *const client,
    const struct LDUser *const user, const char *const key,
    struct LDJSON *const data, const double metric)
{
    struct LDJSON *tmp, *event;

    LD_ASSERT(user);
    LD_ASSERT(key);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newCustomEvent(client, user, key, data))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!(tmp = LDNewNumber(metric))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!LDObjectSetKey(event, "metricValue", tmp)) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        LDJSONFree(tmp);

        goto error;
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
    LD_ASSERT(LDUserValidate(user));

    event = NULL;
    tmp   = NULL;

    if (!(event = LDi_newBaseEvent("identify"))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct base event");

        return NULL;
    }

    if (!(tmp = LDNewText(user->key))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(event);

        return false;
    }

    if (!(LDObjectSetKey(event, "key", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(event);
        LDJSONFree(tmp);

        return false;
    }

    if (!(tmp = LDUserToJSON(client, user, true))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(event);

        return NULL;
    }

    if (!(LDObjectSetKey(event, "user", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);
        LDJSONFree(event);

        return NULL;
    }

    return event;
}

void
LDi_addEvent(struct LDClient *const client, struct LDJSON *const event)
{
    LD_ASSERT(client);
    LD_ASSERT(event);

    LDi_rwlock_wrlock(&client->lock);

    /* sanity check */
    LD_ASSERT(LDJSONGetType(client->events) == LDArray);

    if (LDCollectionGetSize(client->events) >= client->config->eventsCapacity) {
        LD_LOG(LD_LOG_WARNING, "event capacity exceeded, dropping event");

        LDi_rwlock_wrunlock(&client->lock);
    } else {
        LDArrayPush(client->events, event);

        LDi_rwlock_wrunlock(&client->lock);
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

        return NULL;
    }

    tmp = LDObjectLookup(event, "variation");

    if (LDi_notNull(tmp)) {
        LD_ASSERT(LDJSONGetType(tmp) == LDNumber);

        if (!(tmp = LDJSONDuplicate(tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);

            return NULL;
        }

        if (!LDObjectSetKey(key, "variation", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);
            LDJSONFree(tmp);

            return NULL;
        }
    }

    tmp = LDObjectLookup(event, "version");

    if (LDi_notNull(tmp)) {
        LD_ASSERT(LDJSONGetType(tmp) == LDNumber);

        if (!(tmp = LDJSONDuplicate(tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);

            return NULL;
        }

        if (!LDObjectSetKey(key, "version", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(key);
            LDJSONFree(tmp);

            return NULL;
        }
    }

    if (!(keytext = LDJSONSerialize(key))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(key);

        return NULL;
    }

    LDJSONFree(key);

    return keytext;
}

bool
LDi_summarizeEvent(struct LDClient *const client,
    const struct LDJSON *const event, const bool unknown)
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

    tmp = LDObjectLookup(event, "key");
    LD_ASSERT(tmp);
    LD_ASSERT(LDJSONGetType(tmp) == LDText);
    flagKey = LDGetText(tmp);
    LD_ASSERT(flagKey);

    if (!(keytext = LDi_makeSummaryKey(event))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return false;
    }

    LDi_rwlock_wrlock(&client->lock);

    if (client->summaryStart == 0) {
        unsigned long now;

        LDi_getUnixMilliseconds(&now);

        client->summaryStart = now;
    }

    if (!(flagContext = LDObjectLookup(client->summaryCounters, flagKey))) {
        if (!(flagContext = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        tmp = LDObjectLookup(event, "default");

        if (LDi_notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(flagContext);

                goto cleanup;
            }

            if (!LDObjectSetKey(flagContext, "default", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(tmp);
                LDJSONFree(flagContext);

                goto cleanup;
            }
        }

        if (!(tmp = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(flagContext);

            goto cleanup;
        }

        if (!LDObjectSetKey(flagContext, "counters", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);
            LDJSONFree(flagContext);

            goto cleanup;
        }

        if (!LDObjectSetKey(client->summaryCounters, flagKey, flagContext)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(flagContext);

            goto cleanup;
        }
    }

    counters = LDObjectLookup(flagContext, "counters");
    LD_ASSERT(counters);
    LD_ASSERT(LDJSONGetType(counters) == LDObject);

    if (!(entry = LDObjectLookup(counters, keytext))) {
        if (!(entry = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            goto cleanup;
        }

        if (!(tmp = LDNewNumber(1))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(entry);

            goto cleanup;
        }

        if (!LDObjectSetKey(entry, "count", tmp)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);
            LDJSONFree(entry);

            goto cleanup;
        }

        tmp = LDObjectLookup(event, "value");

        if (LDi_notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(entry);

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "value", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(tmp);
                LDJSONFree(entry);

                goto cleanup;
            }
        }

        tmp = LDObjectLookup(event, "version");

        if (LDi_notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(entry);

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "version", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(tmp);
                LDJSONFree(entry);

                goto cleanup;
            }
        }

        tmp = LDObjectLookup(event, "variation");

        if (LDi_notNull(tmp)) {
            if (!(tmp = LDJSONDuplicate(tmp))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(entry);

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "variation", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(tmp);
                LDJSONFree(entry);

                goto cleanup;
            }
        }

        if (unknown) {
            if (!(tmp = LDNewBool(true))) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(entry);

                goto cleanup;
            }

            if (!LDObjectSetKey(entry, "unknown", tmp)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                LDJSONFree(tmp);
                LDJSONFree(entry);

                goto cleanup;
            }
        }

        if (!LDObjectSetKey(counters, keytext, entry)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(entry);

            goto cleanup;
        }
    } else {
        bool status;
        tmp = LDObjectLookup(entry, "count");
        LD_ASSERT(tmp);
        status = LDSetNumber(tmp, LDGetNumber(tmp) + 1);
        LD_ASSERT(status);
    }

    success = true;

  cleanup:
    LDi_rwlock_wrunlock(&client->lock);

    LDFree(keytext);

    return success;
}

struct AnalyticsContext {
    bool active;
    unsigned long lastFlush;
    struct curl_slist *headers;
    struct LDClient *client;
    char *buffer;
    unsigned int failureTime;
    char payloadId[LD_UUID_SIZE + 1];
};

static void
resetMemory(struct AnalyticsContext *const context)
{
    LD_ASSERT(context);

    curl_slist_free_all(context->headers);
    context->headers = NULL;

    LDFree(context->buffer);
    context->buffer = NULL;

    context->failureTime = 0;
}

static void
done(struct LDClient *const client, void *const rawcontext,
    const int responseCode)
{
    struct AnalyticsContext *context;
    const bool success = responseCode == 200 || responseCode == 202;

    LD_ASSERT(client);
    LD_ASSERT(rawcontext);

    context = (struct AnalyticsContext *)rawcontext;

    context->active = false;

    LD_LOG(LD_LOG_TRACE, "events network interface called done");

    if (success) {
        LD_LOG(LD_LOG_TRACE, "event batch send successful");

        LDi_rwlock_wrlock(&client->lock);
        client->shouldFlush = false;
        LDi_rwlock_wrunlock(&client->lock);

        LDi_getMonotonicMilliseconds(&context->lastFlush);

        resetMemory(context);
    } else {
        unsigned long now;

        /* failed twice so just discard the payload */
        if (context->failureTime) {
            LD_LOG(LD_LOG_ERROR, "failed sending events twice, discarding");

            resetMemory(context);
        } else {
            LD_LOG(LD_LOG_WARNING, "failed sending events, retrying");
            /* setup waiting and retry logic */
            LDi_getMonotonicMilliseconds(&now);

            context->failureTime = now;

            curl_slist_free_all(context->headers);
            context->headers = NULL;
        }
    }
}

static void
destroy(void *const rawcontext)
{
    struct AnalyticsContext *context;

    LD_ASSERT(rawcontext);

    context = (struct AnalyticsContext *)rawcontext;

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

        LDJSONFree(tmp);

        goto error;
    }

    if (!(tmp = LDNewNumber(client->summaryStart))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(summary, "startDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);

        goto error;
    }

    LDi_getUnixMilliseconds(&now);

    if (!(tmp = LDNewNumber(now))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(summary, "endDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);

        goto error;
    }

    if (!(counters = LDJSONDuplicate(client->summaryCounters))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    for (iter = LDGetIter(counters); iter; iter = LDIterNext(iter)) {
        struct LDJSON *countersObject, *countersArray;

        countersObject = LDObjectDetachKey(iter, "counters");
        LD_ASSERT(countersObject);

        if (!(countersArray = objectToArray(countersObject))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(countersObject);

            goto error;
        }

        LDJSONFree(countersObject);

        if (!LDObjectSetKey(iter, "counters", countersArray)) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(countersArray);

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

static const char *
strnchr(const char *str, const char c, size_t len)
{
    for (; str && len; len--, str++) {
        if (*str == c) {
            return str;
        }
    }

    return NULL;
}

bool
LDi_parseRFC822(const char *const date, struct tm *tm)
{
    return strptime(date, "%a, %d %b %Y %H:%M:%S %Z", tm) != NULL;
}

/* curl spec says these may not be NULL terminated */
size_t
LDi_onHeader(const char *buffer, const size_t size,
    const size_t itemcount, void *const context)
{
    struct LDClient *client;
    const size_t total = size * itemcount;
    const char *const dateheader = "Date:";
    const size_t dateheaderlen = strlen(dateheader);
    struct tm tm;
    char datebuffer[128];
    const char *headerend;

    LD_ASSERT(context);

    client = context;

    memset(&tm, 0, sizeof(struct tm));

    /* ensures we do not segfault if not terminated */
    if (!(headerend = strnchr(buffer, '\r', total))) {
        LD_LOG(LD_LOG_ERROR, "failed to find end of header");

        return total;
    }

    /* guard segfault for very short headers */
    if (total <= dateheaderlen) {
        return total;
    }

    /* check if header is the date header */
    if (LDi_strncasecmp(buffer, dateheader, dateheaderlen) != 0) {
        return total;
    }

    buffer += dateheaderlen;

    /* skip any spaces or tabs after header type */
    while (*buffer == ' ' || *buffer == '\t') {
        buffer++;
    }

    /* copy just date segment into own buffer */
    if ((size_t)(headerend - buffer + 1) > sizeof(datebuffer)) {
        LD_LOG(LD_LOG_ERROR, "not enough room to parse date");

        return total;
    }
    strncpy(datebuffer, buffer, headerend - buffer);
    datebuffer[headerend - buffer] = 0;

    if (!LDi_parseRFC822(datebuffer, &tm)) {
        LD_LOG(LD_LOG_ERROR, "failed to extract date from server");

        return total;
    }

    LDi_rwlock_wrlock(&client->lock);
    client->lastServerTime = 1000 * (unsigned long long)mktime(&tm);
    LDi_rwlock_wrunlock(&client->lock);

    return total;

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
    bool lastFailed;

    LD_ASSERT(rawcontext);

    curl        = NULL;
    shouldFlush = false;
    mime        = "Content-Type: application/json";
    schema      = "X-LaunchDarkly-Event-Schema: 3";
    context     = (struct AnalyticsContext *)rawcontext;
    lastFailed  = context->failureTime != 0;

    /* decide if events should be sent */

    if (context->active) {
        return NULL;
    }

    if (context->failureTime) {
        unsigned long now;

        LDi_getMonotonicMilliseconds(&now);

        /* wait for one second before retrying send */
        if (now <= context->failureTime + 1000) {
            return NULL;
        }
    }

    if (!lastFailed) {
        struct LDJSON *nextEvents, *nextSummaryCounters;

        nextEvents          = NULL;
        nextSummaryCounters = NULL;

        LDi_rwlock_wrlock(&client->lock);
        if (LDCollectionGetSize(client->events) == 0 &&
            LDCollectionGetSize(client->summaryCounters) == 0)
        {
            LDi_rwlock_wrunlock(&client->lock);

            client->shouldFlush = false;

            return NULL;
        }
        shouldFlush = client->shouldFlush;
        LDi_rwlock_wrunlock(&client->lock);

        if (!shouldFlush) {
            unsigned long now;

            LDi_getMonotonicMilliseconds(&now);
            LD_ASSERT(now >= context->lastFlush);

            if (now - context->lastFlush <
                client->config->flushInterval)
            {
                return NULL;
            }
        }

        /* serialize events */

        if (!(nextEvents = LDNewArray())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            return NULL;
        }

        if (!(nextSummaryCounters = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(nextEvents);

            return NULL;
        }

        LDi_rwlock_wrlock(&client->lock);

        if (!(summaryEvent = LDi_prepareSummaryEvent(client))) {
            LD_LOG(LD_LOG_ERROR, "failed to prepare summary");

            LDi_rwlock_wrunlock(&client->lock);

            LDJSONFree(nextEvents);
            LDJSONFree(nextSummaryCounters);

            return NULL;
        }

        LDArrayPush(client->events, summaryEvent);

        if (!(context->buffer = LDJSONSerialize(client->events))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDi_rwlock_wrunlock(&client->lock);

            LDJSONFree(nextEvents);
            LDJSONFree(nextSummaryCounters);

            return NULL;
        }

        LDJSONFree(client->events);
        LDJSONFree(client->summaryCounters);

        client->summaryStart    = 0;
        client->events          = nextEvents;
        client->summaryCounters = nextSummaryCounters;

        LDi_rwlock_wrunlock(&client->lock);

        /* Only generate a UUID once per payload. We want the header to remain
        the same during a retry */
        context->payloadId[LD_UUID_SIZE] = 0;

        if (!LDi_UUIDv4(context->payloadId)) {
            LD_LOG(LD_LOG_ERROR, "failed to generate payload identifier");

            goto error;
        }
    }

    /* prepare request */

    if (snprintf(url, sizeof(url), "%s/bulk",
        client->config->eventsURI) < 0)
    {
        LD_LOG(LD_LOG_CRITICAL, "snprintf URL failed");

        return NULL;
    }

    LD_LOG_1(LD_LOG_INFO, "connection to analytics url: %s", url);

    if (!LDi_prepareShared(client->config, url, &curl, &context->headers)) {
        goto error;
    }

    if (!(context->headers = curl_slist_append(context->headers, mime))) {
        goto error;
    }

    if (!(context->headers = curl_slist_append(context->headers, schema))) {
        goto error;
    }

    {
        int status;
        /* This is done as a macro so that the string is a literal */
        #define LD_PAYLOAD_ID_HEADER "X-LaunchDarkly-Payload-ID: "

        /* do not need to add space for null termination because of sizeof */
        char payloadIdHeader[sizeof(LD_PAYLOAD_ID_HEADER) + LD_UUID_SIZE];

        status = snprintf(payloadIdHeader, sizeof(payloadIdHeader),
            "%s%s", LD_PAYLOAD_ID_HEADER, context->payloadId);
        LD_ASSERT(status == sizeof(payloadIdHeader) - 1);

        #undef LD_PAYLOAD_ID_HEADER

        if (!(context->headers = curl_slist_append(context->headers,
            payloadIdHeader)))
        {
            goto error;
        }
    }

    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, context->headers)
        != CURLE_OK)
    {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, LDi_onHeader)
        != CURLE_OK)
    {
        goto error;
    }

    if (curl_easy_setopt(curl, CURLOPT_HEADERDATA, client)
        != CURLE_OK)
    {
        goto error;
    }

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
    struct NetworkInterface *netInterface;
    struct AnalyticsContext *context;

    LD_ASSERT(client);

    netInterface = NULL;
    context      = NULL;

    if (!(netInterface =
        (struct NetworkInterface *)LDAlloc(sizeof(struct NetworkInterface))))
    {
        goto error;
    }

    if (!(context =
        (struct AnalyticsContext *)LDAlloc(sizeof(struct AnalyticsContext))))
    {
        goto error;
    }

    context->active      = false;
    context->headers     = NULL;
    context->client      = client;
    context->buffer      = NULL;
    context->failureTime = 0;

    LDi_getMonotonicMilliseconds(&context->lastFlush);

    netInterface->done     = done;
    netInterface->poll     = poll;
    netInterface->context  = context;
    netInterface->destroy  = destroy;
    netInterface->current  = NULL;

    return netInterface;

  error:
    LDFree(context);

    LDFree(netInterface);

    return NULL;
}
