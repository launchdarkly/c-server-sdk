#include <string.h>
#include <stdlib.h>

#include "cJSON.h"

#include "ldinternal.h"

static struct LDNode *
newnode(const LDNodeType type)
{
    struct LDNode *const node = malloc(sizeof(struct LDNode));

    if (node) {
        memset(node, 0, sizeof(struct LDNode));

        node->type = type;

        return node;
    } else {
        return NULL;
    }
}

struct LDNode *
LDNodeNewNull()
{
    return newnode(LDNodeNull);
}

struct LDNode *
LDNodeNewBool(const bool boolean)
{
    struct LDNode *const node = newnode(LDNodeBool);

    if (node) {
        node->value.boolean = boolean;

        return node;
    } else {
        return NULL;
    }
}

bool
LDNodeGetBool(const struct LDNode *const node)
{
    LD_ASSERT(node); LD_ASSERT(node->type == LDNodeBool);

    return node->value.boolean;
}

struct LDNode *
LDNodeNewNumber(const double number)
{
    struct LDNode *const node = newnode(LDNodeNumber);

    if (node) {
        node->value.number = number;

        return node;
    } else {
        return NULL;
    }
}

double
LDNodeGetNumber(const struct LDNode *const node)
{
    LD_ASSERT(node); LD_ASSERT(node->type == LDNodeNumber);

    return node->value.number;
}

struct LDNode *
LDNodeNewText(const char *const text)
{
    struct LDNode *const node = newnode(LDNodeBool);

    if (node) {
        node->value.text = strdup(text);

        if (text && !node->value.text) {
            free(node);

            return NULL;
        } else {
            return node;
        }
    } else {
        return NULL;
    }
}

const char *
LDNodeGetText(const struct LDNode *const node)
{
    LD_ASSERT(node); LD_ASSERT(node->type == LDNodeText);

    return node->value.text;
}

struct LDNode *
LDNodeNewObject()
{
    struct LDNode *const node = newnode(LDNodeObject);

    if (node) {
        node->value.object = NULL;

        return node;
    } else {
        return NULL;
    }
}

struct LDNode *
LDNodeNewArray()
{
    struct LDNode *const node = newnode(LDNodeArray);

    if (node) {
        node->value.array = NULL;

        return node;
    } else {
        return NULL;
    }
}

bool
LDNodeObjectSetItem(struct LDNode *const object, const char *const key, struct LDNode *const item)
{
    LD_ASSERT(object); LD_ASSERT(key); LD_ASSERT(item); LD_ASSERT(object->type == LDNodeObject);

    item->location.key = strdup(key);

    if (!item->location.key) {
        return false;
    }

    HASH_ADD_KEYPTR(hh, object->value.object, item->location.key, strlen(item->location.key), item);

    return true;
}

struct LDNode *
LDNodeObjectGetIterator(const struct LDNode *const object)
{
    LD_ASSERT(object); LD_ASSERT(object->type == LDNodeObject);

    return object->value.object;
}

const char *
LDNodeObjectIterGetKey(struct LDNode *const iter)
{
    LD_ASSERT(iter);

    return iter->location.key;
}

bool
LDNodeArrayAppendItem(struct LDNode *const array, struct LDNode *const item)
{
    LD_ASSERT(array); LD_ASSERT(item); LD_ASSERT(array->type == LDNodeArray);

    item->location.index = HASH_COUNT(array->value.array);

    HASH_ADD_INT(array->value.array, location.index, item);

    return true;
}

unsigned int
LDNodeArrayIterGetIndex(struct LDNode *const iter)
{
    LD_ASSERT(iter);

    return iter->location.index;
}

struct LDNode *
LDNodeArrayGetIterator(const struct LDNode *const array)
{
    LD_ASSERT(array); LD_ASSERT(array->type == LDNodeArray);

    return array->value.array;
}

struct LDNode *
LDNodeAdvanceIterator(const struct LDNode *const iter)
{
    LD_ASSERT(iter);

    return iter->hh.next;
}

static void freeiter(struct LDNode *iter, const bool freekey);

static void
freenode(struct LDNode *const node)
{
    switch (node->type) {
        case LDNodeText:
            free(node->value.text);
            break;
        case LDNodeObject:
            freeiter(node->value.object, true);
            break;
        case LDNodeArray:
            freeiter(node->value.array, false);
            break;
        default: break;
    }

    free(node);
}

static void
freeiter(struct LDNode *iter, const bool freekey)
{
    struct LDNode *node = NULL, *tmp = NULL;

    HASH_ITER(hh, iter, node, tmp) {
        HASH_DEL(iter, node);

        if (freekey) {
            free(node->location.key);
        }

        freenode(node);
    }
}

void
LDNodeFree(struct LDNode *const node)
{
    if (node) {
        freenode(node);
    }
}

cJSON *
LDNodeToJSON(const struct LDNode *const node)
{
    cJSON *result = NULL; const struct LDNode *iter = NULL;

    LD_ASSERT(node);

    switch (node->type) {
        case LDNodeNull:
            result = cJSON_CreateNull();
            break;
        case LDNodeBool:
            result = cJSON_CreateBool(node->value.boolean);
            break;
        case LDNodeNumber:
            result = cJSON_CreateNumber(node->value.number);
            break;
        case LDNodeText:
            result = cJSON_CreateString(node->value.text);
            break;
        case LDNodeObject:
            result = cJSON_CreateObject();

            if (!result) {
                return NULL;
            }

            for (iter = LDNodeObjectGetIterator(node); iter; iter=LDNodeAdvanceIterator(iter)) {
                cJSON *const child = LDNodeToJSON(iter);

                if (!child) {
                    cJSON_Delete(result);

                    return NULL;
                } else {
                    cJSON_AddItemToObject(result, iter->location.key, child);
                }
            }
            break;
        case LDNodeArray:
            result = cJSON_CreateArray();

            if (!result) {
                return NULL;
            }

            for (iter = LDNodeArrayGetIterator(node); iter; iter=LDNodeAdvanceIterator(iter)) {
                cJSON *const child = LDNodeToJSON(iter);

                if (!child) {
                    cJSON_Delete(result);

                    return NULL;
                } else {
                    cJSON_AddItemToArray(result, child);
                }
            }
            break;
    }

    return result;
}

struct LDNode *
LDNodeFromJSON(const cJSON *const json)
{
    struct LDNode *result = NULL; cJSON *iter = NULL;

    LD_ASSERT(json);

    switch (json->type) {
        case cJSON_False:
            result = LDNodeNewBool(false);
            break;
        case cJSON_True:
            result = LDNodeNewBool(true);
            break;
        case cJSON_NULL:
            result = LDNodeNewNull();
            break;
        case cJSON_Number:
            result = LDNodeNewNumber(json->valuedouble);
            break;
        case cJSON_String:
            result = LDNodeNewText(json->valuestring);
            break;
        case cJSON_Array:
            result = LDNodeNewArray();

            if (!result) {
                return NULL;
            }

            cJSON_ArrayForEach(iter, json) {
                struct LDNode *const child = LDNodeFromJSON(iter);

                if (!child) {
                    LDNodeFree(result);

                    return NULL;
                }

                LDNodeArrayAppendItem(result, child);
            }
            break;
        case cJSON_Object:
            result = LDNodeNewObject();

            if (!result) {
                return NULL;
            }

            cJSON_ArrayForEach(iter, json) {
                struct LDNode *const child = LDNodeFromJSON(iter);

                if (!child) {
                    LDNodeFree(result);

                    return NULL;
                }

                LDNodeObjectSetItem(result, iter->string, child);
            }
            break;
        default: break;
    }

    return result;
}

char *
LDNodeToJSONString(const struct LDNode *const node)
{
    char *serialized = NULL; cJSON *json = NULL;

    LD_ASSERT(node);

    json = LDNodeToJSON(node);

    if (!json) {
        return NULL;
    }

    serialized = cJSON_PrintUnformatted(json);

    cJSON_Delete(json);

    return serialized;
}

struct LDNode *
LDNodeFromJSONString(const char *const serialized)
{
    struct LDNode *node = NULL; cJSON *json = NULL;

    LD_ASSERT(serialized);

    json = cJSON_Parse(serialized);

    if (!json) {
        return NULL;
    }

    node = LDNodeFromJSON(json);

    cJSON_Delete(json);

    return node;
}

LDNodeType
LDNodeGetType(const struct LDNode *const node)
{
    LD_ASSERT(node);

    return node->type;
}

struct LDNode *
LDNodeArrayLookupIndex(const struct LDNode *const array, const unsigned int index)
{
    struct LDNode *result = NULL;

    LD_ASSERT(array); LD_ASSERT(array->type == LDNodeArray);

    HASH_FIND_INT(array->value.array, &index, result);

    return result;
}

struct LDNode *
LDNodeObjectLookupKey(const struct LDNode *const object, const char *const key)
{
    struct LDNode *result = NULL;

    LD_ASSERT(object); LD_ASSERT(object->type == LDNodeObject); LD_ASSERT(key);

    HASH_FIND_STR(object->value.object, key, result);

    return result;
}

struct LDNode *
LDNodeDeepCopy(const struct LDNode *const node)
{
    struct LDNode *result = NULL, *iter = NULL;

    LD_ASSERT(node);

    switch (LDNodeGetType(node)) {
        case LDNodeNull:
            result = LDNodeNewNull();
            break;
        case LDNodeText:
            result = LDNodeNewText(LDNodeGetText(node));
            break;
        case LDNodeNumber:
            result = LDNodeNewNumber(LDNodeGetNumber(node));
            break;
        case LDNodeBool:
            result = LDNodeNewBool(LDNodeGetBool(node));
            break;
        case LDNodeObject:
            result = LDNodeNewObject();

            if (!result) {
                return NULL;
            }

            for (iter = LDNodeObjectGetIterator(node); iter; iter=LDNodeAdvanceIterator(iter)) {
                struct LDNode *const child = LDNodeDeepCopy(iter);

                if (!child) {
                    LDNodeFree(result);

                    return NULL;
                } else {
                    LDNodeObjectSetItem(result, iter->location.key, child);
                }
            }
            break;
        case LDNodeArray:
            result = LDNodeNewArray();

            if (!result) {
                return NULL;
            }

            for (iter = LDNodeArrayGetIterator(node); iter; iter=LDNodeAdvanceIterator(iter)) {
                struct LDNode *const child = LDNodeDeepCopy(iter);

                if (!child) {
                    LDNodeFree(result);

                    return NULL;
                } else {
                    LDNodeArrayAppendItem(result, child);
                }
            }

            break;
    }

    return result;
}
