#include "assertion.h"
#include "json_internal_helpers.h"
#include "cJSON.h"

LDBoolean
LDObjectSetString(struct LDJSON *const rawObject, const char *const key, const char *const value) {
    cJSON *const object = (cJSON *)rawObject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);
    LD_ASSERT_API(value);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (object == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetString NULL object");

        return LDBooleanFalse;
    }

    if (cJSON_IsObject(object) == LDBooleanFalse) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetString not object");

        return LDBooleanFalse;
    }

    if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetString NULL key");

        return LDBooleanFalse;
    }

    if (value == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetString NULL value");

        return LDBooleanFalse;
    }
#endif

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);

    if (!cJSON_AddStringToObject(object, key, value)) {
        LD_LOG(LD_LOG_ERROR, "LDObjectSetString failed to set value");

        return LDBooleanFalse;
    }

    return LDBooleanTrue;
}

LDBoolean
LDObjectSetBool(struct LDJSON *rawObject, const char *const key, LDBoolean value) {
    cJSON *const object = (cJSON *)rawObject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (object == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetBool NULL object");

        return LDBooleanFalse;
    }

    if (cJSON_IsObject(object) == LDBooleanFalse) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetBool not object");

        return LDBooleanFalse;
    }

    if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetBool NULL key");

        return LDBooleanFalse;
    }

#endif

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);

    if (!cJSON_AddBoolToObject(object, key, value)) {
        LD_LOG(LD_LOG_ERROR, "LDObjectSetBool failed to set value");

        return LDBooleanFalse;
    }

    return LDBooleanTrue;
}

struct LDJSON *
LDObjectNewChild(struct LDJSON *rawObject, const char *const key) {
    cJSON *const object = (cJSON *)rawObject;
    cJSON *child = NULL;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (object == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectNewChild NULL object");

        return NULL;
    }

    if (cJSON_IsObject(object) == LDBooleanFalse) {
        LD_LOG(LD_LOG_WARNING, "LDObjectNewChild not object");

        return NULL;
    }

    if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectNewChild NULL key");

        return NULL;
    }

#endif

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);

    if (!(child = cJSON_AddObjectToObject(object, key))) {
        LD_LOG(LD_LOG_ERROR, "LDObjectNewChild failed to create child object");

        return NULL;
    }

    return (struct LDJSON*) child;
}

LDBoolean
LDObjectSetNumber(struct LDJSON *rawObject, const char *const key, const double number) {
    cJSON *const object = (cJSON *)rawObject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (object == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetNumber NULL object");

        return LDBooleanFalse;
    }

    if (cJSON_IsObject(object) == LDBooleanFalse) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetNumber not object");

        return LDBooleanFalse;
    }

    if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetNumber NULL key");

        return LDBooleanFalse;
    }

#endif

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);

    if (!cJSON_AddNumberToObject(object, key, number)) {
        LD_LOG(LD_LOG_ERROR, "LDObjectSetNumber failed to set value");

        return LDBooleanFalse;
    }

    return LDBooleanTrue;
}


LDBoolean
LDObjectSetReference(struct LDJSON *const rawObject, const char *const key, struct LDJSON *const item)
{
    cJSON *const object = (cJSON *)rawObject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);
    LD_ASSERT_API(item);

#ifdef LAUNCHDARKLY_DEFENSIVE
    if (object == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetReference NULL object");

        return LDBooleanFalse;
    }

    if (cJSON_IsObject(object) == LDBooleanFalse) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetReference not object");

        return LDBooleanFalse;
    }

    if (key == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetReference NULL key");

        return LDBooleanFalse;
    }

    if (item == NULL) {
        LD_LOG(LD_LOG_WARNING, "LDObjectSetReference NULL item");

        return LDBooleanFalse;
    }
#endif

    cJSON_AddItemReferenceToObject(object, key, (cJSON *)item);

    return LDBooleanTrue;
}
