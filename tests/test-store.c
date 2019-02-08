#include "ldinternal.h"
#include "ldstore.h"

static struct LDStore *
prepareEmptyStore()
{
    struct LDStore *store = NULL; struct LDJSON *sets = NULL, *tmp = NULL;

    LD_ASSERT(store = makeInMemoryStore());
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
    struct LDJSON *feature = NULL, *tmp = NULL;

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
    struct LDJSON *feature = NULL, *tmp = NULL;

    LD_ASSERT(feature = makeVersioned(key, version));

    LD_ASSERT(tmp = LDNewBool(true));
    LD_ASSERT(LDObjectSetKey(feature, "deleted", tmp));

    return feature;
}

static void
deletedOnly()
{
    struct LDStore *store = NULL; struct LDJSON *feature = NULL, *lookup = NULL;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeDeleted("abc", 123));

    LD_ASSERT(LDStoreUpsert(store, "flags", feature));

    LD_ASSERT(!(lookup = LDStoreGet(store, "abc", "flags")));

    LDStoreDestroy(store);
}

static void
basicExists()
{
    struct LDStore *store = NULL; struct LDJSON *feature = NULL, *lookup = NULL;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))

    LD_ASSERT(LDStoreUpsert(store, "flags", feature));

    LD_ASSERT((lookup = LDStoreGet(store, "my-heap-key", "flags")));

    /* LD_ASSERT(lookup->data == segment); NEED compare */

    LDJSONFree(lookup);

    LDStoreDestroy(store);
}

static void
basicDoesNotExist()
{
    struct LDStore *store = NULL; struct LDJSON *lookup = NULL;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(!(lookup = LDStoreGet(store, "abc", "flags")));

    LDStoreDestroy(store);
}

static void
upsertNewer()
{
    struct LDStore *store = NULL; struct LDJSON *feature = NULL, *lookup = NULL;

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(feature = makeVersioned("my-heap-key", 3))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature));

    LD_ASSERT(feature = makeVersioned("my-heap-key", 5))
    LD_ASSERT(LDStoreUpsert(store, "segments", feature));

    LD_ASSERT((lookup = LDStoreGet(store, "my-heap-key", "segments")));

    /* LD_ASSERT(lookup->data == segment); requires deep compare */

    LDJSONFree(lookup);

    LDStoreDestroy(store);
}
/*
static void
upsertOlder()
{
    struct LDFeatureStore *store = NULL;
    struct Segment *segment1 = NULL, *segment2 = NULL;
    struct LDVersionedData *versioned = NULL, *lookup = NULL;
    struct LDVersionedDataKind kind = getSegmentKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(segment1 = constructSegment("my-heap-key", 5))
    LD_ASSERT(versioned = segmentToVersioned(segment1));
    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT(segment2 = constructSegment("my-heap-key", 3))
    LD_ASSERT(versioned = segmentToVersioned(segment2));
    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT((lookup = store->get(store->context, "my-heap-key", kind)));
    LD_ASSERT(lookup->data == segment1);
    store->finalizeGet(store->context, lookup);

    LDStoreDestroy(store);
}

static void
upsertDelete()
{
    struct LDFeatureStore *store = NULL;
    struct Segment *segment1 = NULL, *segment2 = NULL;
    struct LDVersionedData *versioned = NULL, *lookup = NULL;
    struct LDVersionedDataKind kind = getSegmentKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(segment1 = constructSegment("my-heap-key", 8))
    LD_ASSERT(versioned = segmentToVersioned(segment1));
    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT(segment2 = segmentMakeDeleted("my-heap-key", 10))
    LD_ASSERT(versioned = segmentToVersioned(segment2));
    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT(!(lookup = store->get(store->context, "my-heap-key", kind)));
    store->finalizeGet(store->context, lookup);

    LDStoreDestroy(store);
}

static void
conflictDifferentNamespace()
{
    struct LDFeatureStore *store = NULL;
    struct Segment *segment = NULL;
    struct FeatureFlag *flag = NULL;
    struct LDVersionedData *versioned = NULL, *lookup = NULL;
    struct LDVersionedDataKind segmentKind = getSegmentKind(), flagKind = getFlagKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(segment = constructSegment("my-heap-key", 3));
    LD_ASSERT(versioned = segmentToVersioned(segment));
    LD_ASSERT(store->upsert(store->context, segmentKind, versioned));

    LD_ASSERT(flag = constructFlag("my-heap-key", 3));
    LD_ASSERT(versioned = flagToVersioned(flag));
    LD_ASSERT(store->upsert(store->context, flagKind, versioned));

    LD_ASSERT((lookup = store->get(store->context, "my-heap-key", segmentKind)));
    LD_ASSERT(lookup->data == segment);
    store->finalizeGet(store->context, lookup);

    LD_ASSERT((lookup = store->get(store->context, "my-heap-key", flagKind)));
    LD_ASSERT(lookup->data == flag);
    store->finalizeGet(store->context, lookup);

    LDStoreDestroy(store);
}
*/

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    allocateAndFree();
    deletedOnly();
    basicExists();
    basicDoesNotExist();
    upsertNewer();

    /*
    upsertOlder();
    upsertDelete();
    conflictDifferentNamespace();
    */

    return 0;
}
