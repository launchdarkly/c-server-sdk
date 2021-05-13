#include <launchdarkly/api.h>

#include "assertion.h"
#include "store.h"
#include "utility.h"

#include "test-utils/store.h"

static struct LDJSON *
makeVersioned(const char *const key, const unsigned int version)
{
    struct LDJSON *feature, *tmp;

    LD_ASSERT(feature = LDNewObject());

    LD_ASSERT(tmp = LDNewText(key));
    LD_ASSERT(LDObjectSetKey(feature, "key", tmp));

    LD_ASSERT(tmp = LDNewNumber(version));
    LD_ASSERT(LDObjectSetKey(feature, "version", tmp));

    LD_ASSERT(tmp = LDNewBool(LDBooleanFalse));
    LD_ASSERT(LDObjectSetKey(feature, "deleted", tmp));

    return feature;
}

static void
allocateAndFree()
{
}

static void
initializeEmpty(struct LDStore *const store)
{
    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(LDStoreInitEmpty(store));
    LD_ASSERT(LDStoreInitialized(store));
}

static void
getAll(struct LDStore *const store)
{
    struct LDJSON *  all, *category;
    struct LDJSONRC *rawFlags;

    LD_ASSERT(!LDStoreInitialized(store));

    LD_ASSERT(all = LDNewObject());
    LD_ASSERT(category = LDNewObject());
    LD_ASSERT(LDObjectSetKey(all, "features", category));
    LD_ASSERT(LDObjectSetKey(category, "a", makeVersioned("a", 32)));
    LD_ASSERT(LDObjectSetKey(category, "b", makeVersioned("b", 51)));

    LD_ASSERT(LDStoreInit(store, all));

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &rawFlags));
    LDJSONRCDecrement(rawFlags);

    LD_ASSERT(LDStoreInitialized(store));
}

static void
testUpsertUpdatesAll(struct LDStore *const store)
{
    struct LDJSONRC *result;
    struct LDJSON *  flag1, *flag1Dupe, *flag2, *flag2Dupe, *all;

    LD_ASSERT(all = LDNewObject());

    LD_ASSERT(flag1 = makeVersioned("a", 52));
    LD_ASSERT(flag1Dupe = LDJSONDuplicate(flag1));
    LD_ASSERT(LDObjectSetKey(all, "a", flag1Dupe));
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, flag1));

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &result));
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(result), all));
    LDJSONRCDecrement(result);

    LD_ASSERT(flag2 = makeVersioned("b", 30));
    LD_ASSERT(flag2Dupe = LDJSONDuplicate(flag2));
    LD_ASSERT(LDObjectSetKey(all, "b", flag2Dupe));
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, flag2));

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &result));
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(result), all));
    LDJSONRCDecrement(result);

    LDObjectDeleteKey(all, "a");

    LDStoreRemove(store, LD_FLAG, "a", 60);
    LD_ASSERT(LDStoreAll(store, LD_FLAG, &result));
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(result), all));
    LDJSONRCDecrement(result);

    LDJSONFree(all);
}

static void
deletedOnly(struct LDStore *const store)
{
    struct LDJSONRC *lookup;

    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(LDStoreRemove(store, LD_FLAG, "abc", 123))

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &lookup));
    LD_ASSERT(!lookup);
}

static void
basicExists(struct LDStore *const store)
{
    struct LDJSON *  feature, *featureCopy;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(featureCopy = LDJSONDuplicate(feature));

    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), featureCopy));

    LDJSONFree(featureCopy);
    LDJSONRCDecrement(lookup);
}

static void
basicDoesNotExist(struct LDStore *const store)
{
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &lookup));
    LD_ASSERT(!lookup);
}

static void
upsertNewer(struct LDStore *const store)
{
    struct LDJSON *  feature, *featureCopy;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 5))
    LD_ASSERT(featureCopy = LDJSONDuplicate(feature));
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature));

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), featureCopy));

    LDJSONFree(featureCopy);
    LDJSONRCDecrement(lookup);
}

static void
upsertOlder(struct LDStore *const store)
{
    struct LDJSON *  feature1, *feature2, *feature1Copy;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature1 = makeVersioned("my-heap-key", 5))
    LD_ASSERT(feature1Copy = LDJSONDuplicate(feature1));
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature1));

    LD_ASSERT(feature2 = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature2));

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature1Copy));

    LDJSONFree(feature1Copy);
    LDJSONRCDecrement(lookup);
}

static void
upsertDelete(struct LDStore *const store)
{
    struct LDJSON *  feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature));

    LD_ASSERT(LDStoreRemove(store, LD_SEGMENT, "my-heap-key", 5))

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);
}

static void
conflictDifferentNamespace(struct LDStore *const store)
{
    struct LDJSON *  feature1, *feature2, *feature1Copy, *feature2Copy;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature1 = makeVersioned("my-heap-key", 3));
    LD_ASSERT(feature1Copy = LDJSONDuplicate(feature1));
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature1));

    LD_ASSERT(feature2 = makeVersioned("my-heap-key", 3));
    LD_ASSERT(feature2Copy = LDJSONDuplicate(feature2));
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, feature2));

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature1Copy));
    LDJSONRCDecrement(lookup);

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature2Copy));
    LDJSONRCDecrement(lookup);

    LDJSONFree(feature1Copy);
    LDJSONFree(feature2Copy);
}

static void
testUpsertFeatureNotAnObject(struct LDStore *const store)
{
    struct LDJSON *  feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = LDNewNumber(52));

    LD_ASSERT(!LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);
}

static void
testUpsertFeatureMissingVersion(struct LDStore *const store)
{
    struct LDJSON *  feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "version");

    LD_ASSERT(!LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);
}

static void
testUpsertFeatureVersionNotNumber(struct LDStore *const store)
{
    struct LDJSON *  feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "version");
    LDObjectSetKey(feature, "version", LDNewText("abc"));

    LD_ASSERT(!LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);
}

static void
testUpsertFeatureMissingKey(struct LDStore *const store)
{
    struct LDJSON *  feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "key");

    LD_ASSERT(!LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);
}

static void
testUpsertFeatureKeyNotText(struct LDStore *const store)
{
    struct LDJSON *  feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "key");
    LDObjectSetKey(feature, "key", LDNewNumber(52));

    LD_ASSERT(!LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);
}

static void
testUpsertFeatureDeletedNotBool(struct LDStore *const store)
{
    struct LDJSON *  feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3));
    LDObjectDeleteKey(feature, "deleted");
    LDObjectSetKey(feature, "deleted", LDNewNumber(52));

    LD_ASSERT(!LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);
}

typedef void (*test)(struct LDStore *const store);

void
runSharedStoreTests(struct LDStore *(*const prepareEmptyStore)())
{
    unsigned int    i;
    struct LDStore *store;

    test tests[] = {
        allocateAndFree,
        initializeEmpty,
        getAll,
        testUpsertUpdatesAll,
        deletedOnly,
        basicExists,
        basicDoesNotExist,
        upsertNewer,
        upsertOlder,
        upsertDelete,
        conflictDifferentNamespace,
        testUpsertFeatureNotAnObject,
        testUpsertFeatureMissingVersion,
        testUpsertFeatureVersionNotNumber,
        testUpsertFeatureMissingKey,
        testUpsertFeatureKeyNotText,
        testUpsertFeatureDeletedNotBool};

    LD_ASSERT(prepareEmptyStore);

    for (i = 0; i < sizeof(tests) / sizeof(test); i++) {
        LD_ASSERT(store = prepareEmptyStore());
        tests[i](store);
        LDStoreDestroy(store);
    }
}
