#include <string.h>

#include <launchdarkly/memory.h>

#include "assertion.h"
#include "event_processor.h"
#include "event_processor_internal.h"
#include "logging.h"
#include "utility.h"

struct EventProcessor *
LDi_newEventProcessor(const struct LDConfig *const config)
{
    struct EventProcessor *context;

    if (!(context =
        (struct EventProcessor *)LDAlloc(sizeof(struct EventProcessor))))
    {
        goto error;
    }

    context->summaryStart     = 0;
    context->lastUserKeyFlush = 0;
    context->lastServerTime   = 0;
    context->config           = config;

    LDi_getMonotonicMilliseconds(&context->lastUserKeyFlush);
    LDi_mutex_init(&context->lock);

    if (!(context->events = LDNewArray())) {
        goto error;
    }

    if (!(context->summaryCounters = LDNewObject())) {
        goto error;
    }

    if (!(context->userKeys = LDLRUInit(config->userKeysCapacity))) {
        goto error;
    }

    return context;

  error:
    LDi_freeEventProcessor(context);

    return NULL;
}

void
LDi_freeEventProcessor(struct EventProcessor *const context)
{
    if (context) {
        LDi_mutex_destroy(&context->lock);
        LDJSONFree(context->events);
        LDJSONFree(context->summaryCounters);
        LDLRUFree(context->userKeys);
        LDFree(context);
    }
}

bool
LDi_processEvaluation(
    /* required */
    struct EventProcessor *const  context,
    /* required */
    const struct LDUser *const    user,
    /* optional */
    struct LDJSON *const          subEvents,
    /* required */
    const char *const             flagKey,
    /* required */
    const struct LDJSON *const    actualValue,
    /* required */
    const struct LDJSON *const    fallbackValue,
    /* optional */
    const struct LDJSON *const    flag,
    /* required */
    const struct LDDetails *const details,
    /* required */
    const bool                    detailedEvaluation
) {
    struct LDJSON *indexEvent, *featureEvent;
    const struct LDJSON *evaluationValue;
    const unsigned int *variationIndexRef;
    double now;

    indexEvent        = NULL;
    featureEvent      = NULL;
    evaluationValue   = NULL;
    variationIndexRef = NULL;

    LD_ASSERT(context);
    LD_ASSERT(details);

    LDi_getUnixMilliseconds(&now);

    if (LDi_notNull(actualValue)) {
        evaluationValue = actualValue;
    } else {
        evaluationValue = fallbackValue;
    }

    if (details->hasVariation) {
        variationIndexRef = &details->variationIndex;
    }

    LDi_mutex_lock(&context->lock);

    featureEvent = LDi_newFeatureRequestEvent(context, flagKey, user,
        variationIndexRef, evaluationValue, fallbackValue, NULL, flag,
        details, now);

    if (!featureEvent) {
        LDi_mutex_unlock(&context->lock);

        LDJSONFree(subEvents);

        return false;
    }

    if (!LDi_maybeMakeIndexEvent(context, user, now, &indexEvent)) {
        LDi_mutex_unlock(&context->lock);

        LDJSONFree(featureEvent);
        LDJSONFree(subEvents);

        return false;
    }

    if (!LDi_summarizeEvent(context, featureEvent, !flag)) {
        LDi_mutex_unlock(&context->lock);

        LDJSONFree(featureEvent);
        LDJSONFree(subEvents);

        return false;
    }

    if (indexEvent) {
        LDi_addEvent(context, indexEvent);

        indexEvent = NULL;
    }

    if (flag) {
        LDi_possiblyQueueEvent(context, featureEvent, now, detailedEvaluation);

        featureEvent = NULL;
    }

    LDJSONFree(featureEvent);

    if (subEvents) {
        struct LDJSON *iter;
        /* local only sanity */
        LD_ASSERT(LDJSONGetType(subEvents) == LDArray);
        /* different loop to make cleanup easier */
        for (iter = LDGetIter(subEvents); iter; iter = LDIterNext(iter)) {
            /* should lock here */
            if (!LDi_summarizeEvent(context, iter, false)) {
                LD_LOG(LD_LOG_ERROR, "summary failed");

                LDJSONFree(subEvents);

                LDi_mutex_unlock(&context->lock);

                return false;
            }
        }

        for (iter = LDGetIter(subEvents); iter;) {
            struct LDJSON *const next = LDIterNext(iter);

            LDi_possiblyQueueEvent(
                context,
                LDCollectionDetachIter(subEvents, iter),
                now,
                detailedEvaluation
            );

            iter = next;
        }

        LDJSONFree(subEvents);
    }

    LDi_mutex_unlock(&context->lock);

    return true;
}

bool
LDi_summarizeEvent(struct EventProcessor *const context,
    const struct LDJSON *const event, const bool unknown)
{
    char *keytext;
    const char *flagKey;
    struct LDJSON *tmp, *entry, *flagContext, *counters;
    bool success;

    LD_ASSERT(context);
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

    if (context->summaryStart == 0) {
        double now;

        LDi_getUnixMilliseconds(&now);

        context->summaryStart = now;
    }

    if (!(flagContext = LDObjectLookup(context->summaryCounters, flagKey))) {
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

        if (!LDObjectSetKey(context->summaryCounters, flagKey, flagContext)) {
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
    LDFree(keytext);

    return success;
}

static bool
convertToDebug(struct LDJSON *const event)
{
    struct LDJSON *kind;

    LD_ASSERT(event);

    LDObjectDeleteKey(event, "kind");

    if (!(kind = LDNewText("debug"))) {
        goto error;
    }

    if (!LDObjectSetKey(event, "kind", kind)) {
        LDJSONFree(kind);

        goto error;
    }

    return true;

  error:
    return false;
}

void
LDi_possiblyQueueEvent(struct EventProcessor *const context,
    struct LDJSON *event, const double now, const bool detailedEvaluation)
{
    bool shouldTrack;
    struct LDJSON *tmp;

    LD_ASSERT(context);
    LD_ASSERT(event);

    if (LDi_notNull(tmp = LDObjectLookup(event, "shouldAlwaysTrackDetails"))) {
        if (!LDGetBool(tmp)) {
            LDObjectDeleteKey(event, "reason");
        }

        LDJSONFree(LDCollectionDetachIter(event, tmp));
    } else if (!detailedEvaluation) {
        LDObjectDeleteKey(event, "reason");
    }

    if (LDi_notNull(tmp = LDObjectLookup(event, "trackEvents"))) {
        /* validated as Boolean by LDi_newFeatureRequestEvent */
        shouldTrack = LDGetBool(tmp);
        /* ensure we don't send trackEvents to LD */
        LDJSONFree(LDCollectionDetachIter(event, tmp));
    } else {
        shouldTrack = false;
    }

    if (LDi_notNull(tmp = LDObjectLookup(event, "debugEventsUntilDate"))) {
        /* validated as Number by LDi_newFeatureRequestEvent */
        const double until = LDGetNumber(tmp);
        /* ensure we don't send debugEventsUntilDate to LD */
        LDJSONFree(LDCollectionDetachIter(event, tmp));

        if (now < until && context->lastServerTime < until) {
            struct LDJSON *debugEvent = NULL;

            if (shouldTrack) {
                debugEvent = LDJSONDuplicate(event);
            } else {
                debugEvent = event;
                event      = NULL;
            }

            if (!debugEvent || !convertToDebug(debugEvent)) {
                LDi_addEvent(context, debugEvent);
            } else {
                LDJSONFree(debugEvent);

                LD_LOG(LD_LOG_WARNING, "failed to allocate debug event");
            }
        }
    }

    if (shouldTrack) {
        LDi_addEvent(context, event);

        event = NULL;
    }

    /* consume if neither debug or track */
    LDJSONFree(event);
}

void
LDi_addEvent(struct EventProcessor *const context, struct LDJSON *const event)
{
    LD_ASSERT(context);
    LD_ASSERT(event);

    /* sanity check */
    LD_ASSERT(LDJSONGetType(context->events) == LDArray);

    if (
        LDCollectionGetSize(context->events) >= context->config->eventsCapacity
    ) {
        LD_LOG(LD_LOG_WARNING, "event capacity exceeded, dropping event");
    } else {
        LDArrayPush(context->events, event);
    }
}

bool
LDi_maybeMakeIndexEvent(
    struct EventProcessor *const context,
    const struct LDUser *const   user,
    const double                 now,
    struct LDJSON **const        result
) {
    struct LDJSON *event, *tmp;
    enum LDLRUStatus status;

    LD_ASSERT(context);
    LD_ASSERT(user);
    LD_ASSERT(result);

    if (context->config->inlineUsersInEvents) {
        *result = NULL;

        return true;
    }

    if (
        now > context->lastUserKeyFlush + context->config->userKeysFlushInterval
    ) {
        LDLRUClear(context->userKeys);

        context->lastUserKeyFlush = now;
    }

    status = LDLRUInsert(context->userKeys, user->key);

    if (status == LDLRUSTATUS_ERROR) {
        return false;
    } else if (status == LDLRUSTATUS_EXISTED) {
        *result = NULL;

        return true;
    }

    if (!(event = LDi_newBaseEvent("index", now))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return false;
    }

    if (!(tmp = LDUserToJSON(context->config, user, true))) {
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

struct LDJSON *
LDi_newFeatureRequestEvent(
    const struct EventProcessor *const context,
    const char *const                  key,
    const struct LDUser *const         user,
    const unsigned int *const          variation,
    const struct LDJSON *const         value,
    const struct LDJSON *const         defaultValue,
    const char *const                  prereqOf,
    const struct LDJSON *const         flag,
    const struct LDDetails *const      details,
    const double                       now
) {
    struct LDJSON *tmp, *event;
    bool shouldTrack, shouldAlwaysDetail;

    LD_ASSERT(context);
    LD_ASSERT(key);
    LD_ASSERT(user);

    tmp                = NULL;
    event              = NULL;
    shouldTrack        = false;
    shouldAlwaysDetail = false;

    if (!(event = LDi_newBaseEvent("feature", now))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!LDi_addUserInfoToEvent(context, event, user)) {
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
                shouldTrack = true;
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

    if (details && flag) {
        if (LDi_notNull(tmp = LDObjectLookup(flag, "trackEventsFallthrough"))) {
            if (LDJSONGetType(tmp) != LDBool) {
                LD_LOG(LD_LOG_ERROR, "schema error");

                goto error;
            }

            if (LDGetBool(tmp) && details->reason == LD_FALLTHROUGH) {
                shouldTrack        = true;
                shouldAlwaysDetail = true;
            }
        }

        if (details->reason == LD_RULE_MATCH) {
            tmp = LDArrayLookup(LDObjectLookup(
                flag, "rules"), details->extra.rule.ruleIndex);

            if (LDi_notNull(tmp = LDObjectLookup(tmp, "trackEvents"))
                && LDJSONGetType(tmp) == LDBool
                && LDGetBool(tmp) == true
            ) {
                shouldTrack        = true;
                shouldAlwaysDetail = true;
            }
        }
    }

    if (shouldTrack) {
        if (!(tmp = LDNewBool(true))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }

        if (!LDObjectSetKey(event, "trackEvents", tmp)) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            LDJSONFree(tmp);

            goto error;
        }
    }

    if (shouldAlwaysDetail) {
        if (!(tmp = LDNewBool(true))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }

        if (!LDObjectSetKey(event, "shouldAlwaysTrackDetails", tmp)) {
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
LDi_newBaseEvent(const char *const kind, const double now)
{
    struct LDJSON *tmp, *event;

    tmp          = NULL;
    event        = NULL;

    if (!(event = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDNewNumber(now))) {
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
LDi_addUserInfoToEvent(
    const struct EventProcessor *const context,
    struct LDJSON *const               event,
    const struct LDUser *const         user
) {
    struct LDJSON *tmp;

    LD_ASSERT(context);
    LD_ASSERT(event);
    LD_ASSERT(user);

    if (context->config->inlineUsersInEvents) {
        if (!(tmp = LDUserToJSON(context->config, user, true))) {
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
LDi_identify(
    struct EventProcessor *const context,
    const struct LDUser *const   user
) {
    struct LDJSON *event;
    double now;

    LD_ASSERT(context);
    LD_ASSERT(user);

    LDi_getUnixMilliseconds(&now);

    if (!(event = LDi_newIdentifyEvent(context, user, now))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct identify event");

        return false;
    }

    LDi_mutex_lock(&context->lock);

    LDi_addEvent(context, event);

    LDi_mutex_unlock(&context->lock);

    return true;
}

struct LDJSON *
LDi_newIdentifyEvent(
    const struct EventProcessor *const context,
    const struct LDUser *const         user,
    const double                       now
) {
    struct LDJSON *event, *tmp;

    LD_ASSERT(context);
    LD_ASSERT(user);

    event = NULL;
    tmp   = NULL;

    if (!(event = LDi_newBaseEvent("identify", now))) {
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

    if (!(tmp = LDUserToJSON(context->config, user, true))) {
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

struct LDJSON *
LDi_newCustomEvent(
    const struct EventProcessor *const context,
    const struct LDUser *const         user,
    const char *const                  key,
    struct LDJSON *const               data,
    const double                       metric,
    const bool                         hasMetric,
    const double                       now
) {
    struct LDJSON *tmp, *event;

    LD_ASSERT(context);
    LD_ASSERT(user);
    LD_ASSERT(key);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newBaseEvent("custom", now))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!LDi_addUserInfoToEvent(context, event, user)) {
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

    if (hasMetric) {
        if (!(tmp = LDNewNumber(metric))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }

        if (!LDObjectSetKey(event, "metricValue", tmp)) {
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
LDi_objectToArray(const struct LDJSON *const object)
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
LDi_prepareSummaryEvent(
    struct EventProcessor *const context,
    const double                 now
) {
    struct LDJSON *tmp, *summary, *iter, *counters;

    LD_ASSERT(context);

    tmp      = NULL;
    summary  = NULL;
    iter     = NULL;
    counters = NULL;

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

    if (!(tmp = LDNewNumber(context->summaryStart))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(summary, "startDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);

        goto error;
    }

    if (!(tmp = LDNewNumber(now))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(LDObjectSetKey(summary, "endDate", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);

        goto error;
    }

    if (!(counters = LDJSONDuplicate(context->summaryCounters))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    for (iter = LDGetIter(counters); iter; iter = LDIterNext(iter)) {
        struct LDJSON *countersObject, *countersArray;

        countersObject = LDObjectDetachKey(iter, "counters");
        LD_ASSERT(countersObject);

        if (!(countersArray = LDi_objectToArray(countersObject))) {
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

bool
LDi_track(
    struct EventProcessor *const context,
    const struct LDUser *const   user,
    const char *const            key,
    struct LDJSON *const         data,
    const double                 metric,
    const bool                   hasMetric
) {
    struct LDJSON *event, *indexEvent;
    double now;

    LD_ASSERT(context);
    LD_ASSERT(user);

    LDi_getUnixMilliseconds(&now);

    LDi_mutex_lock(&context->lock);

    if (!LDi_maybeMakeIndexEvent(context, user, now, &indexEvent)) {
        LD_LOG(LD_LOG_ERROR, "failed to construct index event");

        LDi_mutex_unlock(&context->lock);

        return false;
    }

    if (!(event = LDi_newCustomEvent(
        context, user, key, data, metric, hasMetric, now)))
    {
        LD_LOG(LD_LOG_ERROR, "failed to construct custom event");

        LDi_mutex_unlock(&context->lock);

        LDJSONFree(indexEvent);

        return false;
    }

    LDi_addEvent(context, event);

    if (indexEvent) {
        LDi_addEvent(context, indexEvent);
    }

    LDi_mutex_unlock(&context->lock);

    return true;
}

bool
LDi_bundleEventPayload(
    struct EventProcessor *const context,
    struct LDJSON **const        result
) {
    struct LDJSON *nextEvents, *nextSummaryCounters, *summaryEvent;
    double now;

    LD_ASSERT(context);
    LD_ASSERT(result);

    nextEvents          = NULL;
    nextSummaryCounters = NULL;
    *result             = NULL;

    LDi_getUnixMilliseconds(&now);

    LDi_mutex_lock(&context->lock);

    if (LDCollectionGetSize(context->events) == 0 &&
        LDCollectionGetSize(context->summaryCounters) == 0)
    {
        LDi_mutex_unlock(&context->lock);

        /* succesful but no events to send */

        return true;
    }

    if (!(nextEvents = LDNewArray())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDi_mutex_unlock(&context->lock);

        return false;
    }

    if (!(nextSummaryCounters = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDi_mutex_unlock(&context->lock);

        LDJSONFree(nextEvents);

        return false;
    }

    if (!(summaryEvent = LDi_prepareSummaryEvent(context, now))) {
        LD_LOG(LD_LOG_ERROR, "failed to prepare summary");

        LDi_mutex_unlock(&context->lock);

        LDJSONFree(nextEvents);
        LDJSONFree(nextSummaryCounters);

        return false;
    }

    LDArrayPush(context->events, summaryEvent);

    *result = context->events;

    LDJSONFree(context->summaryCounters);

    context->summaryStart    = 0;
    context->events          = nextEvents;
    context->summaryCounters = nextSummaryCounters;

    LDi_mutex_unlock(&context->lock);

    return true;
}

void
LDi_setServerTime(
    struct EventProcessor *const context,
    const double                 serverTime
) {
    LD_ASSERT(context);
    LDi_mutex_lock(&context->lock);
    context->lastServerTime = serverTime;
    LDi_mutex_unlock(&context->lock);
}
