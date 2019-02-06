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
