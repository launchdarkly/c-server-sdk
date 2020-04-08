#include "cJSON.h"

#include <launchdarkly/api.h>

#include "assertion.h"
#include "utility.h"

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
    LD_ASSERT_API(text);

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

bool
LDSetNumber(struct LDJSON *const rawnode, const double number)
{
    struct cJSON *const node = (struct cJSON *)rawnode;

    LD_ASSERT_API(node);
    LD_ASSERT_API(cJSON_IsNumber(node));

    node->valuedouble = number;

    return true;
}

void
LDJSONFree(struct LDJSON *const json)
{
    cJSON_Delete((cJSON *)json);
}

struct LDJSON *
LDJSONDuplicate(const struct LDJSON *const input)
{
    LD_ASSERT_API(input);

    return (struct LDJSON *)cJSON_Duplicate((cJSON *)input, true);
}

LDJSONType
LDJSONGetType(const struct LDJSON *const inputraw)
{
    const struct cJSON *const input = (const struct cJSON *)inputraw;

    LD_ASSERT_API(input);

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
    return cJSON_Compare((const cJSON *)left,
        (const cJSON *)right, true);
}

bool
LDGetBool(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *)node;

    LD_ASSERT_API(json);
    LD_ASSERT_API(cJSON_IsBool(json));

    return cJSON_IsTrue(json);
}

double
LDGetNumber(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *)node;

    LD_ASSERT_API(json);
    LD_ASSERT_API(cJSON_IsNumber(json));

    return json->valuedouble;
}

const char *
LDGetText(const struct LDJSON *const node)
{
    cJSON *const json = (cJSON *)node;

    LD_ASSERT_API(json);
    LD_ASSERT_API(cJSON_IsString(json));

    return json->valuestring;
}

struct LDJSON *
LDGetIter(const struct LDJSON *const rawcollection)
{
    const cJSON *const collection = (const cJSON *)rawcollection;

    LD_ASSERT_API(collection);
    LD_ASSERT_API(cJSON_IsArray(collection) || cJSON_IsObject(collection));

    return (struct LDJSON *)collection->child;
}

struct LDJSON *
LDIterNext(const struct LDJSON *const rawiter)
{
    cJSON *const iter = (cJSON *)rawiter;

    LD_ASSERT_API(iter);

    return (struct LDJSON *)iter->next;
}

const char *
LDIterKey(const struct LDJSON *const rawiter)
{
    cJSON *const iter = (cJSON *)rawiter;

    LD_ASSERT_API(iter);

    return iter->string;
}

unsigned int
LDCollectionGetSize(const struct LDJSON *const rawcollection)
{
    cJSON *const collection = (cJSON *)rawcollection;

    LD_ASSERT_API(collection);
    LD_ASSERT_API(cJSON_IsArray(collection) || cJSON_IsObject(collection));

    /* works for objects */
    return cJSON_GetArraySize(collection);
}

struct LDJSON *
LDCollectionDetachIter(struct LDJSON *const rawcollection,
    struct LDJSON *const rawiter)
{
    cJSON *const collection = (cJSON *)rawcollection;
    cJSON *const iter = (cJSON *)rawiter;

    LD_ASSERT_API(collection);
    LD_ASSERT_API(cJSON_IsArray(collection) || cJSON_IsObject(collection));
    LD_ASSERT_API(iter);

    return (struct LDJSON *)cJSON_DetachItemViaPointer(collection, iter);
}

struct LDJSON *
LDArrayLookup(const struct LDJSON *const rawarray, const unsigned int index)
{
    cJSON *const array = (cJSON *)rawarray;

    LD_ASSERT_API(array);
    LD_ASSERT_API(cJSON_IsArray(array));

    return (struct LDJSON *)cJSON_GetArrayItem(array, index);
}

bool
LDArrayPush(struct LDJSON *const rawarray, struct LDJSON *const item)
{
    cJSON *const array = (cJSON *)rawarray;

    LD_ASSERT_API(array);
    LD_ASSERT_API(cJSON_IsArray(array));
    LD_ASSERT_API(item);

    cJSON_AddItemToArray(array, (cJSON *)item);

    return true;
}

bool
LDArrayAppend(struct LDJSON *const rawprefix,
    const struct LDJSON *const rawsuffix)
{
    cJSON *iter;
    cJSON *const prefix = (cJSON *)rawprefix;
    const cJSON *const suffix = (const cJSON *)rawsuffix;

    LD_ASSERT_API(prefix);
    LD_ASSERT_API(cJSON_IsArray(prefix));
    LD_ASSERT_API(suffix);
    LD_ASSERT_API(cJSON_IsArray(suffix));

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
    cJSON *const object = (cJSON *)rawobject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);

    return (struct LDJSON *)cJSON_GetObjectItemCaseSensitive(object, key);
}

bool
LDObjectSetKey(struct LDJSON *const rawobject,
    const char *const key, struct LDJSON *const item)
{
    cJSON *const object = (cJSON *)rawobject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);
    LD_ASSERT_API(item);

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);

    cJSON_AddItemToObject(object, key, (cJSON *)item);

    return true;
}

void
LDObjectDeleteKey(struct LDJSON *const rawobject, const char *const key)
{
    cJSON *const object = (cJSON *)rawobject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);

    cJSON_DeleteItemFromObjectCaseSensitive(object, key);
}

struct LDJSON *
LDObjectDetachKey(struct LDJSON *const rawobject, const char *const key)
{
    cJSON *const object = (cJSON *)rawobject;

    LD_ASSERT_API(object);
    LD_ASSERT_API(cJSON_IsObject(object));
    LD_ASSERT_API(key);

    return (struct LDJSON *)
        cJSON_DetachItemFromObjectCaseSensitive(object, key);
}

bool
LDObjectMerge(struct LDJSON *const to, const struct LDJSON *const from)
{
    const struct LDJSON *iter;

    LD_ASSERT_API(to);
    LD_ASSERT_API(LDJSONGetType(to) == LDObject);
    LD_ASSERT_API(from);
    LD_ASSERT_API(LDJSONGetType(from) == LDObject);

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
    LD_ASSERT_API(json);

    return cJSON_PrintUnformatted((cJSON *)json);
}

struct LDJSON *
LDJSONDeserialize(const char *const text)
{
    LD_ASSERT_API(text);

    return (struct LDJSON *)cJSON_Parse(text);
}
