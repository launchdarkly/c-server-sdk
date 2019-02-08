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

/*
static struct LDJSON *
constructSegment(const char *const key, const unsigned int version)
{
    struct LDJSON *segment = NULL, *tmp = NULL;

    LD_ASSERT(key);

    LD_ASSERT(segment = LDNewObject());

    LD_ASSERT(tmp = LDNewArray());
    LD_ASSERT(LDObjectSetKey(segment, "included", tmp));

    LD_ASSERT(tmp = LDNewArray())
    LD_ASSERT(LDObjectSetKey(segment, "excluded", tmp));

    LD_ASSERT(tmp = LDNewArray())
    LD_ASSERT(LDObjectSetKey(segment, "rules", tmp));

    LD_ASSERT(tmp = LDNewNumber(version));
    LD_ASSERT(LDObjectSetKey(segment, "version", tmp));

    LD_ASSERT(tmp = LDNewBool(false));
    LD_ASSERT(LDObjectSetKey(segment, "deleted", tmp));

    return segment;
}
*/

/*
static struct Segment *
constructSegment(const char *const key, const unsigned int version)
{
    struct Segment *segment = NULL;

    LD_ASSERT(key);

    if (!(segment = malloc(sizeof(struct Segment)))) {
        return NULL;
    }

    memset(segment, 0, sizeof(struct Segment));

    if (!(segment->key = strdup(key))) {
        free(segment);

        return NULL;
    }

    segment->included = NULL;
    segment->excluded = NULL;
    segment->salt     = NULL;
    segment->rules    = NULL;
    segment->version  = version;
    segment->deleted  = false;

    return segment;
}

static struct FeatureFlag *
constructFlag(const char *const key, const unsigned int version)
{
    struct FeatureFlag *flag = NULL;

    LD_ASSERT(key);

    if (!(flag = malloc(sizeof(struct FeatureFlag)))) {
        return NULL;
    }

    memset(flag, 0, sizeof(struct FeatureFlag));

    if (!(flag->key = strdup(key))) {
        free(flag);

        return NULL;
    }

    flag->version                 = version;
    flag->on                      = true;
    flag->trackEvents             = true;
    flag->deleted                 = false;
    flag->prerequisites           = NULL;
    flag->salt                    = NULL;
    flag->sel                     = NULL;
    flag->targets                 = NULL;
    flag->rules                   = NULL;
    flag->fallthrough             = NULL;
    flag->offVariation            = 0;
    flag->hasOffVariation         = false;
    flag->variations              = NULL;
    flag->debugEventsUntilDate    = 0;
    flag->hasDebugEventsUntilDate = false;
    flag->clientSide              = false;

    return flag;
}
*/

static void
allocateAndFree()
{
    struct LDStore *const store = prepareEmptyStore();

    LDStoreDestroy(store);
}

/*
static void
deletedOnlySegment()
{
    struct LDFeatureStore *store = NULL;
    struct Segment *segment = NULL;
    struct LDVersionedData *versioned = NULL, *lookup = NULL;
    struct LDVersionedDataKind kind = getSegmentKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(segment = segmentMakeDeleted("abc", 123));

    LD_ASSERT(versioned = segmentToVersioned(segment));

    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT(!(lookup = store->get(store->context, "abc", kind)));

    store->finalizeGet(store->context, lookup);

    freeStore(store);
}

static void
basicExistsSegment()
{
    struct LDFeatureStore *store = NULL;
    struct Segment *segment = NULL;
    struct LDVersionedData *versioned = NULL, *lookup = NULL;
    struct LDVersionedDataKind kind = getSegmentKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(segment = constructSegment("my-heap-key", 3))

    LD_ASSERT(versioned = segmentToVersioned(segment));

    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT((lookup = store->get(store->context, "my-heap-key", kind)));
    LD_ASSERT(lookup->data == segment);

    store->finalizeGet(store->context, lookup);

    freeStore(store);
}

static void
basicDoesNotExist()
{
    struct LDFeatureStore *store = NULL;
    struct LDVersionedData *lookup = NULL;
    struct LDVersionedDataKind kind = getSegmentKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(!(lookup = store->get(store->context, "abc", kind)));

    store->finalizeGet(store->context, lookup);

    freeStore(store);
}

static void
upsertNewer()
{
    struct LDFeatureStore *store = NULL;
    struct Segment *segment;
    struct LDVersionedData *versioned = NULL, *lookup = NULL;
    struct LDVersionedDataKind kind = getSegmentKind();

    LD_ASSERT(store = prepareEmptyStore());

    LD_ASSERT(segment = constructSegment("my-heap-key", 3))
    LD_ASSERT(versioned = segmentToVersioned(segment));
    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT(segment = constructSegment("my-heap-key", 5))
    LD_ASSERT(versioned = segmentToVersioned(segment));
    LD_ASSERT(store->upsert(store->context, kind, versioned));

    LD_ASSERT((lookup = store->get(store->context, "my-heap-key", kind)));
    LD_ASSERT(lookup->data == segment);

    store->finalizeGet(store->context, lookup);

    freeStore(store);
}

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

    freeStore(store);
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

    freeStore(store);
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

    freeStore(store);
}
*/

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    allocateAndFree();

    /*
    deletedOnlySegment();
    basicExistsSegment();
    basicDoesNotExist();
    upsertNewer();
    upsertOlder();
    upsertDelete();
    conflictDifferentNamespace();
    */

    return 0;
}
