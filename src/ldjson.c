#include "cJSON.h"
#include "ldjson.h"

#include "ldinternal.h"

bool
LDNewNull(struct LDJSON **const result)
{
    cJSON *json = NULL;

    LD_ASSERT(result);

    if ((json = cJSON_CreateNull())) {
        *result = (struct LDJSON *)json;
    }

    return json != NULL;
}

bool
LDNewBool(const bool boolean, struct LDJSON **const result)
{
    cJSON *json = NULL;

    LD_ASSERT(result);

    if ((json = cJSON_CreateBool(boolean))) {
        *result = (struct LDJSON *)json;
    }

    return json != NULL;
}

bool
LDNewNumber(const double number, struct LDJSON **const result)
{
    cJSON *json = NULL;

    LD_ASSERT(result);

    if ((json = cJSON_CreateNumber(number))) {
        *result = (struct LDJSON *)json;
    }

    return json != NULL;
}

bool
LDNewText(const char *const text, struct LDJSON **const result)
{
    cJSON *json = NULL;

    LD_ASSERT(text); LD_ASSERT(result);

    if ((json = cJSON_CreateString(text))) {
        *result = (struct LDJSON *)json;
    }

    return json != NULL;
}

bool
LDNewObject(struct LDJSON **const result)
{
    cJSON *json = NULL;

    LD_ASSERT(result);

    if ((json = cJSON_CreateObject())) {
        *result = (struct LDJSON *)json;
    }

    return json != NULL;
}

bool
LDNewArray(struct LDJSON **const result)
{
    cJSON *json = NULL;

    LD_ASSERT(result);

    if ((json = cJSON_CreateArray())) {
        *result = (struct LDJSON *)json;
    }

    return json != NULL;
}

void
LDJSONFree(struct LDJSON *const json)
{
    cJSON_Delete((cJSON *)json);
}

bool
LDJSONDuplicate(const struct LDJSON *const input, struct LDJSON **const output)
{
    cJSON *result = NULL;

    LD_ASSERT(input); LD_ASSERT(output);

    if ((result = cJSON_Duplicate((cJSON *)input, true))) {
        *output = (struct LDJSON *)result;
    }

    return result != NULL;
}

bool
LDGetBool(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *const)node;

    LD_ASSERT(json); LD_ASSERT(cJSON_IsBool(json));

    return cJSON_IsTrue(json);
}

double
LDGetNumber(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *const)node;

    LD_ASSERT(json); LD_ASSERT(cJSON_IsNumber(json));

    return json->valuedouble;
}

char *
LDGetText(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *const)node;

    LD_ASSERT(json); LD_ASSERT(cJSON_IsString(json));

    return strdup(json->valuestring);
}

bool
LDArrayLookup(const struct LDJSON *const rawarray, const unsigned int index, struct LDJSON **const result)
{
    cJSON *const array = (cJSON *const)rawarray, *item = NULL;

    LD_ASSERT(array); LD_ASSERT(cJSON_IsArray(array)); LD_ASSERT(result);

    if ((item = cJSON_GetArrayItem(array, index))) {
        *result = (struct LDJSON *)item;

        return true;
    }

    return false;
}

bool
LDArrayGetIter(const struct LDJSON *const rawarray, struct LDArrayIter **const result)
{
    const cJSON *const array = (const cJSON *const)rawarray;

    LD_ASSERT(array); LD_ASSERT(cJSON_IsArray(array));

    *result = (struct LDArrayIter *)array->child;

    return true;
}

bool
LDArrayIterNext(struct LDArrayIter **const rawiter, struct LDJSON **const result)
{
    cJSON **const iter = (cJSON **const)rawiter;

    if (iter) {
        cJSON *const currentvalue = *iter;

        *iter = currentvalue->next;

        if (result) {
            *result = (struct LDJSON *)currentvalue;
        }

        return true;
    } else {
        return false;
    }
}

bool
LDArrayIterValue(const struct LDArrayIter *const iter, struct LDJSON **const result)
{
    LD_ASSERT(result);

    if (iter) {
        *result = (struct LDJSON *)iter;
    }

    return iter != NULL;
}

void
LDArrayIterFree(struct LDArrayIter *const iter)
{
    (void)iter; /* no-op */
}

bool
LDObjectLookup(const struct LDJSON *const rawobject, const char *const key, struct LDJSON **const result)
{
    cJSON *const object = (cJSON *const)rawobject, *item = NULL;

    LD_ASSERT(object); LD_ASSERT(cJSON_IsObject(object)); LD_ASSERT(key); LD_ASSERT(result);

    if ((item = cJSON_GetObjectItemCaseSensitive(object, key))) {
        *result = (struct LDJSON *)item;

        return true;
    }

    return false;
}

bool
LDObjectGetIter(const struct LDJSON *const rawobject, struct LDObjectIter **const result)
{
    const cJSON *const object = (const cJSON *const)rawobject;

    LD_ASSERT(object); LD_ASSERT(cJSON_IsObject(object));

    *result = (struct LDObjectIter *)object->child;

    return true;
}

bool
LDObjectIterNext(struct LDObjectIter **const rawiter, struct LDJSON **const result)
{
    cJSON **const iter = (cJSON **const)rawiter;

    if (iter) {
        cJSON *const currentvalue = *iter;

        *iter = currentvalue->next;

        if (result) {
            *result = (struct LDJSON *)currentvalue;
        }

        return true;
    } else {
        return false;
    }
}

bool
LDObjectIterValue(const struct LDObjectIter *const iter, struct LDJSON **const result)
{
    LD_ASSERT(result);

    if (iter) {
        *result = (struct LDJSON *)iter;
    }

    return iter != NULL;
}

void
LDObjectIterFree(struct LDObjectIter *const iter)
{
    (void)iter; /* no-op */
}

char *
LDJSONSerialize(const struct LDJSON *const json)
{
    LD_ASSERT(json);

    return cJSON_Print((cJSON *)json);
}

bool
LDJSONDeserialize(const char *const text, struct LDJSON **const json)
{
    cJSON *deserialized = NULL;

    LD_ASSERT(text); LD_ASSERT(json);

    if ((deserialized = cJSON_Parse(text))) {
        *json = (struct LDJSON *)deserialized;

        return true;
    } else {
        return false;
    }
}
