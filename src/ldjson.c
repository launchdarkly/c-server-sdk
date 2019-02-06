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

struct LDArrayIter *
LDArrayGetIter(const struct LDJSON *const rawarray)
{
    const cJSON *const array = (const cJSON *const)rawarray;

    LD_ASSERT(array); LD_ASSERT(cJSON_IsArray(array));

    return (struct LDArrayIter *)array->child;
}

struct LDJSON *
LDArrayIterNext(struct LDArrayIter **const rawiter)
{
    cJSON **const iter = (cJSON **const)rawiter;
    cJSON *const currentvalue = *iter;

    LD_ASSERT(iter);

    *iter = currentvalue->next;

    return (struct LDJSON *)currentvalue;
}

struct LDJSON *
LDArrayIterValue(const struct LDArrayIter *const iter)
{
    return (struct LDJSON *)iter;
}

void
LDArrayIterFree(struct LDArrayIter *const iter)
{
    (void)iter; /* no-op */
}
