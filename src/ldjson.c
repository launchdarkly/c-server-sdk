#include "cJSON.h"
#include "ldjson.h"

#include "ldinternal.h"

struct LDJSON *
LDNewNull()
{
    return (struct LDJSON *)cJSON_CreateNull();
}

struct LDJSON *
LDNewBool(const bool boolean)
{
    return (struct LDJSON *)cJSON_CreateBool(boolean);
}

struct LDJSON *
LDNewNumber(const double number)
{
    return (struct LDJSON *)cJSON_CreateNumber(number);
}

struct LDJSON *
LDNewText(const char *const text)
{
    LD_ASSERT(text);

    return (struct LDJSON *)cJSON_CreateString(text);
}

struct LDJSON *
LDNewObject()
{
    return (struct LDJSON *)cJSON_CreateObject();
}

struct LDJSON *
LDNewArray()
{
    return (struct LDJSON *)cJSON_CreateArray();
}

void
LDJSONFree(struct LDJSON *const json)
{
    cJSON_Delete((cJSON *)json);
}

struct LDJSON *
LDJSONDuplicate(const struct LDJSON *const input)
{
    LD_ASSERT(input);

    return (struct LDJSON *)cJSON_Duplicate((cJSON *)input, true);
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

unsigned int
LDArrayGetSize(const struct LDJSON *const rawarray)
{
    cJSON *const array = (cJSON *const)rawarray;

    LD_ASSERT(array);

    return cJSON_GetArraySize(array);
}

struct LDJSON *
LDArrayLookup(const struct LDJSON *const rawarray, const unsigned int index)
{
    cJSON *const array = (cJSON *const)rawarray;

    LD_ASSERT(array); LD_ASSERT(cJSON_IsArray(array));

    return (struct LDJSON *)cJSON_GetArrayItem(array, index);
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
LDArrayAppend(struct LDJSON *const rawarray, struct LDJSON *const item)
{
    cJSON *const array = (cJSON *const)rawarray;

    LD_ASSERT(array); LD_ASSERT(cJSON_IsArray(array)); LD_ASSERT(item);

    cJSON_AddItemToArray(array, (cJSON *)item);

    return true;
}

struct LDJSON *
LDObjectLookup(const struct LDJSON *const rawobject, const char *const key)
{
    cJSON *const object = (cJSON *const)rawobject;

    LD_ASSERT(object); LD_ASSERT(cJSON_IsObject(object)); LD_ASSERT(key);

    return (struct LDJSON *)cJSON_GetObjectItemCaseSensitive(object, key);
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
LDObjectIterNext(struct LDObjectIter **const rawiter, const char **const key, struct LDJSON **const result)
{
    cJSON **const iter = (cJSON **const)rawiter;

    if (iter) {
        cJSON *const currentvalue = *iter;

        *iter = currentvalue->next;

        if (key) {
            *key = currentvalue->string;
        }

        if (result) {
            *result = (struct LDJSON *)currentvalue;
        }

        return true;
    } else {
        return false;
    }
}

bool
LDObjectIterValue(const struct LDObjectIter *const iter, const char **const key, struct LDJSON **const result)
{
    if (iter) {
        if (key) {
            *key = ((cJSON *)iter)->string;
        }

        if (result) {
            *result = (struct LDJSON *)iter;
        }
    }

    return iter != NULL;
}

void
LDObjectIterFree(struct LDObjectIter *const iter)
{
    (void)iter; /* no-op */
}

bool
LDObjectSetKey(struct LDJSON *const rawobject, const char *const key, struct LDJSON *const item)
{
    cJSON *const object = (cJSON *const)rawobject;

    LD_ASSERT(object); LD_ASSERT(cJSON_IsObject(object)); LD_ASSERT(key); LD_ASSERT(item);

    cJSON_AddItemToObject(object, key, (cJSON *)item);

    return true;
}

char *
LDJSONSerialize(const struct LDJSON *const json)
{
    LD_ASSERT(json);

    return cJSON_Print((cJSON *)json);
}

struct LDJSON *
LDJSONDeserialize(const char *const text)
{
    LD_ASSERT(text);

    return (struct LDJSON *)cJSON_Parse(text);
}
