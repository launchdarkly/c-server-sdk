#include "ldinternal.h"
#include "ldstore.h"

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore *store;
    struct LDJSON *sets;
    struct LDJSON *tmp;

    LD_ASSERT(store = LDMakeInMemoryStore());
    LD_ASSERT(!LDStoreInitialized(store));

    LD_ASSERT(sets = LDNewObject());

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(sets, "segments", tmp));

    LD_ASSERT(tmp = LDNewObject());
    LD_ASSERT(LDObjectSetKey(sets, "flags", tmp));

    LD_ASSERT(LDStoreInit(store, sets));
    LD_ASSERT(LDStoreInitialized(store));

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
    struct LDJSON *feature;
    struct LDJSON *tmp;

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
    struct LDJSON *feature;
    struct LDJSON *tmp;

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
    struct LDJSON *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeDeleted("abc", 123));

    LD_ASSERT(LDStoreUpsert(store, "flags", feature));

    LD_ASSERT(LDStoreGet(store, "flags", "abc", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDi_isDeleted(lookup));

    LDStoreDestroy(store);
}

static void
basicExists()
{
    struct LDStore *store;
    struct LDJSON *feature;
    struct LDJSON *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))

    LD_ASSERT(LDStoreUpsert(store, "flags", feature));

    LD_ASSERT(LDStoreGet(store, "flags", "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(lookup, feature));

    LDJSONFree(lookup);

    LDStoreDestroy(store);
}

static void
basicDoesNotExist()
{
    struct LDStore *store;
    struct LDJSON *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(LDStoreGet(store, "flags", "abc", &lookup));
    LD_ASSERT(!lookup);

    LDStoreDestroy(store);
}

static void
upsertNewer()
{
    struct LDStore *store;
    struct LDJSON *feature;
    struct LDJSON *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 5))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature));

    LD_ASSERT(LDStoreGet(store, "segments", "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(lookup, feature));

    LDJSONFree(lookup);

    LDStoreDestroy(store);
}

static void
upsertOlder()
{
    struct LDStore *store;
    struct LDJSON *feature1;
    struct LDJSON *feature2;
    struct LDJSON *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature1 = makeVersioned("my-heap-key", 5))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature1));

    LD_ASSERT(feature2 = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature2));

    LD_ASSERT(LDStoreGet(store, "segments", "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(lookup, feature1));

    LDJSONFree(lookup);

    LDStoreDestroy(store);
}

static void
upsertDelete()
{
    struct LDStore *store;
    struct LDJSON *feature;
    struct LDJSON *lookup;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature));

    LD_ASSERT(feature = makeDeleted("my-heap-key", 5))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature));

    LD_ASSERT(LDStoreGet(store, "segments", "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDi_isDeleted(lookup));

    LDStoreDestroy(store);
}

static void
conflictDifferentNamespace()
{
    struct LDStore *store;
    struct LDJSON *feature1;
    struct LDJSON *feature2;
    struct LDJSON *lookup;;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature1 = makeVersioned("my-heap-key", 3));
    LD_ASSERT(LDStoreUpsert(store, "segments", feature1));

    LD_ASSERT(feature2 = makeVersioned("my-heap-key", 3));
    LD_ASSERT(LDStoreUpsert(store, "flags", feature2));

    LD_ASSERT(LDStoreGet(store, "segments", "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(lookup, feature1));
    LDJSONFree(lookup);

    LD_ASSERT(LDStoreGet(store, "flags", "my-heap-key", &lookup));
    LD_ASSERT(lookup);
    LD_ASSERT(LDJSONCompare(lookup, feature2));
    LDJSONFree(lookup);

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
