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

LDJSONType
LDJSONGetType(const struct LDJSON *const inputraw)
{
    const struct cJSON *const input = (const struct cJSON *const) inputraw;

    LD_ASSERT(input);

    if (cJSON_IsBool(input)) {
        return LDBool;
    } else if (cJSON_IsNumber(input)) {
        return LDNumber;
    } else if (cJSON_IsNull(input)) {
        return LDNull;
    } else if (cJSON_IsObject(input)) {
        return LDObject;
    } else if (cJSON_IsArray(input)) {
        return LDArray;
    } else if (cJSON_IsString(input)) {
        return LDText;
    }

    LD_LOG(LD_LOG_CRITICAL, "LDJSONGetType Unknown");

    abort();
}

bool
LDJSONCompare(const struct LDJSON *const left, const struct LDJSON *const right)
{
    return cJSON_Compare((const cJSON *const)left,
        (const cJSON *const)right, true);
}

bool
LDGetBool(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *const)node;

    LD_ASSERT(json);
    LD_ASSERT(cJSON_IsBool(json));

    return cJSON_IsTrue(json);
}

double
LDGetNumber(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *const)node;

    LD_ASSERT(json);
    LD_ASSERT(cJSON_IsNumber(json));

    return json->valuedouble;
}

const char *
LDGetText(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *const)node;

    LD_ASSERT(json); LD_ASSERT(cJSON_IsString(json));

    return json->valuestring;
}

struct LDJSON *
LDGetIter(const struct LDJSON *const rawcollection)
{
    const cJSON *const collection = (const cJSON *const)rawcollection;

    LD_ASSERT(collection);
    LD_ASSERT(cJSON_IsArray(collection) || cJSON_IsObject(collection));

    return (struct LDJSON *)collection->child;
}

struct LDJSON *
LDIterNext(const struct LDJSON *const rawiter)
{
    cJSON *const iter = (cJSON *const)rawiter;

    LD_ASSERT(iter);

    return (struct LDJSON *)iter->next;
}

const char *
LDIterKey(const struct LDJSON *const rawiter)
{
    cJSON *const iter = (cJSON *const)rawiter;

    LD_ASSERT(iter);

    return iter->string;
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

    LD_ASSERT(array);
    LD_ASSERT(cJSON_IsArray(array));

    return (struct LDJSON *)cJSON_GetArrayItem(array, index);
}

bool
LDArrayPush(struct LDJSON *const rawarray, struct LDJSON *const item)
{
    cJSON *const array = (cJSON *const)rawarray;

    LD_ASSERT(array);
    LD_ASSERT(cJSON_IsArray(array));
    LD_ASSERT(item);

    cJSON_AddItemToArray(array, (cJSON *)item);

    return true;
}

bool
LDArrayAppend(struct LDJSON *const rawprefix,
    const struct LDJSON *const rawsuffix)
{
    cJSON *iter;
    cJSON *const prefix = (cJSON *const)rawprefix;
    const cJSON *const suffix = (const cJSON *const)rawsuffix;

    LD_ASSERT(prefix);
    LD_ASSERT(cJSON_IsArray(prefix));
    LD_ASSERT(suffix);
    LD_ASSERT(cJSON_IsArray(suffix));

    for (iter = suffix->child; iter; iter = iter->next) {
        cJSON *dupe;

        if (!(dupe = cJSON_Duplicate(iter, true))) {
            return false;
        }

        cJSON_AddItemToArray(prefix, dupe);
    }

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
LDObjectSetKey(struct LDJSON *const rawobject,
    const char *const key, struct LDJSON *const item)
{
    cJSON *const object = (cJSON *const)rawobject;

    LD_ASSERT(object);
    LD_ASSERT(cJSON_IsObject(object));
    LD_ASSERT(key);
    LD_ASSERT(item);

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);

    cJSON_AddItemToObject(object, key, (cJSON *)item);

    return true;
}

void
LDObjectDeleteKey(struct LDJSON *const rawobject, const char *const key)
{
    cJSON *const object = (cJSON *const)rawobject;

    LD_ASSERT(object);
    LD_ASSERT(cJSON_IsObject(object));
    LD_ASSERT(key);

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);
}

bool
LDObjectMerge(struct LDJSON *const to, const struct LDJSON *const from)
{
    const struct LDJSON *iter;

    LD_ASSERT(to);
    LD_ASSERT(LDJSONGetType(to) == LDObject);
    LD_ASSERT(from);
    LD_ASSERT(LDJSONGetType(from) == LDObject);

    for (iter = LDGetIter(from); iter; iter = LDIterNext(iter)) {
        struct LDJSON *duplicate;

        if (!(duplicate = LDJSONDuplicate(iter))) {
            return false;
        }

        LDObjectSetKey(to, LDIterKey(iter), duplicate);
    }

    return true;
}

char *
LDJSONSerialize(const struct LDJSON *const json)
{
    LD_ASSERT(json);

    return cJSON_PrintUnformatted((cJSON *)json);
}

struct LDJSON *
LDJSONDeserialize(const char *const text)
{
    LD_ASSERT(text);

    return (struct LDJSON *)cJSON_Parse(text);
}
