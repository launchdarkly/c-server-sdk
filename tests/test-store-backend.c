#include <launchdarkly/api.h>

#include "assertion.h"
#include "utility.h"
#include "store.h"

#include "test-utils/flags.h"

static LDBoolean
mockFailInit(void *const context,
    const struct LDStoreCollectionState *collections,
    const unsigned int collectionCount)
{
    (void)context;
    LD_ASSERT(collections || collectionCount == 0);

    return false;
}

static LDBoolean
mockFailGet(void *const context, const char *const kind,
    const char *const featureKey,
    struct LDStoreCollectionItem *const result)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(result);

    return false;
}

static LDBoolean
mockFailAll(void *const context, const char *const kind,
    struct LDStoreCollectionItem **const result,
    unsigned int *const resultCount)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(result);
    LD_ASSERT(resultCount);

    return false;
}

static LDBoolean
mockFailUpsert(void *const context, const char *const kind,
    const struct LDStoreCollectionItem *const feature,
    const char *const featureKey)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(feature);
    LD_ASSERT(featureKey);

    return false;
}

static LDBoolean
mockFailInitialized(void *const context)
{
    (void)context;

    return false;
}

static void
mockFailDestructor(void *const context)
{
    (void)context;
}

static struct LDStoreInterface *
makeMockFailInterface()
{
    struct LDStoreInterface *handle;

    LD_ASSERT(handle = (struct LDStoreInterface *)
        LDAlloc(sizeof(struct LDStoreInterface)));

    handle->context     = NULL;
    handle->init        = mockFailInit;
    handle->get         = mockFailGet;
    handle->all         = mockFailAll;
    handle->upsert      = mockFailUpsert;
    handle->initialized = mockFailInitialized;
    handle->destructor  = mockFailDestructor;

    return handle;
}

static struct LDStore *
prepareStore(struct LDStoreInterface *const handle)
{
    struct LDStore *store;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew(""));
    if (handle) {
        LDConfigSetFeatureStoreBackend(config, handle);
    }
    LD_ASSERT(store = LDStoreNew(config));
    config->storeBackend = NULL;

    LDConfigFree(config);

    return store;
}

static void
testFailInit()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;

    LD_ASSERT(handle = makeMockFailInterface());
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreInitEmpty(store));

    LDStoreDestroy(store);
}

static void
testFailGet()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    LD_ASSERT(handle = makeMockFailInterface());
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreGet(store, LD_FLAG, "abc", &item));
    LD_ASSERT(!item);

    LDStoreDestroy(store);
}

static void
testFailAll()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *items;

    LD_ASSERT(handle = makeMockFailInterface());
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreAll(store, LD_FLAG, &items));
    LD_ASSERT(!items);

    LDStoreDestroy(store);
}

static void
testFailUpsert()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSON *flag;

    LD_ASSERT(flag = makeMinimalFlag("abc", 52, true, false));
    LD_ASSERT(handle = makeMockFailInterface());
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreUpsert(store, LD_FLAG, flag));

    LDStoreDestroy(store);
}

static void
testFailRemove()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;

    LD_ASSERT(handle = makeMockFailInterface());
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreRemove(store, LD_FLAG, "abc", 52));

    LDStoreDestroy(store);
}

static void
testFailInitialized()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;

    LD_ASSERT(handle = makeMockFailInterface());
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreInitialized(store));

    LDStoreDestroy(store);
}

static LDBoolean
mockFailGetInvalidJSON(void *const context, const char *const kind,
    const char *const featureKey,
    struct LDStoreCollectionItem *const result)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(result);

    result->buffer     = LDStrDup("bad json");
    result->bufferSize = strlen(result->buffer) + 1;
    result->version    = 52;

    return true;
}

static void
testFailGetInvalidJSON()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    LD_ASSERT(handle = makeMockFailInterface());
    handle->get = mockFailGetInvalidJSON;
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreGet(store, LD_FLAG, "abc", &item));
    LD_ASSERT(!item);

    LDStoreDestroy(store);
}

static LDBoolean
mockFailAllInvalidJSON(void *const context, const char *const kind,
    struct LDStoreCollectionItem **const result,
    unsigned int *const resultCount)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(result);
    LD_ASSERT(resultCount);

    LD_ASSERT(*result = LDAlloc(sizeof(struct LDStoreCollectionItem)));
    *resultCount = 1;

    (*result)->buffer     = LDStrDup("bad json");
    (*result)->bufferSize = strlen((*result)->buffer) + 1;
    (*result)->version    = 52;

    return true;
}

static void
testFailAllInvalidJSON()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *items;

    LD_ASSERT(handle = makeMockFailInterface());
    handle->all = mockFailAllInvalidJSON;
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreAll(store, LD_FLAG, &items));
    LD_ASSERT(!items);

    LDStoreDestroy(store);
}

static LDBoolean
mockFailGetInvalidFlag(void *const context, const char *const kind,
    const char *const featureKey,
    struct LDStoreCollectionItem *const result)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(result);

    result->buffer     = LDStrDup("52");
    result->bufferSize = strlen(result->buffer) + 1;
    result->version    = 52;

    return true;
}

static void
testFailGetInvalidFlag()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    LD_ASSERT(handle = makeMockFailInterface());
    handle->get = mockFailGetInvalidFlag;
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(!LDStoreGet(store, LD_FLAG, "abc", &item));
    LD_ASSERT(!item);

    LDStoreDestroy(store);
}

static LDBoolean
mockFailAllInvalidFlag(void *const context, const char *const kind,
    struct LDStoreCollectionItem **const result,
    unsigned int *const resultCount)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(result);
    LD_ASSERT(resultCount);

    LD_ASSERT(*result = LDAlloc(sizeof(struct LDStoreCollectionItem)));
    *resultCount = 1;

    (*result)->buffer     = LDStrDup("52");
    (*result)->bufferSize = strlen((*result)->buffer) + 1;
    (*result)->version    = 52;

    return true;
}

static void
testFailAllInvalidFlag()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *items;
    struct LDJSON *expected;

    LD_ASSERT(expected = LDNewObject());
    LD_ASSERT(handle = makeMockFailInterface());
    handle->all = mockFailAllInvalidFlag;
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &items));
    LD_ASSERT(items);
    LD_ASSERT(LDJSONCompare(expected, LDJSONRCGet(items)));

    LDJSONFree(expected);
    LDJSONRCDecrement(items);
    LDStoreDestroy(store);
}

static bool staticInitializedValue;
static unsigned int staticInitializedCount;

static LDBoolean
mockStaticInitialized(void *const context)
{
    (void)context;

    staticInitializedCount++;

    return staticInitializedValue;
}

static void
testInitializedCache()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;

    LD_ASSERT(handle = makeMockFailInterface());
    handle->initialized = mockStaticInitialized;
    LD_ASSERT(store = prepareStore(handle));

    staticInitializedValue = false;
    staticInitializedCount = 0;

    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(staticInitializedCount == 1);
    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(staticInitializedCount == 1);
    LDi_expireAll(store);
    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(staticInitializedCount == 2);
    staticInitializedValue = true;
    LDi_expireAll(store);
    LD_ASSERT(LDStoreInitialized(store));
    LD_ASSERT(staticInitializedCount == 3);
    LD_ASSERT(LDStoreInitialized(store));
    LD_ASSERT(staticInitializedCount == 3);

    LDStoreDestroy(store);
}

static unsigned int staticGetCount;
static struct LDJSON *staticGetValue;
static char *staticGetKey;

static LDBoolean
mockStaticGet(void *const context, const char *const kind,
    const char *const featureKey,
    struct LDStoreCollectionItem *const result)
{
    (void)context;

    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(strcmp(featureKey, staticGetKey) == 0);
    LD_ASSERT(result);

    if (staticGetValue) {
        LD_ASSERT(result->buffer = LDJSONSerialize(staticGetValue));
        result->bufferSize = strlen(result->buffer) + 1;
        result->version = LDGetNumber(
            LDObjectLookup(staticGetValue, "version"));
    } else {
        result->buffer     = NULL;
        result->bufferSize = 0;
        result->version    = 0;
    }

    staticGetCount++;

    return true;
}

static void
testGetCache()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item1, *item2;

    item1 = NULL;
    item2 = NULL;

    LD_ASSERT(handle = makeMockFailInterface());
    handle->get = mockStaticGet;
    LD_ASSERT(store = prepareStore(handle));

    staticGetValue = NULL;
    staticGetKey = "abc";
    staticGetCount = 0;

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &item1));
    LD_ASSERT(!item1);
    LD_ASSERT(staticGetCount == 1);

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &item1));
    LD_ASSERT(!item1);
    LD_ASSERT(staticGetCount == 1);

    LDi_expireAll(store);

    LD_ASSERT(staticGetValue = makeMinimalFlag("abc", 12, true, true));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &item1));
    LD_ASSERT(item1);
    LD_ASSERT(staticGetCount == 2);
    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &item2));
    LD_ASSERT(item2);
    LD_ASSERT(staticGetCount == 2);

    LD_ASSERT(LDJSONCompare(LDJSONRCGet(item1), LDJSONRCGet(item2)));

    LDJSONRCDecrement(item1);
    LDJSONRCDecrement(item2);
    LDStoreDestroy(store);
}

static unsigned int staticUpsertCount;
static struct LDJSON *staticUpsertValue;
static char *staticUpsertKey;

static LDBoolean
mockStaticUpsert(void *const context, const char *const kind,
    const struct LDStoreCollectionItem *const feature,
    const char *const featureKey)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(feature);
    LD_ASSERT(featureKey);
    LD_ASSERT(strcmp(featureKey, staticUpsertKey) == 0);

    staticUpsertCount++;

    return true;
}

static void
testUpsertCache()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    LD_ASSERT(handle = makeMockFailInterface());
    handle->upsert = mockStaticUpsert;
    LD_ASSERT(store = prepareStore(handle));

    staticUpsertKey = "abc";
    LD_ASSERT(staticUpsertValue = makeMinimalFlag("abc", 12, true, true));

    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, staticUpsertValue));
    LD_ASSERT(staticUpsertCount == 1);

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &item));
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(item), staticUpsertValue));
    LDJSONRCDecrement(item);

    LD_ASSERT(LDStoreRemove(store, LD_FLAG, "abc", 52));
    LD_ASSERT(staticUpsertCount == 2);

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &item));
    LD_ASSERT(!item);

    LDStoreDestroy(store);
}

static struct LDJSON *staticAllValue;
static unsigned int staticAllCount;

static LDBoolean
mockStaticAll(void *const context, const char *const kind,
    struct LDStoreCollectionItem **const result,
    unsigned int *const resultCount)
{
    (void)context;
    LD_ASSERT(kind);
    LD_ASSERT(result);
    LD_ASSERT(resultCount);

    if (staticAllValue) {
        struct LDJSON *valueIter;
        struct LDStoreCollectionItem *resultIter;
        size_t resultBytes;

        *resultCount = LDCollectionGetSize(staticAllValue);
        LD_ASSERT(*resultCount);
        resultBytes = sizeof(struct LDStoreCollectionItem) * (*resultCount);

        LD_ASSERT(*result = LDAlloc(resultBytes));
        resultIter = *result;

        for (valueIter = LDGetIter(staticAllValue); valueIter;
            valueIter = LDIterNext(valueIter))
        {
            LD_ASSERT(resultIter->buffer = LDJSONSerialize(valueIter));
            resultIter->bufferSize = strlen(resultIter->buffer) + 1;
            resultIter->version =
                LDGetNumber(LDObjectLookup(valueIter, "version"));

            resultIter++;
        }
    } else {
        *result      = NULL;
        *resultCount = 0;
    }

    staticAllCount++;

    return true;
}

static void
testAllCache()
{
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSON *empty, *tmp, *full;
    struct LDJSONRC *values;

    staticAllValue    = NULL;
    staticAllCount    = 0;
    staticUpsertCount = 0;
    staticUpsertKey   = 0;

    LD_ASSERT(handle = makeMockFailInterface());
    handle->all = mockStaticAll;
    handle->upsert = mockStaticUpsert;
    LD_ASSERT(store = prepareStore(handle));

    LD_ASSERT(empty = LDNewObject());

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &values));
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(values), empty));
    LD_ASSERT(staticAllCount == 1);
    LDJSONRCDecrement(values);

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &values));
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(values), empty));
    LD_ASSERT(staticAllCount == 1);
    LDJSONRCDecrement(values);

    LDi_expireAll(store);

    LD_ASSERT(full = LDNewObject());

    staticUpsertKey = "abc";
    LD_ASSERT(tmp = makeMinimalFlag("abc", 12, true, true));
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, LDJSONDuplicate(tmp)));
    LD_ASSERT(LDObjectSetKey(full, "abc", tmp));
    LD_ASSERT(staticUpsertCount == 1);

    staticUpsertKey = "123";
    LD_ASSERT(tmp = makeMinimalFlag("123", 13, true, true));
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, LDJSONDuplicate(tmp)));
    LD_ASSERT(LDObjectSetKey(full, "123", tmp));
    LD_ASSERT(staticUpsertCount == 2);

    staticAllValue = full;

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &values));
    LD_ASSERT(staticAllCount == 2);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(values), full));
    LDJSONRCDecrement(values);

    LDi_expireAll(store);

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &values));
    LD_ASSERT(staticAllCount == 3);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(values), full));
    LDJSONRCDecrement(values);

    LDJSONFree(full);
    LDJSONFree(empty);

    LDStoreDestroy(store);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    testFailInit();
    testFailGet();
    testFailAll();
    testFailRemove();
    testFailUpsert();
    testFailInitialized();
    testFailGetInvalidJSON();
    testFailAllInvalidJSON();
    testFailGetInvalidFlag();
    testFailAllInvalidFlag();
    testInitializedCache();
    testGetCache();
    testUpsertCache();
    testAllCache();

    return 0;
}
