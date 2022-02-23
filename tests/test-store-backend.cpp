#include "gtest/gtest.h"
#include "commonfixture.h"
#include "concurrencyfixture.h"

extern "C" {
#include <string.h>

#include <launchdarkly/api.h>

#include "assertion.h"
#include "store.h"
#include "utility.h"

#include "test-utils/flags.h"
}

// Inherit from the CommonFixture to give a reasonable name for the test output.
// Any custom setup and teardown would happen in this derived class.
class StoreBackendFixture : public CommonFixture {
};


static LDBoolean
mockFailInit(
        void *const context,
        const struct LDStoreCollectionState *collections,
        const unsigned int collectionCount) {
    (void) context;
    LD_ASSERT(collections || collectionCount == 0);

    return LDBooleanFalse;
}

static LDBoolean
mockFailGet(
        void *const context,
        const char *const kind,
        const char *const featureKey,
        struct LDStoreCollectionItem *const result) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(result);

    return LDBooleanFalse;
}

static LDBoolean
mockFailAll(
        void *const context,
        const char *const kind,
        struct LDStoreCollectionItem **const result,
        unsigned int *const resultCount) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(result);
    LD_ASSERT(resultCount);

    return LDBooleanFalse;
}

static LDBoolean
mockFailUpsert(
        void *const context,
        const char *const kind,
        const struct LDStoreCollectionItem *const feature,
        const char *const featureKey) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(feature);
    LD_ASSERT(featureKey);

    return LDBooleanFalse;
}

static LDBoolean
mockFailInitialized(void *const context) {
    (void) context;

    return LDBooleanFalse;
}

static void
mockFailDestructor(void *const context) {
    (void) context;
}

static struct LDStoreInterface *
makeMockFailInterface() {
    struct LDStoreInterface *handle;

    LD_ASSERT(
            handle = (struct LDStoreInterface *) LDAlloc(
                    sizeof(struct LDStoreInterface)));

    handle->context = NULL;
    handle->init = mockFailInit;
    handle->get = mockFailGet;
    handle->all = mockFailAll;
    handle->upsert = mockFailUpsert;
    handle->initialized = mockFailInitialized;
    handle->destructor = mockFailDestructor;

    return handle;
}

static struct LDStore *
prepareStore(struct LDStoreInterface *const handle, unsigned int storeCacheMilliseconds = 30000) {
    struct LDStore *store;
    struct LDConfig *config;

    LD_ASSERT(config = LDConfigNew(""));
    config->storeCacheMilliseconds = storeCacheMilliseconds;

    if (handle) {
        LDConfigSetFeatureStoreBackend(config, handle);
    }
    LD_ASSERT(store = LDStoreNew(config));
    config->storeBackend = NULL;

    LDConfigFree(config);

    return store;
}

TEST_F(StoreBackendFixture, FailInit) {
    struct LDStore *store;
    struct LDStoreInterface *handle;

    ASSERT_TRUE(handle = makeMockFailInterface());
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreInitEmpty(store));

    LDStoreDestroy(store);
}

TEST_F(StoreBackendFixture, FailGet) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    ASSERT_TRUE(handle = makeMockFailInterface());
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreGet(store, LD_FLAG, "abc", &item));
    ASSERT_FALSE(item);

    LDStoreDestroy(store);
}

TEST_F(StoreBackendFixture, FailAll) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *items;

    ASSERT_TRUE(handle = makeMockFailInterface());
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreAll(store, LD_FLAG, &items));
    ASSERT_FALSE(items);

    LDStoreDestroy(store);
}

TEST_F(StoreBackendFixture, FailUpsert) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSON *flag;

    ASSERT_TRUE(flag = makeMinimalFlag("abc", 52, LDBooleanTrue, LDBooleanFalse));
    ASSERT_TRUE(handle = makeMockFailInterface());
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreUpsert(store, LD_FLAG, flag));

    LDStoreDestroy(store);
}

TEST_F(StoreBackendFixture, FailRemove) {
    struct LDStore *store;
    struct LDStoreInterface *handle;

    ASSERT_TRUE(handle = makeMockFailInterface());
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreRemove(store, LD_FLAG, "abc", 52));

    LDStoreDestroy(store);
}

TEST_F(StoreBackendFixture, FailInitialized) {
    struct LDStore *store;
    struct LDStoreInterface *handle;

    ASSERT_TRUE(handle = makeMockFailInterface());
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreInitialized(store));

    LDStoreDestroy(store);
}

static LDBoolean
mockFailGetInvalidJSON(
        void *const context,
        const char *const kind,
        const char *const featureKey,
        struct LDStoreCollectionItem *const result) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(result);

    result->buffer = LDStrDup("bad json");
    result->bufferSize = strlen((char *) result->buffer) + 1;
    result->version = 52;

    return LDBooleanTrue;
}

TEST_F(StoreBackendFixture, FailGetInvalidJSON) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->get = mockFailGetInvalidJSON;
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreGet(store, LD_FLAG, "abc", &item));
    ASSERT_FALSE(item);

    LDStoreDestroy(store);
}

static LDBoolean
mockFailAllInvalidJSON(
        void *const context,
        const char *const kind,
        struct LDStoreCollectionItem **const result,
        unsigned int *const resultCount) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(result);
    LD_ASSERT(resultCount);

    LD_ASSERT(*result = static_cast<LDStoreCollectionItem *>(LDAlloc(sizeof(struct LDStoreCollectionItem))));
    *resultCount = 1;

    (*result)->buffer = LDStrDup("bad json");
    (*result)->bufferSize = strlen((char *) (*result)->buffer) + 1;
    (*result)->version = 52;

    return LDBooleanTrue;
}

TEST_F(StoreBackendFixture, FailAllInvalidJSON) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *items;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->all = mockFailAllInvalidJSON;
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreAll(store, LD_FLAG, &items));
    ASSERT_FALSE(items);

    LDStoreDestroy(store);
}

static LDBoolean
mockFailGetInvalidFlag(
        void *const context,
        const char *const kind,
        const char *const featureKey,
        struct LDStoreCollectionItem *const result) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(result);

    result->buffer = LDStrDup("52");
    result->bufferSize = strlen((char *) result->buffer) + 1;
    result->version = 52;

    return LDBooleanTrue;
}

TEST_F(StoreBackendFixture, FailGetInvalidFlag) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->get = mockFailGetInvalidFlag;
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_FALSE(LDStoreGet(store, LD_FLAG, "abc", &item));
    ASSERT_FALSE(item);

    LDStoreDestroy(store);
}

static LDBoolean
mockFailAllInvalidFlag(
        void *const context,
        const char *const kind,
        struct LDStoreCollectionItem **const result,
        unsigned int *const resultCount) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(result);
    LD_ASSERT(resultCount);

    LD_ASSERT(*result = static_cast<LDStoreCollectionItem *>(LDAlloc(sizeof(struct LDStoreCollectionItem))));
    *resultCount = 1;

    (*result)->buffer = LDStrDup("52");
    (*result)->bufferSize = strlen((char *) (*result)->buffer) + 1;
    (*result)->version = 52;

    return LDBooleanTrue;
}

TEST_F(StoreBackendFixture, FailAllInvalidFlag) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *items;
    struct LDJSON *expected;

    ASSERT_TRUE(expected = LDNewObject());
    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->all = mockFailAllInvalidFlag;
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &items));
    ASSERT_TRUE(items);
    ASSERT_TRUE(LDJSONCompare(expected, LDJSONRCGet(items)));

    LDJSONFree(expected);
    LDJSONRCDecrement(items);
    LDStoreDestroy(store);
}

static LDBoolean staticInitializedValue;
static unsigned int staticInitializedCount;

static LDBoolean
mockStaticInitialized(void *const context) {
    (void) context;

    staticInitializedCount++;

    return staticInitializedValue;
}

TEST_F(StoreBackendFixture, InitializedCache) {
    struct LDStore *store;
    struct LDStoreInterface *handle;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->initialized = mockStaticInitialized;
    ASSERT_TRUE(store = prepareStore(handle));

    staticInitializedValue = LDBooleanFalse;
    staticInitializedCount = 0;

    ASSERT_FALSE(LDStoreInitialized(store));
    ASSERT_EQ(staticInitializedCount, 1);
    ASSERT_FALSE(LDStoreInitialized(store));
    ASSERT_EQ(staticInitializedCount, 1);
    LDi_expireAll(store);
    ASSERT_FALSE(LDStoreInitialized(store));
    ASSERT_EQ(staticInitializedCount, 2);
    staticInitializedValue = LDBooleanTrue;
    LDi_expireAll(store);
    ASSERT_TRUE(LDStoreInitialized(store));
    ASSERT_EQ(staticInitializedCount, 3);
    ASSERT_TRUE(LDStoreInitialized(store));
    ASSERT_EQ(staticInitializedCount, 3);

    LDStoreDestroy(store);
}

static unsigned int staticGetCount;
static struct LDJSON *staticGetValue;
static char const *staticGetKey;

static LDBoolean
mockStaticGet(
        void *const context,
        const char *const kind,
        const char *const featureKey,
        struct LDStoreCollectionItem *const result) {
    (void) context;

    LD_ASSERT(kind);
    LD_ASSERT(featureKey);
    LD_ASSERT(strcmp(featureKey, staticGetKey) == 0);
    LD_ASSERT(result);

    if (staticGetValue) {
        LD_ASSERT(result->buffer = LDJSONSerialize(staticGetValue));
        result->bufferSize = strlen((char *) result->buffer) + 1;
        result->version =
                LDGetNumber(LDObjectLookup(staticGetValue, "version"));
    } else {
        result->buffer = NULL;
        result->bufferSize = 0;
        result->version = 0;
    }

    staticGetCount++;

    return LDBooleanTrue;
}

TEST_F(StoreBackendFixture, GetCache) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item1, *item2;

    item1 = NULL;
    item2 = NULL;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->get = mockStaticGet;
    ASSERT_TRUE(store = prepareStore(handle));

    staticGetValue = NULL;
    staticGetKey = "abc";
    staticGetCount = 0;

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &item1));
    ASSERT_FALSE(item1);
    ASSERT_EQ(staticGetCount, 1);

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &item1));
    ASSERT_FALSE(item1);
    ASSERT_EQ(staticGetCount, 1);

    LDi_expireAll(store);

    ASSERT_TRUE(
            staticGetValue =
                    makeMinimalFlag("abc", 12, LDBooleanTrue, LDBooleanTrue));

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &item1));
    ASSERT_TRUE(item1);
    ASSERT_EQ(staticGetCount, 2);
    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &item2));
    ASSERT_TRUE(item2);
    ASSERT_EQ(staticGetCount, 2);

    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(item1), LDJSONRCGet(item2)));

    LDJSONFree(staticGetValue);
    LDJSONRCDecrement(item1);
    LDJSONRCDecrement(item2);
    LDStoreDestroy(store);
}

static unsigned int staticUpsertCount;
static struct LDJSON *staticUpsertValue;
static char const *staticUpsertKey;

static LDBoolean
mockStaticUpsert(
        void *const context,
        const char *const kind,
        const struct LDStoreCollectionItem *const feature,
        const char *const featureKey) {
    (void) context;
    LD_ASSERT(kind);
    LD_ASSERT(feature);
    LD_ASSERT(featureKey);
    LD_ASSERT(strcmp(featureKey, staticUpsertKey) == 0);

    staticUpsertCount++;

    return LDBooleanTrue;
}

TEST_F(StoreBackendFixture, UpsertCache) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSONRC *item;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->upsert = mockStaticUpsert;
    ASSERT_TRUE(store = prepareStore(handle));

    staticUpsertKey = "abc";
    ASSERT_TRUE(
            staticUpsertValue =
                    makeMinimalFlag("abc", 12, LDBooleanTrue, LDBooleanTrue));

    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, staticUpsertValue));
    ASSERT_EQ(staticUpsertCount, 1);

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &item));
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(item), staticUpsertValue));
    LDJSONRCDecrement(item);

    ASSERT_TRUE(LDStoreRemove(store, LD_FLAG, "abc", 52));
    ASSERT_EQ(staticUpsertCount, 2);

    ASSERT_TRUE(LDStoreGet(store, LD_FLAG, "abc", &item));
    ASSERT_FALSE(item);

    LDStoreDestroy(store);
}

static struct LDJSON *staticAllValue;
static unsigned int staticAllCount;

static LDBoolean
mockStaticAll(
        void *const context,
        const char *const kind,
        struct LDStoreCollectionItem **const result,
        unsigned int *const resultCount) {
    (void) context;
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

        LD_ASSERT(*result = static_cast<LDStoreCollectionItem *>(LDAlloc(resultBytes)));
        resultIter = *result;

        for (valueIter = LDGetIter(staticAllValue); valueIter;
             valueIter = LDIterNext(valueIter)) {
            LD_ASSERT(resultIter->buffer = LDJSONSerialize(valueIter));
            resultIter->bufferSize = strlen((char *) resultIter->buffer) + 1;
            resultIter->version =
                    LDGetNumber(LDObjectLookup(valueIter, "version"));

            resultIter++;
        }
    } else {
        *result = NULL;
        *resultCount = 0;
    }

    staticAllCount++;

    return LDBooleanTrue;
}

TEST_F(StoreBackendFixture, AllCache) {
    struct LDStore *store;
    struct LDStoreInterface *handle;
    struct LDJSON *empty, *tmp, *full;
    struct LDJSONRC *values;

    staticAllValue = NULL;
    staticAllCount = 0;
    staticUpsertCount = 0;
    staticUpsertKey = 0;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->all = mockStaticAll;
    handle->upsert = mockStaticUpsert;
    ASSERT_TRUE(store = prepareStore(handle));

    ASSERT_TRUE(empty = LDNewObject());

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &values));
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(values), empty));
    ASSERT_EQ(staticAllCount, 1);
    LDJSONRCDecrement(values);

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &values));
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(values), empty));
    ASSERT_EQ(staticAllCount, 1);
    LDJSONRCDecrement(values);

    LDi_expireAll(store);

    ASSERT_TRUE(full = LDNewObject());

    staticUpsertKey = "abc";
    ASSERT_TRUE(tmp = makeMinimalFlag("abc", 12, LDBooleanTrue, LDBooleanTrue));
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, LDJSONDuplicate(tmp)));
    ASSERT_TRUE(LDObjectSetKey(full, "abc", tmp));
    ASSERT_EQ(staticUpsertCount, 1);

    staticUpsertKey = "123";
    ASSERT_TRUE(tmp = makeMinimalFlag("123", 13, LDBooleanTrue, LDBooleanTrue));
    ASSERT_TRUE(LDStoreUpsert(store, LD_FLAG, LDJSONDuplicate(tmp)));
    ASSERT_TRUE(LDObjectSetKey(full, "123", tmp));
    ASSERT_EQ(staticUpsertCount, 2);

    staticAllValue = full;

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &values));
    ASSERT_EQ(staticAllCount, 2);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(values), full));
    LDJSONRCDecrement(values);

    LDi_expireAll(store);

    ASSERT_TRUE(LDStoreAll(store, LD_FLAG, &values));
    ASSERT_EQ(staticAllCount, 3);
    ASSERT_TRUE(LDJSONCompare(LDJSONRCGet(values), full));
    LDJSONRCDecrement(values);

    LDJSONFree(full);
    LDJSONFree(empty);

    LDStoreDestroy(store);
}

// It was previously possible to encounter a double-free when calling LDStoreInitialized,
// triggered by deleteAndRemoveCacheItem.
// LDStoreInitialized did not hold a write-lock while removing the INIT_CHECKED_KEY item, which could end up
// corrupting the hash table, or aborting, presumably depending on the platform.
// This test can reliably trigger a SIGABRT/SIGSEGV on various hosts by creating many competing threads
// and utilizing a short cache-timeout.
// Note: This test does not seem to trigger valgrind's race detection (helgrind or drd).
// Rather, the normal unit tests (especially OSX) tend to abort.
TEST_F(ConcurrencyFixture, TestStoreInitializedDoubleFree) {
    struct LDStore *store;
    struct LDStoreInterface *handle;

    ASSERT_TRUE(handle = makeMockFailInterface());
    handle->initialized = mockStaticInitialized;
    ASSERT_TRUE(store = prepareStore(handle, 5 /* milliseconds */));

    const std::size_t THREAD_CONCURRENCY = 100;
    const std::size_t CALLS = 10;

    Defer([store]() { LDStoreDestroy(store); });

    RunMany(THREAD_CONCURRENCY, [=]() {
        for (std::size_t j = 0; j < CALLS; j++) {
            Sleep();
            LDStoreInitialized(store);
        }
    });
}
