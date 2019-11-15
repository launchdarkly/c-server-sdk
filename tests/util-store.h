#include <launchdarkly/api.h>

#include "misc.h"

static struct LDJSON *
makeVersioned(const char *const key, const unsigned int version)
{
    struct LDJSON *feature, *tmp;

    LD_ASSERT(feature = LDNewObject());

    LD_ASSERT(tmp = LDNewText(key));
    LD_ASSERT(LDObjectSetKey(feature, "key", tmp));

    LD_ASSERT(tmp = LDNewNumber(version));
    LD_ASSERT(LDObjectSetKey(feature, "version", tmp));

    LD_ASSERT(tmp = LDNewBool(false));
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
    struct LDJSON *all, *category;
    struct LDJSONRC **rawFlags, **rawFlagsIter;

    LD_ASSERT(!LDStoreInitialized(store));

    LD_ASSERT(all = LDNewObject());
    LD_ASSERT(category = LDNewObject());
    LD_ASSERT(LDObjectSetKey(all, "features", category));
    LD_ASSERT(LDObjectSetKey(category, "a", makeVersioned("a", 32)));
    LD_ASSERT(LDObjectSetKey(category, "b", makeVersioned("b", 51)));

    LD_ASSERT(LDStoreInit(store, all));

    LD_ASSERT(LDStoreAll(store, LD_FLAG, &rawFlags));

    for (rawFlagsIter = rawFlags; *rawFlagsIter; rawFlagsIter++) {
        LDJSONRCDecrement(*rawFlagsIter);
    }
    LDFree(rawFlags);

    LD_ASSERT(LDStoreInitialized(store));
}

static void
deletedOnly(struct LDStore *const store)
{
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(LDStoreRemove(store, LD_FLAG, "abc", 123))

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &lookup));
    LD_ASSERT(!lookup);
}

static void
basicExists(struct LDStore *const store)
{
    struct LDJSON *feature, *featureCopy;
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
    struct LDJSON *feature, *featureCopy;
    struct LDJSONRC *lookup;

    LD_ASSERT(LDStoreInitEmpty(store));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store,  LD_SEGMENT, feature));

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
    struct LDJSON *feature1, *feature2, *feature1Copy;
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
    struct LDJSON *feature;
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
    struct LDJSON *feature1, *feature2, *feature1Copy, *feature2Copy;
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

typedef void (*test)(struct LDStore *const store);

void
runSharedStoreTests(struct LDStore *(*const prepareEmptyStore)())
{
    unsigned int i;
    struct LDStore *store;

    test tests[] = {
        allocateAndFree,
        initializeEmpty,
        getAll,
        deletedOnly,
        basicExists,
        basicDoesNotExist,
        upsertNewer,
        upsertOlder,
        upsertDelete,
        conflictDifferentNamespace
    };

    LD_ASSERT(prepareEmptyStore);

    for (i = 0; i < sizeof(tests) / sizeof(test); i++) {
        LD_ASSERT(store = prepareEmptyStore());
        tests[i](store);
        LDStoreDestroy(store);
    }
}
