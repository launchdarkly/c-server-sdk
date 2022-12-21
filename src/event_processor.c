#include <launchdarkly/memory.h>
#include <string.h>

#include "assertion.h"
#include "event_processor.h"
#include "event_processor_internal.h"
#include "logging.h"
#include "utility.h"
#include "time_utils.h"

struct LDEventProcessor
{
    ld_mutex_t             lock;
    struct LDJSON *        events;          /* Array of Objects */
    struct LDJSON *        summaryCounters; /* Object */
    double                 summaryStart;
    struct LDLRU *         userKeys;
    struct LDTimer         lastUserKeyFlush;
    struct LDTimestamp     lastServerTime;
    const struct LDConfig *config;
};

struct LDEventProcessor *
LDEventProcessor_Create(const struct LDConfig *config)
{
    struct LDEventProcessor *processor;

    if (!(processor =
              (struct LDEventProcessor *)LDAlloc(sizeof(struct LDEventProcessor))))
    {
        goto error;
    }

    processor->summaryStart     = 0;
    processor->config           = config;

    LDTimestamp_InitZero(&processor->lastServerTime);
    LDTimer_Reset(&processor->lastUserKeyFlush);

    LDi_mutex_init(&processor->lock);

    if (!(processor->events = LDNewArray())) {
        goto error;
    }

    if (!(processor->summaryCounters = LDNewObject())) {
        goto error;
    }

    if (!(processor->userKeys = LDLRUInit(config->userKeysCapacity))) {
        goto error;
    }

    return processor;

error:
    LDEventProcessor_Destroy(processor);

    return NULL;
}

void
LDEventProcessor_Destroy(struct LDEventProcessor *processor)
{
    if (processor) {
        LDi_mutex_destroy(&processor->lock);
        LDJSONFree(processor->events);
        LDJSONFree(processor->summaryCounters);
        LDLRUFree(processor->userKeys);
        LDFree(processor);
    }
}

LDBoolean
LDEventProcessor_ProcessEvaluation(struct LDEventProcessor *processor, struct EvaluationResult *result)
{
    struct LDJSON *indexEvent, *featureEvent;
    const struct LDJSON *evaluationValue;
    const unsigned int *variationIndexRef;
    struct LDTimestamp now;

    indexEvent        = NULL;
    featureEvent      = NULL;
    evaluationValue   = NULL;
    variationIndexRef = NULL;

    LD_ASSERT(processor);
    LD_ASSERT(result->details);

    if (!LDTimestamp_InitNow(&now)) {
        LD_LOG(LD_LOG_CRITICAL, "failed to obtain current time");

        LDJSONFree(result->subEvents);
        return LDBooleanFalse;
    }

    if (LDi_notNull(result->actualValue)) {
        evaluationValue = result->actualValue;
    } else {
        evaluationValue = result->fallbackValue;
    }

    if (result->details->hasVariation) {
        variationIndexRef = &result->details->variationIndex;
    }

    LDi_mutex_lock(&processor->lock);

    featureEvent = LDi_newFeatureEvent(
            result->flagKey,
            result->user,
            variationIndexRef,
            evaluationValue,
            result->fallbackValue,
            NULL,
            result->flag,
            result->details,
            now,
            processor->config->inlineUsersInEvents,
            processor->config->allAttributesPrivate,
            processor->config->privateAttributeNames
    );

    if (!featureEvent) {
        LDi_mutex_unlock(&processor->lock);

        LDJSONFree(result->subEvents);

        return LDBooleanFalse;
    }

    if (!LDi_maybeMakeIndexEvent(processor, result->user, now, &indexEvent)) {
        LDi_mutex_unlock(&processor->lock);

        LDJSONFree(featureEvent);
        LDJSONFree(result->subEvents);

        return LDBooleanFalse;
    }

    if (!LDi_summarizeEvent(processor, featureEvent, (LDBoolean) (!result->flag))) {
        LDi_mutex_unlock(&processor->lock);

        LDJSONFree(featureEvent);
        LDJSONFree(result->subEvents);

        return LDBooleanFalse;
    }

    if (indexEvent) {
        LDi_addEvent(processor, indexEvent);

        indexEvent = NULL;
    }

    if (result->flag) {
        LDi_possiblyQueueEvent(processor, featureEvent, now, result);

        featureEvent = NULL;
    }

    LDJSONFree(featureEvent);

    if (result->subEvents) {
        struct LDJSON *iter;
        /* local only sanity */
        LD_ASSERT(LDJSONGetType(result->subEvents) == LDArray);
        /* different loop to make cleanup easier */
        for (iter = LDGetIter(result->subEvents); iter; iter = LDIterNext(iter)) {
            /* should lock here */
            if (!LDi_summarizeEvent(processor, iter, LDBooleanFalse)) {
                LD_LOG(LD_LOG_ERROR, "summary failed");

                LDJSONFree(result->subEvents);

                LDi_mutex_unlock(&processor->lock);

                return LDBooleanFalse;
            }
        }

        for (iter = LDGetIter(result->subEvents); iter;) {
            struct LDJSON *const next = LDIterNext(iter);

            LDi_possiblyQueueEvent(
                processor,
                LDCollectionDetachIter(result->subEvents, iter),
                now,
                result);

            iter = next;
        }

        LDJSONFree(result->subEvents);
    }

    LDi_mutex_unlock(&processor->lock);

    return LDBooleanTrue;
}

LDBoolean
LDi_summarizeEvent(
    struct LDEventProcessor *const processor,
    const struct LDJSON *const   event,
    const LDBoolean              unknown)
{
    char *         keytext;
    const char *   flagKey;
    struct LDJSON *tmp, *entry, *flagContext, *counters;
    LDBoolean      success;

    LD_ASSERT(processor);
    LD_ASSERT(event);

    keytext     = NULL;
    flagKey     = NULL;
    tmp         = NULL;
    entry       = NULL;
    flagContext = NULL;
    counters    = NULL;
    success     = LDBooleanFalse;

    tmp = LDObjectLookup(event, "key");
    LD_ASSERT(tmp);
    LD_ASSERT(LDJSONGetType(tmp) == LDText);
    flagKey = LDGetText(tmp);
    LD_ASSERT(flagKey);

    if (!(keytext = LDi_makeSummaryKey(event))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return LDBooleanFalse;
    }

    if (processor->summaryStart == 0) {
        double now;

        LDi_getUnixMilliseconds(&now);

        processor->summaryStart = now;
    }

    if (!(flagContext = LDObjectLookup(processor->summaryCounters, flagKey))) {
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

        if (!LDObjectSetKey(processor->summaryCounters, flagKey, flagContext)) {
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
            if (!(tmp = LDNewBool(LDBooleanTrue))) {
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
        LDBoolean status;
        tmp = LDObjectLookup(entry, "count");
        LD_ASSERT(tmp);
        status = LDSetNumber(tmp, LDGetNumber(tmp) + 1);
        LD_ASSERT(status);
    }

    success = LDBooleanTrue;

cleanup:
    LDFree(keytext);

    return success;
}

static LDBoolean
convertToDebug(struct LDEventProcessor *processor, struct LDJSON *const event, const struct LDUser *user)
{
    struct LDJSON *kind, *inlineUser;

    LD_ASSERT(event);

    LDObjectDeleteKey(event, "kind");
    LDObjectDeleteKey(event, "userKey");

    kind = LDNewText("debug");

    if (!LDObjectSetKey(event, "kind", kind)) {
        LDJSONFree(kind);

        return LDBooleanFalse;
    }

    if (!(inlineUser = LDi_userToJSON(
            user,
            LDBooleanTrue,
            processor->config->allAttributesPrivate,
            processor->config->privateAttributeNames)))
    {
        return LDBooleanFalse;
    }

    if (!(LDObjectSetKey(event, "user", inlineUser))) {
        LDJSONFree(inlineUser);

        return LDBooleanFalse;
    }

    return LDBooleanTrue;
}

void
LDi_possiblyQueueEvent(
    struct LDEventProcessor *const processor,
    struct LDJSON *              event,
    const struct LDTimestamp     now,
    const struct EvaluationResult *result)
{
    LDBoolean      shouldTrack;
    struct LDJSON *tmp;

    LD_ASSERT(processor);
    LD_ASSERT(event);

    shouldTrack = LDBooleanFalse;

    if ((tmp = LDObjectLookup(LDObjectLookup(event, "reason"), "inExperiment")))
    {
        shouldTrack = LDGetBool(tmp);
    }

    if (LDi_notNull(tmp = LDObjectLookup(event, "shouldAlwaysTrackDetails"))) {
        if (!LDGetBool(tmp)) {
            LDObjectDeleteKey(event, "reason");
        }

        LDJSONFree(LDCollectionDetachIter(event, tmp));
    } else if (!result->detailedEvaluation) {
        LDObjectDeleteKey(event, "reason");
    }

    if (LDi_notNull(tmp = LDObjectLookup(event, "trackEvents"))) {
        /* validated as Boolean by LDi_newFeatureRequestEvent */
        if (shouldTrack == LDBooleanFalse) {
            shouldTrack = LDGetBool(tmp);
        }
        /* ensure we don't send trackEvents to LD */
        LDJSONFree(LDCollectionDetachIter(event, tmp));
    }

    if (LDi_notNull(tmp = LDObjectLookup(event, "debugEventsUntilDate"))) {
        /* debugEventsUntilDate was validated as a Number by LDi_newFeatureRequestEvent.
         * Extract it as a timestamp. */

        struct LDTimestamp debugUntil;
        LDTimestamp_InitUnixMillis(&debugUntil, LDGetNumber(tmp));

        /* ensure we don't send debugEventsUntilDate to LD */
        LDJSONFree(LDCollectionDetachIter(event, tmp));

        /* Check that the current system time is still before debugUntil, but also check the server time.
         * This is because the system time may be inaccurate. If there is no server time, the second conditional
         * will evaluate true because the server time is initialized to 0 when the EventProcessor is constructed. */

        if (LDTimestamp_Before(&now, &debugUntil) && LDTimestamp_Before(&processor->lastServerTime, &debugUntil)) {

            struct LDJSON *debugEvent = NULL;

            if (shouldTrack) {
                debugEvent = LDJSONDuplicate(event);
            } else {
                debugEvent = event;
                event      = NULL;
            }

            if (!convertToDebug(processor, debugEvent, result->user)) {
                LDJSONFree(debugEvent);

                LD_LOG(LD_LOG_WARNING, "failed to convert feature event to debug event");
            } else {
                LDi_addEvent(processor, debugEvent);
            }
        }
    }

    if (shouldTrack) {
        LDi_addEvent(processor, event);

        event = NULL;
    }

    /* consume if neither debug nor track */
    LDJSONFree(event);
}

void
LDi_addEvent(struct LDEventProcessor *const processor, struct LDJSON *const event)
{
    LD_ASSERT(processor);
    LD_ASSERT(event);

    /* sanity check */
    LD_ASSERT(LDJSONGetType(processor->events) == LDArray);

    if (LDCollectionGetSize(processor->events) >= processor->config->eventsCapacity)
    {
        LD_LOG(LD_LOG_WARNING, "event capacity exceeded, dropping event");
    } else {
        LDArrayPush(processor->events, event);
    }
}

LDBoolean
LDi_maybeMakeIndexEvent(
    struct LDEventProcessor *const processor,
    const struct LDUser *const   user,
    const struct LDTimestamp     timestamp,
    struct LDJSON **const        result)
{
    struct LDJSON *  event, *tmp;
    enum LDLRUStatus status;
    double elapsedMs;

    LD_ASSERT(processor);
    LD_ASSERT(user);
    LD_ASSERT(result);

    if (processor->config->inlineUsersInEvents) {
        *result = NULL;

        return LDBooleanTrue;
    }

    if (!LDTimer_Elapsed(&processor->lastUserKeyFlush, &elapsedMs)) {
        LD_LOG(LD_LOG_ERROR, "couldn't measure elapsed time since last user key flush");

        return LDBooleanFalse;
    }

    if (elapsedMs > processor->config->userKeysFlushInterval) {
        LDLRUClear(processor->userKeys);

        LDTimer_Reset(&processor->lastUserKeyFlush);
    }

    status = LDLRUInsert(processor->userKeys, user->key);

    if (status == LDLRUSTATUS_ERROR) {
        return LDBooleanFalse;
    } else if (status == LDLRUSTATUS_EXISTED) {
        *result = NULL;

        return LDBooleanTrue;
    }

    if (!(event = LDi_newBaseEvent("index", timestamp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return LDBooleanFalse;
    }

    if (!(tmp = LDi_userToJSON(
              user,
              LDBooleanTrue,
              processor->config->allAttributesPrivate,
              processor->config->privateAttributeNames)))
    {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(event);

        return LDBooleanFalse;
    }

    if (!(LDObjectSetKey(event, "user", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(tmp);
        LDJSONFree(event);

        return LDBooleanFalse;
    }

    *result = event;

    return LDBooleanTrue;
}

struct LDJSON *
LDi_newFeatureEvent(
    const char *const                  key,
    const struct LDUser *const         user,
    const unsigned int *const          variation,
    const struct LDJSON *const         value,
    const struct LDJSON *const         defaultValue,
    const char *const                  prereqOf,
    const struct LDJSON *const         flag,
    const struct LDDetails *const      details,
    const struct LDTimestamp           timestamp,
    LDBoolean                          inlineUsersInEvents,
    LDBoolean                          allAttributesPrivate,
    const struct LDJSON *const         privateAttributeNames)
{
    struct LDJSON *tmp, *event;
    LDBoolean      shouldTrack, shouldAlwaysDetail;

    LD_ASSERT(key);
    LD_ASSERT(user);

    tmp                = NULL;
    event              = NULL;
    shouldTrack        = LDBooleanFalse;
    shouldAlwaysDetail = LDBooleanFalse;

    if (!(event = LDi_newBaseEvent("feature", timestamp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!LDi_addUserInfoToEvent(
            event,
            user,
            inlineUsersInEvents,
            allAttributesPrivate,
            privateAttributeNames
    )) {
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
                shouldTrack = LDBooleanTrue;
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
                shouldTrack        = LDBooleanTrue;
                shouldAlwaysDetail = LDBooleanTrue;
            }
        }

        if (details->reason == LD_RULE_MATCH) {
            tmp = LDArrayLookup(
                LDObjectLookup(flag, "rules"), details->extra.rule.ruleIndex);

            if (LDi_notNull(tmp = LDObjectLookup(tmp, "trackEvents")) &&
                LDJSONGetType(tmp) == LDBool && LDGetBool(tmp) == LDBooleanTrue)
            {
                shouldTrack        = LDBooleanTrue;
                shouldAlwaysDetail = LDBooleanTrue;
            }
        }
    }

    if (shouldTrack) {
        if (!(tmp = LDNewBool(LDBooleanTrue))) {
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
        if (!(tmp = LDNewBool(LDBooleanTrue))) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            goto error;
        }

        if (!LDObjectSetKey(event, "shouldAlwaysTrackDetails", tmp)) {
            LD_LOG(LD_LOG_ERROR, "memory error");

            LDJSONFree(tmp);

            goto error;
        }
    }

    if (user->anonymous) {
        if (!(tmp = LDNewText("anonymousUser"))) {
            goto error;
        }

        if (!LDObjectSetKey(event, "contextKind", tmp)) {
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
LDi_newBaseEvent(const char *const kind, struct LDTimestamp now)
{
    struct LDJSON *tmp, *event;

    tmp   = NULL;
    event = NULL;

    if (!(event = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!(tmp = LDTimestamp_MarshalUnixMillis(&now))) {
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

LDBoolean
LDi_addUserInfoToEvent(
    struct LDJSON *const event,
    const struct LDUser *const user,
    LDBoolean inlineUsersInEvents,
    LDBoolean allAttributesPrivate,
    const struct LDJSON *privateAttributeNames)
{
    struct LDJSON *tmp;

    LD_ASSERT(event);
    LD_ASSERT(user);

    if (inlineUsersInEvents) {
        if (!(tmp = LDi_userToJSON(
                  user,
                  LDBooleanTrue,
                  allAttributesPrivate,
                  privateAttributeNames)))
        {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            return LDBooleanFalse;
        }

        if (!(LDObjectSetKey(event, "user", tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);

            return LDBooleanFalse;
        }
    } else {
        if (!(tmp = LDNewText(user->key))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            return LDBooleanFalse;
        }

        if (!(LDObjectSetKey(event, "userKey", tmp))) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDJSONFree(tmp);

            return LDBooleanFalse;
        }
    }

    return LDBooleanTrue;
}

char *
LDi_makeSummaryKey(const struct LDJSON *const event)
{
    char *         keytext;
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

LDBoolean
LDEventProcessor_Identify(
        struct LDEventProcessor *processor, const struct LDUser *user)
{
    struct LDJSON *event;
    struct LDTimestamp timestamp;

    LD_ASSERT(processor);
    LD_ASSERT(user);

    /* Users with empty keys do not generate identify events. */
    if (strlen(user->key) == 0) {

        return LDBooleanTrue;
    }

    if (!LDTimestamp_InitNow(&timestamp)) {
        LD_LOG(LD_LOG_CRITICAL, "failed to obtain current time");

        return LDBooleanFalse;
    }

    if (!(event = LDi_newIdentifyEvent(
            user,
            timestamp,
            processor->config->allAttributesPrivate,
            processor->config->privateAttributeNames))) {

        LD_LOG(LD_LOG_ERROR, "failed to construct identify event");

        return LDBooleanFalse;
    }

    LDi_mutex_lock(&processor->lock);

    LDLRUInsert(processor->userKeys, user->key);

    LDi_addEvent(processor, event);

    LDi_mutex_unlock(&processor->lock);

    return LDBooleanTrue;
}

struct LDJSON *
LDi_newIdentifyEvent(
    const struct LDUser *const user,
    struct LDTimestamp timestamp,
    LDBoolean allAttributesPrivate,
    const struct LDJSON *privateAttributeNames
) {
    struct LDJSON *event, *tmp;

    LD_ASSERT(user);

    event = NULL;
    tmp   = NULL;

    if (!(event = LDi_newBaseEvent("identify", timestamp))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct base event");

        return NULL;
    }

    if (!(tmp = LDNewText(user->key))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(event);

        return LDBooleanFalse;
    }

    if (!(LDObjectSetKey(event, "key", tmp))) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(event);
        LDJSONFree(tmp);

        return LDBooleanFalse;
    }

    if (!(tmp = LDi_userToJSON(
              user,
              LDBooleanTrue,
              allAttributesPrivate,
              privateAttributeNames)))
    {
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
    const struct LDUser *const user,
    const char* key,
    struct LDJSON* data,
    double metric,
    LDBoolean hasMetric,
    LDBoolean inlineUsersInEvents,
    LDBoolean allAttributesPrivate,
    const struct LDJSON *const privateAttributeNames,
    struct LDTimestamp timestamp)
{
    struct LDJSON *tmp, *event;

    LD_ASSERT(user);
    LD_ASSERT(key);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newBaseEvent("custom", timestamp))) {
        LD_LOG(LD_LOG_ERROR, "memory error");

        goto error;
    }

    if (!LDi_addUserInfoToEvent(
            event,
            user,
            inlineUsersInEvents,
            allAttributesPrivate,
            privateAttributeNames
    )) {
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

    if (user->anonymous) {
        if (!(tmp = LDNewText("anonymousUser"))) {
            goto error;
        }

        if (!LDObjectSetKey(event, "contextKind", tmp)) {
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
LDi_prepareSummaryEvent(struct LDEventProcessor *const processor, const double now)
{
    struct LDJSON *tmp, *summary, *iter, *counters;

    LD_ASSERT(processor);

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

    if (!(tmp = LDNewNumber(processor->summaryStart))) {
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

    if (!(counters = LDJSONDuplicate(processor->summaryCounters))) {
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

static LDBoolean
LDi_track(
        struct LDEventProcessor *processor,
        const struct LDUser *const user,
        const char *eventKey,
        struct LDJSON *data,
        double metric,
        LDBoolean hasMetric
){
    struct LDJSON *event, *indexEvent;
    struct LDTimestamp timestamp;

    LD_ASSERT(processor);
    LD_ASSERT(user);
    LD_ASSERT(eventKey);

    if (!LDTimestamp_InitNow(&timestamp)) {
        LD_LOG(LD_LOG_CRITICAL, "failed to obtain current time");

        return LDBooleanFalse;
    }

    LDi_mutex_lock(&processor->lock);

    if (!LDi_maybeMakeIndexEvent(processor, user, timestamp, &indexEvent)) {
        LD_LOG(LD_LOG_ERROR, "failed to construct index event");

        LDi_mutex_unlock(&processor->lock);

        return LDBooleanFalse;
    }

    if (!(event = LDi_newCustomEvent(
            user,
            eventKey,
            data,
            metric,
            hasMetric,
            processor->config->inlineUsersInEvents,
            processor->config->allAttributesPrivate,
            processor->config->privateAttributeNames,
            timestamp
    )))
    {
        LD_LOG(LD_LOG_ERROR, "failed to construct custom event");

        LDi_mutex_unlock(&processor->lock);

        LDJSONFree(indexEvent);

        return LDBooleanFalse;
    }

    LDi_addEvent(processor, event);

    if (indexEvent) {
        LDi_addEvent(processor, indexEvent);
    }

    LDi_mutex_unlock(&processor->lock);

    return LDBooleanTrue;
}

LDBoolean
LDEventProcessor_Track(
        struct LDEventProcessor *processor,
        const struct LDUser *user,
        const char *eventKey,
        struct LDJSON *data /* ownership transferred */
) {
    return LDi_track(processor, user, eventKey, data, 0, LDBooleanFalse);
}

LDBoolean
LDEventProcessor_TrackMetric(
        struct LDEventProcessor *processor,
        const struct LDUser *const user,
        const char *eventKey,
        struct LDJSON *data, /* ownership transferred */
        double metric
) {
    return LDi_track(processor, user, eventKey, data, metric, LDBooleanTrue);
}

static struct LDJSON *
contextKindString(const struct LDUser *const user)
{
    if (user->anonymous) {
        return LDNewText("anonymousUser");
    } else {
        return LDNewText("user");
    }
}

struct LDJSON *
LDi_newAliasEvent(
    const struct LDUser *const currentUser,
    const struct LDUser *const previousUser,
    struct LDTimestamp         timestamp)
{
    struct LDJSON *tmp, *event;

    LD_ASSERT(currentUser);
    LD_ASSERT(previousUser);

    tmp   = NULL;
    event = NULL;

    if (!(event = LDi_newBaseEvent("alias", timestamp))) {
        goto error;
    }

    if (!(tmp = LDNewText(currentUser->key))) {
        goto error;
    }

    if (!LDObjectSetKey(event, "key", tmp)) {
        goto error;
    }

    if (!(tmp = LDNewText(previousUser->key))) {
        goto error;
    }

    if (!LDObjectSetKey(event, "previousKey", tmp)) {
        goto error;
    }

    if (!(tmp = contextKindString(currentUser))) {
        goto error;
    }

    if (!LDObjectSetKey(event, "contextKind", tmp)) {
        goto error;
    }

    if (!(tmp = contextKindString(previousUser))) {
        goto error;
    }

    if (!LDObjectSetKey(event, "previousContextKind", tmp)) {
        goto error;
    }

    return event;

error:
    LDJSONFree(event);
    LDJSONFree(tmp);

    return NULL;
}

LDBoolean
LDEventProcessor_Alias(
        struct LDEventProcessor *processor,
        const struct LDUser *currentUser,
        const struct LDUser *previousUser)
{
    struct LDJSON *event;
    struct LDTimestamp timestamp;

    LD_ASSERT(processor);
    LD_ASSERT(currentUser);
    LD_ASSERT(previousUser);

    if (!LDTimestamp_InitNow(&timestamp)) {
        LD_LOG(LD_LOG_CRITICAL, "failed to obtain current time");

        return LDBooleanFalse;
    }

    if (!(event = LDi_newAliasEvent(currentUser, previousUser, timestamp))) {
        LD_LOG(LD_LOG_ERROR, "failed to construct alias event");

        return LDBooleanFalse;
    }

    LDi_mutex_lock(&processor->lock);
    LDi_addEvent(processor, event);
    LDi_mutex_unlock(&processor->lock);

    return LDBooleanTrue;
}

LDBoolean
LDEventProcessor_CreateEventPayloadAndResetState(
        struct LDEventProcessor *processor, struct LDJSON **result)
{
    struct LDJSON *nextEvents, *nextSummaryCounters, *summaryEvent;
    double         now;

    LD_ASSERT(processor);
    LD_ASSERT(result);

    nextEvents          = NULL;
    nextSummaryCounters = NULL;
    *result             = NULL;

    LDi_getUnixMilliseconds(&now);

    LDi_mutex_lock(&processor->lock);

    if (LDCollectionGetSize(processor->events) == 0 &&
        LDCollectionGetSize(processor->summaryCounters) == 0)
    {
        LDi_mutex_unlock(&processor->lock);

        /* successful but no events to send */

        return LDBooleanTrue;
    }

    if (!(nextEvents = LDNewArray())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDi_mutex_unlock(&processor->lock);

        return LDBooleanFalse;
    }

    if (LDCollectionGetSize(processor->summaryCounters) != 0) {
        if (!(nextSummaryCounters = LDNewObject())) {
            LD_LOG(LD_LOG_ERROR, "alloc error");

            LDi_mutex_unlock(&processor->lock);

            LDJSONFree(nextEvents);

            return LDBooleanFalse;
        }

        if (!(summaryEvent = LDi_prepareSummaryEvent(processor, now))) {
            LD_LOG(LD_LOG_ERROR, "failed to prepare summary");

            LDi_mutex_unlock(&processor->lock);

            LDJSONFree(nextEvents);
            LDJSONFree(nextSummaryCounters);

            return LDBooleanFalse;
        }

        LDArrayPush(processor->events, summaryEvent);

        LDJSONFree(processor->summaryCounters);

        processor->summaryStart    = 0;
        processor->summaryCounters = nextSummaryCounters;
    }

    *result         = processor->events;
    processor->events = nextEvents;

    LDi_mutex_unlock(&processor->lock);

    return LDBooleanTrue;
}

void
LDEventProcessor_SetLastServerTime(struct LDEventProcessor *processor, time_t serverTime)
{
    LD_ASSERT(processor);
    LDi_mutex_lock(&processor->lock);
    LDTimestamp_InitUnixSeconds(&processor->lastServerTime, serverTime);
    LDi_mutex_unlock(&processor->lock);
}


struct LDJSON *
LDEventProcessor_GetEvents(struct LDEventProcessor *processor) {
    return processor->events;
}

double
LDEventProcessor_GetLastServerTime(struct LDEventProcessor *processor) {
    return LDTimestamp_AsUnixMillis(&processor->lastServerTime);
}
