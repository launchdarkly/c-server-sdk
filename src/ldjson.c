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
