#include <launchdarkly/api.h>

#include "config.h"
#include "client.h"
#include "user.h"
#include "evaluate.h"
#include "store.h"
#include "events.h"
#include "misc.h"

void
LDDetailsInit(struct LDDetails *const details)
{
    details->variationIndex = 0;
    details->hasVariation   = false;
    details->reason         = LD_UNKNOWN;
}

void
LDDetailsClear(struct LDDetails *const details)
{
    if (details->reason == LD_RULE_MATCH) {
        LDFree(details->extra.rule.id);
    } else if (details->reason == LD_PREREQUISITE_FAILED) {
        LDFree(details->extra.prerequisiteKey);
    }

    LDDetailsInit(details);
}

const char *
LDEvalReasonKindToString(const enum LDEvalReason kind)
{
    switch (kind) {
        case LD_ERROR:               return "ERROR";
        case LD_OFF:                 return "OFF";
        case LD_PREREQUISITE_FAILED: return "PREREQUISITE_FAILED";
        case LD_TARGET_MATCH:        return "TARGET_MATCH";
        case LD_RULE_MATCH:          return "RULE_MATCH";
        case LD_FALLTHROUGH:         return "FALLTHROUGH";
        default: return NULL;
    }
}

const char *
LDEvalErrorKindToString(const enum LDEvalErrorKind kind)
{
    switch (kind) {
        case LD_CLIENT_NOT_READY:   return "CLIENT_NOT_READY";
        case LD_NULL_KEY:           return "NULL_KEY";
        case LD_STORE_ERROR:        return "STORE_ERROR";
        case LD_FLAG_NOT_FOUND:     return "FLAG_NOT_FOUND";
        case LD_USER_NOT_SPECIFIED: return "USER_NOT_SPECIFIED";
        case LD_MALFORMED_FLAG:     return "MALFORMED_FLAG";
        case LD_WRONG_TYPE:         return "WRONG_TYPE";
        default: return NULL;
    }
}

struct LDJSON *
LDReasonToJSON(const struct LDDetails *const details)
{
    struct LDJSON *result, *tmp;
    const char *kind;

    LD_ASSERT(details);

    result = NULL;

    if (!(result = LDNewObject())) {
        goto error;
    }

    if (!(kind = LDEvalReasonKindToString(details->reason))) {
        LD_LOG(LD_LOG_ERROR, "cannot find kind");

        goto error;
    }

    if (!(tmp = LDNewText(kind))) {
        goto error;
    }

    if (!LDObjectSetKey(result, "kind", tmp)) {
        LDJSONFree(tmp);

        goto error;
    }

    if (details->reason == LD_ERROR) {
        if (!(kind = LDEvalErrorKindToString(details->extra.errorKind))) {
            LD_LOG(LD_LOG_ERROR, "cannot find kind");

            goto error;
        }

        if (!(tmp = LDNewText(kind))) {
            goto error;
        }

        if (!LDObjectSetKey(result, "errorKind", tmp)) {
            LDJSONFree(tmp);

            goto error;
        }
    } else if (details->reason == LD_PREREQUISITE_FAILED &&
        details->extra.prerequisiteKey)
    {
        if (!(tmp = LDNewText(details->extra.prerequisiteKey))) {
            goto error;
        }

        if (!LDObjectSetKey(result, "prerequisiteKey", tmp)) {
            LDJSONFree(tmp);

            goto error;
        }
    } else if (details->reason == LD_RULE_MATCH) {
        if (details->extra.rule.id) {
            if (!(tmp = LDNewText(details->extra.rule.id))) {
                goto error;
            }

            if (!LDObjectSetKey(result, "id", tmp)) {
                LDJSONFree(tmp);

                goto error;
            }
        }

        if (!(tmp = LDNewNumber(details->extra.rule.ruleIndex))) {
            goto error;
        }

        if (!LDObjectSetKey(result, "ruleIndex", tmp)) {
            LDJSONFree(tmp);

            goto error;
        }
    }

    return result;

  error:
    LDJSONFree(result);

    return NULL;
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

static void
possiblyQueueEvent(struct LDClient *const client,
    struct LDJSON *event)
{
    bool shouldTrack;
    struct LDJSON *tmp;

    LD_ASSERT(client);
    LD_ASSERT(event);

    if (LDi_notNull(tmp = LDObjectLookup(event, "trackEvents"))) {
        /* validated as Boolean by LDi_newFeatureRequestEvent */
        shouldTrack = LDGetBool(tmp);
        /* ensure we don't send trackEvents to LD */
        LDJSONFree(LDCollectionDetachIter(event, tmp));
    } else {
        shouldTrack = false;
    }

    if (LDi_notNull(tmp = LDObjectLookup(event, "debugEventsUntilDate"))) {
        unsigned long now;
        /* validated as Number by LDi_newFeatureRequestEvent */
        const unsigned long until = LDGetNumber(tmp);
        /* ensure we don't send debugEventsUntilDate to LD */
        LDJSONFree(LDCollectionDetachIter(event, tmp));

        if (LDi_getUnixMilliseconds(&now)) {
            unsigned long servertime;

            LD_ASSERT(LDi_rdlock(&client->lock));
            servertime = client->lastServerTime;
            LD_ASSERT(LDi_rdunlock(&client->lock));

            if (now < until && servertime < until) {
                struct LDJSON *debugEvent = NULL;

                if (shouldTrack) {
                    debugEvent = LDJSONDuplicate(event);
                } else {
                    debugEvent = event;
                    event      = NULL;
                }

                if (!debugEvent || !convertToDebug(debugEvent)) {
                    LDi_addEvent(client, debugEvent);
                } else {
                    LDJSONFree(debugEvent);

                    LD_LOG(LD_LOG_WARNING, "failed to allocate debug event");
                }
            }
        } else {
            LD_LOG(LD_LOG_WARNING,
                "failed to get time not recording debug event");
        }
    }

    if (shouldTrack) {
        LDi_addEvent(client, event);

        event = NULL;
    }

    /* consume if neither debug or track */
    LDJSONFree(event);
}

static struct LDJSON *
variation(struct LDClient *const client, const struct LDUser *const user,
    const char *const key, struct LDJSON *const fallback,
    const LDJSONType type, struct LDDetails *const o_details)
{
    EvalStatus status;
    unsigned int *variationNumRef;
    struct LDStore *store;
    struct LDJSON *flag, *value, *events, *event, *evalue;
    struct LDDetails details, *detailsref;
    struct LDJSONRC *flagrc;

    LD_ASSERT(client);

    flag             = NULL;
    flagrc           = NULL;
    value            = NULL;
    store            = NULL;
    events           = NULL;
    event            = NULL;
    variationNumRef  = NULL;
    evalue           = NULL;

    LDDetailsInit(&details);

    if (o_details) {
        detailsref = o_details;
        LDDetailsInit(detailsref);
    } else {
        detailsref = &details;
    }

    if (!LDClientIsInitialized(client)) {
        detailsref->reason = LD_ERROR;
        detailsref->extra.errorKind = LD_CLIENT_NOT_READY;

        goto error;
    }

    if (!key) {
        detailsref->reason = LD_ERROR;
        detailsref->extra.errorKind = LD_NULL_KEY;

        goto error;
    }

    LD_ASSERT(store = client->config->store);

    if (!LDStoreGet(store, LD_FLAG, key, &flagrc)) {
        detailsref->reason = LD_ERROR;
        detailsref->extra.errorKind = LD_STORE_ERROR;

        status = EVAL_MISS;
    }

    if (flagrc) {
        flag = LDJSONRCGet(flagrc);
    }

    if (!flag) {
        detailsref->reason = LD_ERROR;
        detailsref->extra.errorKind = LD_FLAG_NOT_FOUND;

        status = EVAL_MISS;
    } else if (!user || !user->key) {
        detailsref->reason = LD_ERROR;
        detailsref->extra.errorKind = LD_USER_NOT_SPECIFIED;

        status = EVAL_MISS;
    } else {
        status = LDi_evaluate(client, flag, user, store,
            detailsref, &events, &value, o_details != NULL);
    }

    if (status == EVAL_MEM) {
        goto error;
    }

    if (status == EVAL_SCHEMA) {
        detailsref->reason = LD_ERROR;
        detailsref->extra.errorKind = LD_MALFORMED_FLAG;

        goto error;
    }

    if (events) {
        struct LDJSON *iter;
        /* local only sanity */
        LD_ASSERT(LDJSONGetType(events) == LDArray);
        /* different loop to make cleanup easier */
        for (iter = LDGetIter(events); iter; iter = LDIterNext(iter)) {
            if (!LDi_summarizeEvent(client, iter, false)) {
                LD_LOG(LD_LOG_ERROR, "summary failed");

                goto error;
            }
        }

        for (iter = LDGetIter(events); iter;) {
            struct LDJSON *const next = LDIterNext(iter);

            possiblyQueueEvent(client, LDCollectionDetachIter(events, iter));

            iter = next;
        }

        LDJSONFree(events);
        events = NULL;
    }

    if (detailsref->hasVariation) {
        variationNumRef = &detailsref->variationIndex;
    }

    if (!LDi_notNull(evalue = value)) {
        evalue = fallback;
    }

    {
        struct LDDetails *optionalDetails = NULL;

        if (o_details) {
            optionalDetails = detailsref;
        }

        event = LDi_newFeatureRequestEvent(client, key, user, variationNumRef,
            evalue, fallback, NULL, flag, optionalDetails);
    }

    if (!event) {
        LD_LOG(LD_LOG_ERROR, "failed to build feature request event");

        goto error;
    }

    if (!LDi_summarizeEvent(client, event, !flag)) {
        LD_LOG(LD_LOG_ERROR, "summary failed");

        goto error;
    }

    if (flag) {
        possiblyQueueEvent(client, event);
        event = NULL;
    }

    if (!LDi_notNull(value)) {
        goto error;
    }

    if (LDJSONGetType(value) != type) {
        detailsref->reason = LD_ERROR;
        detailsref->extra.errorKind = LD_WRONG_TYPE;

        goto error;
    }

    LDJSONFree(event);
    LDJSONFree(fallback);
    LDDetailsClear(&details);
    LDJSONFree(events);
    LDJSONRCDecrement(flagrc);

    return value;

  error:
    LDJSONFree(event);
    LDJSONFree(value);
    LDDetailsClear(&details);
    LDJSONFree(events);
    LDJSONRCDecrement(flagrc);

    return fallback;
}

bool
LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback,
    struct LDDetails *const details)
{
    bool value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (!(fallbackJSON = LDNewBool(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, LDBool, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        return fallback;
    }

    value = LDGetBool(result);

    LDJSONFree(result);

    return value;
}

int
LDIntVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const int fallback,
    struct LDDetails *const details)
{
    int value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (!(fallbackJSON = LDNewNumber(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, LDNumber, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        return fallback;
    }

    value = LDGetNumber(result);

    LDJSONFree(result);

    return value;
}

double
LDDoubleVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const double fallback,
    struct LDDetails *const details)
{
    double value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (!(fallbackJSON = LDNewNumber(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, LDNumber, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        return fallback;
    }

    value = LDGetNumber(result);

    LDJSONFree(result);

    return value;
}

char *
LDStringVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const char* const fallback,
    struct LDDetails *const details)
{
    char *value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;
    value        = NULL;

    if (fallback && !(fallbackJSON = LDNewText(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        if (fallback) {
            return LDStrDup(fallback);
        } else {
            return NULL;
        }
    }

    result = variation(client, user, key, fallbackJSON, LDText, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        if (fallback) {
            LDStrDup(fallback);
        } else {
            return NULL;
        }
    }

    /* never mutate just type hack */
    value = (char *)LDGetText(result);

    if (value) {
        value = LDStrDup(value);
    }

    LDJSONFree(result);

    return value;
}

struct LDJSON *
LDJSONVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const struct LDJSON *const fallback,
    struct LDDetails *const details)
{
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (fallback && !(fallbackJSON = LDJSONDuplicate(fallback))) {
        LD_LOG(LD_LOG_ERROR, "allocation error");

        return NULL;
    }

    result = variation(client, user, key, fallbackJSON, LDObject, details);

    if (!result) {
        LD_LOG(LD_LOG_ERROR, "LDVariation internal failure");

        if (fallback) {
            return LDJSONDuplicate(fallback);
        } else {
            return NULL;
        }
    }

    return result;
}

struct LDJSON *
LDAllFlags(struct LDClient *const client, struct LDUser *const user)
{
    struct LDJSON *evaluatedFlags;
    struct LDJSONRC **rawFlags, **rawFlagsIter;

    LD_ASSERT(client);

    rawFlags       = NULL;
    rawFlagsIter   = NULL;
    evaluatedFlags = NULL;

    if (client->config->offline) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlags called when offline returning NULL");

        return NULL;
    }

    if (!user || !user->key) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlags NULL user returning NULL");

        return NULL;
    }

    if (!client->initialized) {
        if (LDStoreInitialized(client->config->store)) {
            LD_LOG(LD_LOG_WARNING, "LDAllFlags using stale values");
        } else {
            LD_LOG(LD_LOG_WARNING, "LDAllFlags not initialized returning NULL");

            return NULL;
        }
    }

    if (!(evaluatedFlags = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return NULL;
    }

    if (!LDStoreAll(client->config->store, LD_FLAG, &rawFlags)) {
        LD_LOG(LD_LOG_ERROR, "LDAllFlags failed to fetch flags");

        LDJSONFree(evaluatedFlags);

        return NULL;
    }

    for (rawFlagsIter = rawFlags; *rawFlagsIter; rawFlagsIter++) {
        struct LDJSON *value, *events;
        EvalStatus status;
        struct LDDetails details;
        struct LDJSON *flag;
        const char *key;

        value   = NULL;
        events  = NULL;
        key     = NULL;

        LD_ASSERT(flag = LDJSONRCGet(*rawFlagsIter));

        LDDetailsInit(&details);

        status = LDi_evaluate(client, flag, user, client->config->store,
            &details, &events, &value, false);

        if (LDi_isEvalError(status)) {
            LDJSONFree(events);
            LDDetailsClear(&details);

            goto error;
        }

        LD_ASSERT(key = LDGetText(LDObjectLookup(flag, "key")));

        if (value) {
            if (!LDObjectSetKey(evaluatedFlags, key, value)) {
                LDJSONFree(events);
                LDJSONFree(value);
                LDDetailsClear(&details);

                goto error;
            }
        }

        LDJSONFree(events);
        LDDetailsClear(&details);

        LDJSONRCDecrement(*rawFlagsIter);
    }

    LDFree(rawFlags);

    return evaluatedFlags;

  error:
    for (;*rawFlagsIter; rawFlagsIter++) {
        LDJSONRCDecrement(*rawFlagsIter);
    }
    LDFree(rawFlags);

    LDJSONFree(evaluatedFlags);

    return NULL;
}
