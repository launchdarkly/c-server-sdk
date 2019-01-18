#include "ldinternal.h"
#include "ldschema.h"

/* **** Utilities *** */

static const char *
typeToStringJSON(const cJSON *const json)
{
    LD_ASSERT(json);

    if (cJSON_IsInvalid(json)) {
        return "Invalid";;
    } else if(cJSON_IsBool(json)) {
        return "Boolean";
    } else if(cJSON_IsNull(json)) {
        return "Null";
    } else if(cJSON_IsNumber(json)) {
        return "Number";
    } else if(cJSON_IsString(json)) {
        return "String";
    } else if(cJSON_IsArray(json)) {
        return "Array";
    } else if(cJSON_IsObject(json)) {
        return "Object";
    } else {
        return NULL;
    }
}

static bool
checkKey(const cJSON *const json, const char *const key, const bool required,
    cJSON_bool (*const predicate)(const cJSON *const value), const cJSON **const result)
{
    LD_ASSERT(json); LD_ASSERT(key); LD_ASSERT(predicate); LD_ASSERT(result);

    if ((*result = cJSON_GetObjectItemCaseSensitive(json, key)) && !cJSON_IsNull(*result)) {
        if (predicate(*result)) {
            return true;
        } else {
            LDi_log(LD_LOG_ERROR, "Key '%s' is an invalid type %s", key, typeToStringJSON(*result));

            return false;
        }
    } else {
        *result = NULL; /* for when cJSON_IsNull not pointer NULL */

        if (required) {
            LDi_log(LD_LOG_ERROR, "Key '%s' does not exist", key);

            return false;
        } else {
            return true;
        }
    }
}

/* **** Prerequisite **** */

struct Prerequisite *
prerequisiteFromJSON(const char *const key, const cJSON *const json)
{
    struct Prerequisite *result = NULL; const cJSON *item = NULL;

    LD_ASSERT(key); LD_ASSERT(json);

    if (!(result = malloc(sizeof(struct Prerequisite)))) {
        goto error;
    }

    memset(result, 0, sizeof(struct Prerequisite));

    if (checkKey(json, "variation", true, cJSON_IsNumber, &item)) {
        result->variation = item->valueint;
    } else {
        goto error;
    }

    if (checkKey(json, "key", true, cJSON_IsString, &item)) {
        if (!(result->key = strdup(item->valuestring))) {
            LDi_log(LD_LOG_ERROR, "'strdup' for key 'key' failed");

            goto error;
        }
    } else {
        goto error;
    }

    return result;

  error:
    prerequisiteFree(result);

    return NULL;
}

void
prerequisiteFree(struct Prerequisite *const prerequisite)
{
    if (prerequisite) {
        if (prerequisite->key) {
            free(prerequisite->key);
        }

        free(prerequisite);
    }
}

void
prerequisiteFreeCollection(struct Prerequisite *prerequisites)
{
    struct Prerequisite *prerequisite = NULL, *tmp = NULL;

    HASH_ITER(hh, prerequisites, prerequisite, tmp) {
        HASH_DEL(prerequisites, prerequisite);

        prerequisiteFree(prerequisite);
    }
}

/* **** FeatureFlag **** */

struct FeatureFlag *
featureFlagFromJSON(const char *const key, const cJSON *const json)
{
    struct FeatureFlag *result = NULL; const cJSON *item = NULL;

    LD_ASSERT(key); LD_ASSERT(json);

    if (!(result = malloc(sizeof(struct FeatureFlag)))) {
        goto error;
    }

    memset(result, 0, sizeof(struct FeatureFlag));

    if (!(result->key = strdup(key))) {
        goto error;
    }

    if (checkKey(json, "version", true, cJSON_IsNumber, &item)) {
        result->version = item->valueint;
    } else {
        goto error;
    }

    if (checkKey(json, "on", true, cJSON_IsBool, &item)) {
        result->on = cJSON_IsTrue(item);
    } else {
        goto error;
    }

    if (checkKey(json, "trackEvents", true, cJSON_IsBool, &item)) {
        result->trackEvents = cJSON_IsTrue(item);
    } else {
        goto error;
    }

    if (checkKey(json, "deleted", true, cJSON_IsBool, &item)) {
        result->deleted = cJSON_IsTrue(item);
    } else {
        goto error;
    }

    if (checkKey(json, "salt", true, cJSON_IsString, &item)) {
        if (!(result->salt = strdup(item->valuestring))) {
            LDi_log(LD_LOG_ERROR, "'strdup' for key 'salt' failed");

            goto error;
        }
    } else {
        goto error;
    }

    if (checkKey(json, "sel", true, cJSON_IsString, &item)) {
        if (!(result->sel = strdup(item->valuestring))) {
            LDi_log(LD_LOG_ERROR, "'strdup' for key 'sel' failed");

            goto error;
        }
    } else {
        goto error;
    }

    if (checkKey(json, "offVariation", false, cJSON_IsNumber, &item)) {
        if (item) {
            result->hasOffVariation = true;
            result->offVariation = item->valueint;
        } else {
            result->hasOffVariation = false;
        }
    } else {
        goto error;
    }

    if (checkKey(json, "debugEventsUntilDate", false, cJSON_IsNumber, &item)) {
        if (item) {
            result->hasDebugEventsUntilDate = true;
            result->debugEventsUntilDate = item->valueint;
        } else {
            result->hasDebugEventsUntilDate = false;
        }
    } else {
        goto error;
    }

    if (checkKey(json, "clientSide", true, cJSON_IsBool, &item)) {
        result->clientSide = cJSON_IsTrue(item);
    } else {
        goto error;
    }

    return result;

  error:
    featureFlagFree(result);

    return NULL;
}

void
featureFlagFree(struct FeatureFlag *const featureFlag)
{
    if (featureFlag) {
        if (featureFlag->key) {
            free(featureFlag->key);
        }

        if (featureFlag->salt) {
            free(featureFlag->salt);
        }

        if (featureFlag->sel) {
            free(featureFlag->sel);
        }

        prerequisiteFreeCollection(featureFlag->prerequisites);

        targetFreeCollection(featureFlag->targets);

        free(featureFlag);
    }
}

void
featureFlagFreeCollection(struct FeatureFlag *featureFlags)
{
    struct FeatureFlag *featureFlag = NULL, *tmp = NULL;

    HASH_ITER(hh, featureFlags, featureFlag, tmp) {
        HASH_DEL(featureFlags, featureFlag);

        featureFlagFree(featureFlag);
    }
}

/* **** Target **** */

struct Target *
targetFromJSON(const cJSON *const json)
{
    struct Target *result = NULL; const cJSON *item = NULL, *iter = NULL;

    LD_ASSERT(json);

    if (!(result = malloc(sizeof(struct Target)))) {
        goto error;
    }

    memset(result, 0, sizeof(struct Target));

    if (checkKey(json, "variation", true, cJSON_IsNumber, &item)) {
        result->variation = item->valueint;
    } else {
        goto error;
    }

    if (checkKey(json, "values", true, cJSON_IsArray, &item)) {
        cJSON_ArrayForEach(iter, json) {
            if (cJSON_IsString(iter)) {
                if (!LDHashSetAddKey(&result->values, iter->valuestring)) {
                    goto error;
                }
            } else {
                goto error;
            }
        }
    } else {
        goto error;
    }

    return result;

  error:
    targetFree(result);

    return NULL;
}

void
targetFree(struct Target *const target)
{
    if (target) {
        LDHashSetFree(target->values);

        free(target);
    }
}

void
targetFreeCollection(struct Target *targets)
{
    struct Target *target = NULL, *tmp = NULL;

    HASH_ITER(hh, targets, target, tmp) {
        HASH_DEL(targets, target);

        targetFree(target);
    }
}
