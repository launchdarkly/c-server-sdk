#include "ldinternal.h"
#include "ldevaluate.h"
#include "ldvariations.h"
#include "ldevents.h"

static struct LDJSON *
variation(struct LDClient *const client, const struct LDUser *const user,
    const char *const key, struct LDJSON *const fallback,
    const LDJSONType type, struct LDJSON **const reason)
{
    EvalStatus status;
    struct LDJSON *flag = NULL;
    struct LDJSON *value;
    struct LDStore *store;
    struct LDJSON *details = NULL;
    struct LDJSON *events;
    struct LDJSON *event = NULL;
    unsigned int variationNum;

    if (!client) {
        if (!addErrorReason(&details, "NULL_CLIENT")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    if (!LDClientIsInitialized(client)) {
        if (!addErrorReason(&details, "CLIENT_NOT_READY")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    if (!key) {
        if (!addErrorReason(&details, "NULL_KEY")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            return NULL;
        }

        goto fallback;
    }

    if (!user) {
        if (!addErrorReason(&details, "USER_NOT_SPECIFIED")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    LD_ASSERT(store = client->config->store);

    flag = LDStoreGet(store, "flags", key);

    if (!flag) {
        if (!addErrorReason(&details, "FLAG_NOT_FOUND")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    status = evaluate(flag, user, store, &details);

    if (status == EVAL_MEM) {
        goto error;
    }

    if (status == EVAL_SCHEMA) {
        if (!addErrorReason(&details, "MALFORMED_FLAG")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    if ((events = LDObjectLookup(details, "events"))) {
        struct LDJSON *iter;
        /* local only sanity */
        LD_ASSERT(LDJSONGetType(events) == LDArray);

        for (iter = LDGetIter(events); iter; iter = LDIterNext(iter)) {
            if (!addEvent(client, iter)) {
                LD_LOG(LD_LOG_ERROR, "alloc error");

                goto error;
            }

            if (!summarizeEvent(client, iter)) {
                LD_LOG(LD_LOG_ERROR, "summary failed");

                goto error;
            }
        }
    }

    variationNum = LDGetNumber(LDObjectLookup(details, "variationIndex"));

    event = newFeatureRequestEvent(key, user, &variationNum,
        LDObjectLookup(details, "value"), NULL,
        LDGetText(LDObjectLookup(flag, "key")), flag,
        LDObjectLookup(details, "reason"));

    if (!event) {
        LD_LOG(LD_LOG_ERROR, "failed to build feature request event");

        goto error;
    }

    if (!addEvent(client, event)) {
        LD_LOG(LD_LOG_ERROR, "alloc error");

        goto error;
    }

    if (!summarizeEvent(client, event)) {
        LD_LOG(LD_LOG_ERROR, "summary failed");

        goto error;
    }

    value = LDObjectLookup(details, "value");

    if (!notNull(value)) {
        goto fallback;
    }

    if (LDJSONGetType(value) != type) {
        if (!addErrorReason(&details, "WRONG_TYPE")) {
            LD_LOG(LD_LOG_ERROR, "failed to add error reason");

            goto error;
        }

        goto fallback;
    }

    value = LDJSONDuplicate(value);

    if (reason) {
        *reason = LDJSONDuplicate(details);
    }

    LDJSONFree(event);
    LDJSONFree(details);
    LDJSONFree(fallback);
    LDJSONFree(flag);

    return value;

  fallback:
    if (reason) {
        *reason = LDJSONDuplicate(details);
    }

    LDJSONFree(flag);
    LDJSONFree(event);
    LDJSONFree(details);

    return fallback;

  error:
    LDJSONFree(event);
    LDJSONFree(details);
    LDJSONFree(flag);

    return fallback;
}

bool
LDBoolVariation(struct LDClient *const client, struct LDUser *const user,
    const char *const key, const bool fallback,
    struct LDJSON **const details)
{
    bool value;
    struct LDJSON *result;
    struct LDJSON *fallbackJSON;

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
    struct LDJSON *result;
    struct LDJSON *fallbackJSON;

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
    struct LDJSON *result;
    struct LDJSON *fallbackJSON;

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
    struct LDJSON *result;
    struct LDJSON *fallbackJSON = NULL;

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
    struct LDJSON *result;
    struct LDJSON *fallbackJSON = NULL;

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
    struct LDJSON *flag;
    struct LDJSON *rawFlags;
    struct LDJSON *evaluatedFlags;

    LD_ASSERT(client);

    if (client->config->offline) {
        LD_LOG(LD_LOG_WARNING, "LDAllFlags called when offline returning NULL");

        return NULL;
    }

    if (!user) {
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

    if (!(rawFlags = LDStoreAll(client->config->store, "flags"))) {
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
        struct LDJSON *value;
        struct LDJSON *details = NULL;

        EvalStatus status = evaluate(flag, user,
            client->config->store, &details);

        if (status == EVAL_MEM || status == EVAL_SCHEMA) {
            goto error;
        }

        LD_ASSERT(value = LDObjectLookup(details, "value"));

        if (!(value = LDJSONDuplicate(value))) {
            LDJSONFree(details);

            goto error;
        }

        LDJSONFree(details);

        if (!LDObjectSetKey(evaluatedFlags, LDIterKey(flag), value)) {
            goto error;
        }
    }

    LDJSONFree(rawFlags);

    return evaluatedFlags;

  error:
    LDJSONFree(rawFlags);
    LDJSONFree(evaluatedFlags);

    return NULL;
}
