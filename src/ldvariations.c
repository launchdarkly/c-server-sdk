#include "ldinternal.h"
#include "ldevaluate.h"
#include "ldvariations.h"
#include "ldevents.h"

void
LDDetailsInit(struct LDDetails *const details)
{
    details->variationIndex = 0;
    details->hasVariation   = false;
    details->kind           = LD_UNKNOWN;
}

void
LDDetailsClear(struct LDDetails *const details)
{
    LDDetailsInit(details);
}

static struct LDJSON *
variation(struct LDClient *const client, const struct LDUser *const user,
    const char *const key, struct LDJSON *const fallback,
    const LDJSONType type, struct LDJSON **const reason)
{
    EvalStatus status;
    unsigned int *variationNumRef;
    struct LDStore *store;
    struct LDJSON *flag, *value, *events, *event, *evalue;
    struct LDDetails details;

    LD_ASSERT(client);

    flag             = NULL;
    value            = NULL;
    store            = NULL;
    events           = NULL;
    event            = NULL;
    variationNumRef  = NULL;
    evalue           = NULL;

    LDDetailsInit(&details);

    if (!client) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_NULL_CLIENT;

        goto fallback;
    }

    if (!LDClientIsInitialized(client)) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_CLIENT_NOT_READY;

        goto fallback;
    }

    if (!key) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_NULL_KEY;

        goto fallback;
    }

    LD_ASSERT(store = client->config->store);

    if (!LDStoreGet(store, "flags", key, &flag)) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_STORE_ERROR;

        status = EVAL_MISS;
    }

    if (!flag) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_FLAG_NOT_FOUND;

        status = EVAL_MISS;
    } else if (!user || !user->key) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_USER_NOT_SPECIFIED;

        status = EVAL_MISS;
    } else {
        status = LDi_evaluate(client, flag, user, store, &details, &events,
            &value);
    }

    if (status == EVAL_MEM) {
        goto error;
    }

    if (status == EVAL_SCHEMA) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_MALFORMED_FLAG;

        goto fallback;
    }

    if (events) {
        struct LDJSON *iter;
        /* local only sanity */
        LD_ASSERT(LDJSONGetType(events) == LDArray);

        for (iter = LDGetIter(events); iter; iter = LDIterNext(iter)) {
            if (!LDi_addEvent(client, iter)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto error;
            }

            if (!LDi_summarizeEvent(client, iter, false)) {
                LD_LOG(LD_LOG_ERROR, "summary failed");

                goto error;
            }
        }
    }

    if (details.hasVariation) {
        variationNumRef = &details.variationIndex;
    }

    if (!LDi_notNull(evalue = value)) {
        evalue = fallback;
    }

    event = LDi_newFeatureRequestEvent(client, key, user, variationNumRef,
        evalue, fallback, NULL, flag, &reason);

    if (!event) {
        LD_LOG(LD_LOG_ERROR, "failed to build feature request event");

        goto error;
    }

    if (flag) {
        struct LDJSON *const track = LDObjectLookup(flag, "trackEvents");

        if (LDi_notNull(track) && LDGetBool(track)) {
            if (!LDi_addEvent(client, event)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto error;
            }
        }
    }

    if (!LDi_summarizeEvent(client, event, !flag)) {
        LD_LOG(LD_LOG_ERROR, "summary failed");

        goto error;
    }

    if (!LDi_notNull(value)) {
        goto fallback;
    }

    if (LDJSONGetType(value) != type) {
        details.kind = LD_ERROR;
        details.extra.errorKind = LD_WRONG_TYPE;

        goto fallback;
    }

    /*
    if (reason) {
        *reason = LDJSONDuplicate(details);
    }
    */

    LDJSONFree(event);
    LDJSONFree(fallback);
    LDJSONFree(flag);
    LDDetailsClear(&details);

    return value;

  fallback:
    /*
    if (reason) {
        *reason = LDJSONDuplicate(details);
    }
    */

    LDJSONFree(flag);
    LDJSONFree(event);
    LDJSONFree(value);
    LDDetailsClear(&details);

    return fallback;

  error:
    LDJSONFree(event);
    LDJSONFree(flag);
    LDJSONFree(value);
    LDDetailsClear(&details);

    return fallback;
}

bool
LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback,
    struct LDJSON **const details)
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
    struct LDJSON **const details)
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
    struct LDJSON **const details)
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
    struct LDJSON **const details)
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
    struct LDJSON **const details)
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
    struct LDJSON *flag, *rawFlags, *evaluatedFlags;

    LD_ASSERT(client);

    flag           = NULL;
    rawFlags       = NULL;
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

    if (!LDStoreAll(client->config->store, "flags", &rawFlags)) {
        LD_LOG(LD_LOG_ERROR, "LDAllFlags failed to fetch flags");

        return NULL;
    }

    if (!(evaluatedFlags = LDNewObject())) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        LDJSONFree(rawFlags);

        return NULL;
    }

    /* locally ensured sanity check */
    LD_ASSERT(LDJSONGetType(rawFlags) == LDObject);

    for (flag = LDGetIter(rawFlags); flag; flag = LDIterNext(flag)) {
        struct LDJSON *value, *events;
        EvalStatus status;
        struct LDDetails details;

        value   = NULL;
        events  = NULL;

        LDDetailsInit(&details);

        status = LDi_evaluate(client, flag, user, client->config->store,
            &details, &events, &value);

        if (status == EVAL_MEM || status == EVAL_SCHEMA) {
            LDJSONFree(events);
            LDDetailsClear(&details);

            goto error;
        }

        if (value) {
            if (!LDObjectSetKey(evaluatedFlags, LDIterKey(flag), value)) {
                LDJSONFree(events);
                LDJSONFree(value);
                LDDetailsClear(&details);

                goto error;
            }
        }

        LDJSONFree(events);
        LDDetailsClear(&details);
    }

    LDJSONFree(rawFlags);

    return evaluatedFlags;

  error:
    LDJSONFree(rawFlags);
    LDJSONFree(evaluatedFlags);

    return NULL;
}
