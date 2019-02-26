#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ldapi.h"
#include "ldinternal.h"
#include "ldjson.h"

struct LDJSON *
newBaseEvent(const struct LDUser *const user)
{
    struct LDJSON *tmp;
    struct LDJSON *event;

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

    /* TODO: use actual time */
    if (!(tmp = LDNewNumber(0))) {
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
    const struct LDJSON *const flag)
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
        if (!(tmp = LDObjectLookup(flag, "version"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            goto error;
        }

        if (LDJSONGetType(tmp) != LDText) {
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

        if (!(tmp = LDObjectLookup(flag, "trackEvents"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            goto error;
        }

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

        if (!(tmp = LDObjectLookup(flag, "debugEventsUntilDate"))) {
            LD_LOG(LD_LOG_ERROR, "schema error");

            goto error;
        }

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
