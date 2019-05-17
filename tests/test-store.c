#include <launchdarkly/api.h>

#include "store.h"
#include "misc.h"

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore *store;
    LD_ASSERT(store = LDMakeInMemoryStore());
    LD_ASSERT(!LDStoreInitialized(store));
    LD_ASSERT(LDStoreInitEmpty(store));

    return store;
}

static void
allocateAndFree()
{
    struct LDStore *const store = prepareEmptyStore();

    LDStoreDestroy(store);
}

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

static struct LDJSON *
makeDeleted(const char *const key, const unsigned int version)
{
    struct LDJSON *feature, *tmp;

    LD_ASSERT(feature = makeVersioned(key, version));

    LD_ASSERT(tmp = LDNewBool(true));
    LD_ASSERT(LDObjectSetKey(feature, "deleted", tmp));

    return feature;
}

static void
deletedOnly()
{
    struct LDStore *store;
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeDeleted("abc", 123));

    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &lookup));
    LD_ASSERT(!lookup);

    LDStoreDestroy(store);
}

static void
basicExists()
{
    struct LDStore *store;
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))

    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, feature));

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature));
    LDJSONRCDecrement(lookup);

    LDStoreDestroy(store);
}

static void
basicDoesNotExist()
{
    struct LDStore *store;
    struct LDJSONRC *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "abc", &lookup));
    LD_ASSERT(!lookup);

    LDStoreDestroy(store);
}

static void
upsertNewer()
{
    struct LDStore *store;
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store,  LD_SEGMENT, feature));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 5))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature));

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature));
    LDJSONRCDecrement(lookup);

    LDStoreDestroy(store);
}

static void
upsertOlder()
{
    struct LDStore *store;
    struct LDJSON *feature1, *feature2;
    struct LDJSONRC *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature1 = makeVersioned("my-heap-key", 5))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature1));

    LD_ASSERT(feature2 = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature2));

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature1));

    LDJSONRCDecrement(lookup);
    LDStoreDestroy(store);
}

static void
upsertDelete()
{
    struct LDStore *store;
    struct LDJSON *feature;
    struct LDJSONRC *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature));

    LD_ASSERT(feature = makeDeleted("my-heap-key", 5))
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature));

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(!lookup);

    LDStoreDestroy(store);
}

static void
conflictDifferentNamespace()
{
    struct LDStore *store;
    struct LDJSON *feature1, *feature2;
    struct LDJSONRC *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature1 = makeVersioned("my-heap-key", 3));
    LD_ASSERT(LDStoreUpsert(store, LD_SEGMENT, feature1));

    LD_ASSERT(feature2 = makeVersioned("my-heap-key", 3));
    LD_ASSERT(LDStoreUpsert(store, LD_FLAG, feature2));

    LD_ASSERT(LDStoreGet(store, LD_SEGMENT, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature1));
    LDJSONRCDecrement(lookup);

    LD_ASSERT(LDStoreGet(store, LD_FLAG, "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(LDJSONRCGet(lookup), feature2));
    LDJSONRCDecrement(lookup);

    LDStoreDestroy(store);
}

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);
    LDGlobalInit();

    allocateAndFree();
    deletedOnly();
    basicExists();
    basicDoesNotExist();
    upsertNewer();
    upsertOlder();
    upsertDelete();
    conflictDifferentNamespace();

    return 0;
}
