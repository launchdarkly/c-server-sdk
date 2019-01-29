#include "ldinternal.h"
#include "ldstore.h"
#include "ldschema.h"

static struct LDFeatureStore *
prepareEmptyStore()
{
    struct LDVersionedSet *sets = NULL;
    struct LDFeatureStore *store = NULL;

    const char *namespace = NULL;
    struct LDVersionedSet *set = NULL;

    LD_ASSERT(store = makeInMemoryStore());
    LD_ASSERT(!store->initialized(store->context));

    LD_ASSERT(set = malloc(sizeof(struct LDVersionedSet)));
    set->kind = getSegmentKind();
    set->elements = NULL;
    LD_ASSERT(namespace = set->kind.getNamespace());
    HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), set);

    set = malloc(sizeof(struct LDVersionedSet));
    LD_ASSERT(set);
    set->kind = getFlagKind();
    set->elements = NULL;
    LD_ASSERT(namespace = set->kind.getNamespace());
    HASH_ADD_KEYPTR(hh, sets, namespace, strlen(namespace), set);

    LD_ASSERT(store->init(store->context, sets));
    LD_ASSERT(store->initialized(store->context));

    return store;
}

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

static void
allocateAndFree()
{
    struct LDFeatureStore *const store = prepareEmptyStore();

    freeStore(store);
}

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

int
main()
{
    LDConfigureGlobalLogger(LD_LOG_TRACE, LDBasicLogger);

    allocateAndFree();
    deletedOnlySegment();
    basicExistsSegment();
    upsertNewer();
    upsertOlder();
    upsertDelete();

    return 0;
}
