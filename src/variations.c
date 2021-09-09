#include <launchdarkly/api.h>

#include "assertion.h"
#include "client.h"
#include "config.h"
#include "evaluate.h"
#include "store.h"
#include "user.h"
#include "utility.h"

void
LDDetailsInit(struct LDDetails *const details)
{
    LD_ASSERT_API(details);

    if (details == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDDetailsInit NULL details");

        return;
    }

    details->variationIndex = 0;
    details->hasVariation   = LDBooleanFalse;
    details->reason         = LD_UNKNOWN;
}

void
LDDetailsClear(struct LDDetails *const details)
{
    if (details) {
        if (details->reason == LD_RULE_MATCH) {
            LDFree(details->extra.rule.id);
        } else if (details->reason == LD_PREREQUISITE_FAILED) {
            LDFree(details->extra.prerequisiteKey);
        }

        LDDetailsInit(details);
    }
}

const char *
LDEvalReasonKindToString(const enum LDEvalReason kind)
{
    switch (kind) {
    case LD_ERROR:
        return "ERROR";
    case LD_OFF:
        return "OFF";
    case LD_PREREQUISITE_FAILED:
        return "PREREQUISITE_FAILED";
    case LD_TARGET_MATCH:
        return "TARGET_MATCH";
    case LD_RULE_MATCH:
        return "RULE_MATCH";
    case LD_FALLTHROUGH:
        return "FALLTHROUGH";
    default:
        return NULL;
    }
}

const char *
LDEvalErrorKindToString(const enum LDEvalErrorKind kind)
{
    switch (kind) {
    case LD_CLIENT_NOT_READY:
        return "CLIENT_NOT_READY";
    case LD_NULL_KEY:
        return "NULL_KEY";
    case LD_STORE_ERROR:
        return "STORE_ERROR";
    case LD_FLAG_NOT_FOUND:
        return "FLAG_NOT_FOUND";
    case LD_USER_NOT_SPECIFIED:
        return "USER_NOT_SPECIFIED";
    case LD_CLIENT_NOT_SPECIFIED:
        return "CLIENT_NOT_SPECIFIED";
    case LD_MALFORMED_FLAG:
        return "MALFORMED_FLAG";
    case LD_WRONG_TYPE:
        return "WRONG_TYPE";
    default:
        return NULL;
    }
}

struct LDJSON *
LDReasonToJSON(const struct LDDetails *const details)
{
    struct LDJSON *result, *tmp;
    const char *   kind;

    LD_ASSERT_API(details);

    if (details == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDReasonToJSON NULL details");

        return NULL;
    }

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
    } else if (
        details->reason == LD_PREREQUISITE_FAILED &&
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

            if (!LDObjectSetKey(result, "ruleId", tmp)) {
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

        if (details->extra.rule.inExperiment) {
            if (!(tmp = LDNewBool(LDBooleanTrue))) {
                goto error;
            }

            if (!LDObjectSetKey(result, "inExperiment", tmp)) {
                LDJSONFree(tmp);

                goto error;
            }
        }
    } else if (details->reason == LD_FALLTHROUGH) {
        if (details->extra.fallthrough.inExperiment) {
            if (!(tmp = LDNewBool(LDBooleanTrue))) {
                goto error;
            }

            if (!LDObjectSetKey(result, "inExperiment", tmp)) {
                LDJSONFree(tmp);

                goto error;
            }
        }
    }

    return result;

error:
    LDJSONFree(result);

    return NULL;
}

static void
setDetailsOOM(struct LDDetails *const details)
{
    if (details) {
        details->reason          = LD_ERROR;
        details->extra.errorKind = LD_OOM;
    }
}

static struct LDJSON *
variation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    struct LDJSON *const       fallback,
    LDBoolean (*const checkType)(const LDJSONType type),
    struct LDDetails *const o_details)
{
    struct LDStore *     store;
    const struct LDJSON *flag;
    struct LDJSON *      value, *subEvents;
    struct LDDetails     details, *detailsRef;
    struct LDJSONRC *    flagrc;

    LD_ASSERT_API(client);
    LD_ASSERT_API(user);
    LD_ASSERT_API(key);

    LD_ASSERT(fallback);
    LD_ASSERT(checkType);

    flag      = NULL;
    flagrc    = NULL;
    value     = NULL;
    store     = NULL;
    value     = NULL;
    subEvents = NULL;

    LDDetailsInit(&details);

    if (o_details) {
        detailsRef = o_details;
        LDDetailsInit(detailsRef);
    } else {
        detailsRef = &details;
    }

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (client == NULL) {
        LD_LOG(LD_LOG_WARNING, "variation NULL client");

        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_CLIENT_NOT_SPECIFIED;

        goto error;
    } else if (user == NULL) {
        LD_LOG(LD_LOG_WARNING, "variation NULL user");

        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_USER_NOT_SPECIFIED;

        goto error;
    } else if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "variation NULL key");

        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_NULL_KEY;

        goto error;
    }
#endif

    if (!LDClientIsInitialized(client)) {
        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_CLIENT_NOT_READY;

        goto error;
    }

    store = client->store;
    LD_ASSERT(store);

    if (!LDStoreGet(store, LD_FLAG, key, &flagrc)) {
        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_STORE_ERROR;

        goto error;
    }

    if (flagrc) {
        flag = LDJSONRCGet(flagrc);
    }

    if (!flag) {
        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_FLAG_NOT_FOUND;
    } else if (!user) {
        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_USER_NOT_SPECIFIED;
    } else {
        const EvalStatus status = LDi_evaluate(
            client,
            flag,
            user,
            store,
            detailsRef,
            &subEvents,
            &value,
            o_details != NULL);

        if (status == EVAL_MEM) {
            detailsRef->reason          = LD_ERROR;
            detailsRef->extra.errorKind = LD_OOM;

            LDJSONFree(subEvents);

            goto error;
        } else if (status == EVAL_SCHEMA) {
            detailsRef->reason          = LD_ERROR;
            detailsRef->extra.errorKind = LD_MALFORMED_FLAG;

            LDJSONFree(subEvents);

            goto error;
        }
    }

    if (!LDi_processEvaluation(
            client->eventProcessor,
            user,
            subEvents,
            key,
            value,
            fallback,
            flag,
            detailsRef,
            o_details != NULL))
    {
        subEvents = NULL;

        goto error;
    }
    subEvents = NULL;

    if (!LDi_notNull(value)) {
        goto error;
    }

    if (!checkType(LDJSONGetType(value))) {
        detailsRef->reason          = LD_ERROR;
        detailsRef->extra.errorKind = LD_WRONG_TYPE;

        goto error;
    }

    LDJSONFree(fallback);
    LDDetailsClear(&details);
    LDJSONRCDecrement(flagrc);
    LDJSONFree(subEvents);

    return value;

error:
    LDJSONFree(value);
    LDDetailsClear(&details);
    LDJSONRCDecrement(flagrc);
    LDJSONFree(subEvents);

    return fallback;
}

static LDBoolean
isBool(const LDJSONType type)
{
    return type == LDBool;
}

LDBoolean
LDBoolVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const LDBoolean            fallback,
    struct LDDetails *const    details)
{
    LDBoolean      value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (!(fallbackJSON = LDNewBool(fallback))) {
        setDetailsOOM(details);

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, isBool, details);

    if (result) {
        value = LDGetBool(result);

        LDJSONFree(result);

        return value;
    } else {
        return fallback;
    }
}

static LDBoolean
isNumber(const LDJSONType type)
{
    return type == LDNumber;
}

int
LDIntVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const int                  fallback,
    struct LDDetails *const    details)
{
    int            value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (!(fallbackJSON = LDNewNumber(fallback))) {
        setDetailsOOM(details);

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, isNumber, details);

    if (result) {
        value = LDGetNumber(result);

        LDJSONFree(result);

        return value;
    } else {
        return fallback;
    }
}

double
LDDoubleVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const double               fallback,
    struct LDDetails *const    details)
{
    double         value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (!(fallbackJSON = LDNewNumber(fallback))) {
        setDetailsOOM(details);

        return fallback;
    }

    result = variation(client, user, key, fallbackJSON, isNumber, details);

    if (result) {
        value = LDGetNumber(result);

        LDJSONFree(result);

        return value;
    } else {
        return fallback;
    }
}

static LDBoolean
isText(const LDJSONType type)
{
    return type == LDText;
}

char *
LDStringVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const char *const          fallback,
    struct LDDetails *const    details)
{
    char *         value;
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;
    value        = NULL;

    if (fallback && !(fallbackJSON = LDNewText(fallback))) {
        setDetailsOOM(details);

        return NULL;
    } else if (!fallback) {
        if (!(fallbackJSON = LDNewNull())) {
            setDetailsOOM(details);

            return NULL;
        }
    }

    result = variation(client, user, key, fallbackJSON, isText, details);

    if (result == NULL) {
        return NULL;
    } else if (fallback == NULL && result == fallbackJSON) {
        LDJSONFree(fallbackJSON);

        return NULL;
    } else {
        /* never mutate just type hack */
        value = (char *)LDGetText(result);

        if (value) {
            value = LDStrDup(value);
        }

        LDJSONFree(result);

        return value;
    }
}

static LDBoolean
isArrayOrObject(const LDJSONType type)
{
    return type == LDArray || type == LDObject;
}

struct LDJSON *
LDJSONVariation(
    struct LDClient *const     client,
    const struct LDUser *const user,
    const char *const          key,
    const struct LDJSON *const fallback,
    struct LDDetails *const    details)
{
    struct LDJSON *result, *fallbackJSON;

    result       = NULL;
    fallbackJSON = NULL;

    if (fallback && !(fallbackJSON = LDJSONDuplicate(fallback))) {
        setDetailsOOM(details);

        return NULL;
    } else if (!fallback) {
        if (!(fallbackJSON = LDNewNull())) {
            setDetailsOOM(details);

            return NULL;
        }
    }

    result =
        variation(client, user, key, fallbackJSON, isArrayOrObject, details);

    if (fallback == NULL && result == fallbackJSON) {
        LDJSONFree(fallbackJSON);

        return NULL;
    }

    return result;
}

struct LDJSON *
LDAllFlags(struct LDClient *const client, const struct LDUser *const user)
{
    struct LDJSON *  evaluatedFlags, *rawFlags, *rawFlagsIter;
    struct LDJSONRC *rawFlagsRC;

    LD_ASSERT_API(client);
    LD_ASSERT_API(user);

    rawFlags       = NULL;
    rawFlagsIter   = NULL;
    rawFlagsRC     = NULL;
    evaluatedFlags = NULL;

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (client == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlags NULL client");

        return NULL;
    }

    if (user == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlags NULL user");

        return NULL;
    }
#endif

    if (client->config->offline) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlags called when offline returning NULL");

        return NULL;
    }

    if (!LDStoreInitialized(client->store)) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlags not initialized returning NULL");

        return NULL;
    }

    if (!(evaluatedFlags = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        return NULL;
    }

    if (!LDStoreAll(client->store, LD_FLAG, &rawFlagsRC)) {
        LD_LOG(LD_LOG_ERROR, "LDAllFlags failed to fetch flags");

        LDJSONFree(evaluatedFlags);

        return NULL;
    }

    /* In this case we have read from the store without error, but there are no flags in it. */
    if(!rawFlagsRC) {
        return evaluatedFlags;
    }

    rawFlags = LDJSONRCGet(rawFlagsRC);
    LD_ASSERT(rawFlags);

    for (rawFlagsIter = LDGetIter(rawFlags); rawFlagsIter;
         rawFlagsIter = LDIterNext(rawFlagsIter))
    {
        struct LDJSON *  value, *events;
        struct LDDetails details;
        struct LDJSON *  flag;
        const char *     key;

        value  = NULL;
        events = NULL;
        key    = NULL;

        flag = rawFlagsIter;
        LD_ASSERT(flag);

        LDDetailsInit(&details);

        LDi_evaluate(
            client,
            flag,
            user,
            client->store,
            &details,
            &events,
            &value,
            LDBooleanFalse);
        
        if (value) {
            key = LDGetText(LDObjectLookup(flag, "key"));
            LD_ASSERT(key);
            if (!LDObjectSetKey(evaluatedFlags, key, value)) {
                LDJSONFree(events);
                LDJSONFree(value);
                LDDetailsClear(&details);

                goto error;
            }
        }

        LDJSONFree(events);
        LDDetailsClear(&details);
    }

    LDJSONRCDecrement(rawFlagsRC);

    return evaluatedFlags;

error:
    LDJSONRCDecrement(rawFlagsRC);
    LDJSONFree(evaluatedFlags);

    return NULL;
}
